"""
depth_distortion_filter.py
==========================
Filtro visual de distorsión dinámica basado en profundidad estéreo.

Descripción académica:
    Este módulo implementa un efecto visual experimental que combina la medición
    de profundidad estereoscópica (visión binocular) con distorsión radial adaptativa.
    A mayor distancia del sujeto respecto al sistema de cámaras, mayor es la
    deformación barrel aplicada sobre la imagen rectificada izquierda.

Fundamento matemático:
    La distorsión barrel se modela mediante distorsión radial polinómica de primer orden:
        r_d = r * (1 + k * r²)
    donde:
        r   = distancia normalizada del píxel al centro de la imagen  [-1, 1]
        k   = coeficiente de distorsión  (función de la profundidad Z medida)
        r_d = radio distorsionado resultante

    El coeficiente k se mapea linealmente desde la profundidad medida:
        k = k_max * clamp((Z - Z_min) / (Z_max - Z_min), 0, 1)

Flujo de procesamiento:
    1. Captura simultánea del par estéreo (cámara izquierda / derecha)
    2. Rectificación de ambas imágenes con los parámetros de calibración
    3. Cálculo del mapa de disparidad (SGBM) y reproyección a 3D
    4. Estimación de la profundidad media de la escena (mediana de Z válidas)
    5. Generación del mapa de remapeo con distorsión radial proporcional a Z
    6. Aplicación del filtro sobre la imagen izquierda rectificada
    7. Visualización con información de profundidad y nivel de efecto superpuesto

Autor  : Michael Lata
Materia: Visión por Computador — Proyecto Integrador
"""

import argparse
import json
import math
from concurrent.futures import ThreadPoolExecutor

import cv2
import numpy as np
import requests


# ---------------------------------------------------------------------------
# Utilidades de red y configuración
# ---------------------------------------------------------------------------

def build_url(base: str, path: str) -> str:
    """Construye una URL completa uniendo base y path de forma segura."""
    base = base.rstrip('/')
    path = path if path.startswith('/') else '/' + path
    return base + path


def load_config(cfg_path: str):
    """
    Carga cameras.json y retorna las URLs de snapshot de las dos cámaras.

    Retorna:
        url_izq  -- URL del endpoint /capture de la cámara izquierda
        url_der  -- URL del endpoint /capture de la cámara derecha
    """
    with open(cfg_path, 'r', encoding='utf-8') as f:
        cfg = json.load(f)

    cams = cfg.get('cameras', [])
    if len(cams) < 2:
        raise RuntimeError('Se requieren al menos 2 cámaras en cameras.json')

    c1, c2 = cams[0], cams[1]
    url_izq = build_url(c1['base_url'], c1.get('snapshot_path', '/capture'))
    url_der = build_url(c2['base_url'], c2.get('snapshot_path', '/capture'))
    return url_izq, url_der


def fetch_frame(url: str, timeout: float = 3.0) -> np.ndarray:
    """
    Descarga una imagen JPEG desde el ESP32-CAM y la decodifica en BGR.

    Lanza RuntimeError si la respuesta HTTP no es válida o el JPEG no se puede
    decodificar.
    """
    r = requests.get(url, timeout=timeout)
    r.raise_for_status()
    arr = np.frombuffer(r.content, dtype=np.uint8)
    img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if img is None:
        raise RuntimeError(f'No se pudo decodificar JPEG desde {url}')
    return img


# ---------------------------------------------------------------------------
# Carga de calibración y rectificación
# ---------------------------------------------------------------------------

def load_calib(path: str):
    """
    Lee el archivo de calibración estéreo (.yml generado por OpenCV) y
    precalcula los mapas de rectificación para ambas cámaras.

    Los mapas map1x/map1y y map2x/map2y son los argumentos de cv2.remap que
    transforman cada imagen raw a su versión rectificada, eliminando distorsión
    de lente y alineando las líneas epipolares.

    Retorna:
        (w, h)            -- resolución nominal de calibración
        map1x, map1y      -- mapas rectificación cámara izquierda
        map2x, map2y      -- mapas rectificación cámara derecha
        Q                 -- matriz de reproyección 4×4 (disparidad → 3D)
    """
    fs = cv2.FileStorage(path, cv2.FILE_STORAGE_READ)
    if not fs.isOpened():
        raise RuntimeError(f'No se pudo abrir el archivo de calibración: {path}')

    w = int(fs.getNode('image_width').real())
    h = int(fs.getNode('image_height').real())
    K1 = fs.getNode('K1').mat()
    D1 = fs.getNode('D1').mat()
    K2 = fs.getNode('K2').mat()
    D2 = fs.getNode('D2').mat()
    R1 = fs.getNode('R1').mat()
    R2 = fs.getNode('R2').mat()
    P1 = fs.getNode('P1').mat()
    P2 = fs.getNode('P2').mat()
    Q  = fs.getNode('Q').mat()
    fs.release()

    map1x, map1y = cv2.initUndistortRectifyMap(K1, D1, R1, P1, (w, h), cv2.CV_32FC1)
    map2x, map2y = cv2.initUndistortRectifyMap(K2, D2, R2, P2, (w, h), cv2.CV_32FC1)

    return (w, h), map1x, map1y, map2x, map2y, Q


# ---------------------------------------------------------------------------
# Motor de distorsión dinámica
# ---------------------------------------------------------------------------

def build_distortion_maps(w: int, h: int, k: float):
    """
    Genera los mapas de remapeo (map_x, map_y) para aplicar distorsión barrel
    radial con coeficiente k sobre una imagen de tamaño (w, h).

    Modelo:
        Sea (cx, cy) el centro de la imagen y (xn, yn) las coordenadas
        normalizadas de un píxel de destino en el rango [-1, 1]:

            xn = (x_dst - cx) / cx
            yn = (y_dst - cy) / cy

        El radio normalizado es  r² = xn² + yn².
        La distorsión barrel aplica el factor (1 + k·r²) sobre el radio:

            x_src = cx + xn · (1 + k·r²) · cx
            y_src = cy + yn · (1 + k·r²) · cy

        El resultado se interpreta como la posición de origen en la imagen
        fuente que debe mapearse al píxel de destino (x_dst, y_dst).

    Parámetros:
        w, h  -- dimensiones de la imagen
        k     -- coeficiente de distorsión (0.0 = sin efecto, >0 = barrel)

    Retorna:
        map_x, map_y -- arreglos float32 usados directamente en cv2.remap
    """
    cx, cy = w / 2.0, h / 2.0

    # Grilla de coordenadas de destino
    xs = np.arange(w, dtype=np.float32)
    ys = np.arange(h, dtype=np.float32)
    x_dst, y_dst = np.meshgrid(xs, ys)  # shape (h, w)

    # Coordenadas normalizadas respecto al centro, en [-1, 1]
    xn = (x_dst - cx) / cx
    yn = (y_dst - cy) / cy

    # Radio al cuadrado normalizado
    r2 = xn ** 2 + yn ** 2

    # Factor de distorsión radial: a mayor r² y mayor k, más desplazamiento
    factor = 1.0 + k * r2

    # Coordenadas de origen en la imagen fuente
    map_x = (cx + xn * factor * cx).astype(np.float32)
    map_y = (cy + yn * factor * cy).astype(np.float32)

    return map_x, map_y


def depth_to_k(depth_m: float, z_min: float, z_max: float, k_max: float) -> float:
    """
    Mapea una profundidad medida (en metros) a un coeficiente de distorsión k.

    La relación es lineal y acotada:
        - A distancias menores que z_min → k = 0  (sin efecto)
        - A distancias mayores que z_max → k = k_max  (efecto máximo)
        - Entre z_min y z_max → interpolación lineal

    Parámetros:
        depth_m  -- profundidad media de la escena en metros
        z_min    -- umbral mínimo de distancia para activar el efecto
        z_max    -- distancia a partir de la cual se alcanza el efecto máximo
        k_max    -- coeficiente máximo de distorsión barrel

    Retorna:
        k  -- coeficiente de distorsión en el rango [0, k_max]
    """
    t = (depth_m - z_min) / max(z_max - z_min, 1e-6)  # normalizado [0, 1]
    t = max(0.0, min(1.0, t))                           # clamping
    return k_max * t


def median_scene_depth(points3d: np.ndarray) -> float:
    """
    Estima la profundidad media de la escena como la mediana de los valores Z
    válidos del mapa 3D generado por cv2.reprojectImageTo3D.

    Se filtran valores no finitos y profundidades fuera del rango plausible
    (0.05 m – 10 m) para evitar artefactos de SGBM.

    Retorna:
        Mediana de Z en metros, o None si no hay puntos válidos.
    """
    Z = points3d[:, :, 2].flatten()
    mask = np.isfinite(Z) & (Z > 0.05) & (Z < 10.0)
    valid = Z[mask]
    if len(valid) < 50:
        return None
    return float(np.median(valid))


# ---------------------------------------------------------------------------
# Renderizado de información en pantalla (HUD)
# ---------------------------------------------------------------------------

def draw_hud(img: np.ndarray, depth_m, k: float, z_min: float, z_max: float):
    """
    Dibuja un HUD (Head-Up Display) sobre la imagen con la información de:
      - Profundidad media estimada de la escena
      - Coeficiente de distorsión activo
      - Barra de progreso del efecto (de z_min a z_max)

    Parámetros:
        img     -- imagen BGR sobre la que se dibuja (se modifica in-place)
        depth_m -- profundidad en metros (None si no disponible)
        k       -- coeficiente de distorsión actual
        z_min   -- distancia mínima del rango de efecto
        z_max   -- distancia máxima del rango de efecto
    """
    h, w = img.shape[:2]
    overlay = img.copy()

    # Fondo semitransparente para el panel de texto
    cv2.rectangle(overlay, (8, 8), (310, 90), (0, 0, 0), -1)
    cv2.addWeighted(overlay, 0.5, img, 0.5, 0, img)

    # Texto de profundidad
    if depth_m is not None:
        depth_txt = f'Profundidad: {depth_m:.2f} m'
    else:
        depth_txt = 'Profundidad: calculando...'
    cv2.putText(img, depth_txt, (14, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 200), 1, cv2.LINE_AA)

    # Texto del coeficiente k activo
    cv2.putText(img, f'Distorsion barrel k: {k:.3f}', (14, 52),
                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 200, 255), 1, cv2.LINE_AA)

    # Barra de progreso del efecto (de z_min a z_max)
    bx, by, bw_bar, bh_bar = 14, 64, 280, 12
    cv2.rectangle(img, (bx, by), (bx + bw_bar, by + bh_bar), (60, 60, 60), -1)
    if depth_m is not None:
        fill_ratio = max(0.0, min(1.0, (depth_m - z_min) / max(z_max - z_min, 1e-6)))
        fill_px = int(bw_bar * fill_ratio)
        # Color gradiente: verde (cerca) → rojo (lejos)
        b_ch = 0
        g_ch = int(255 * (1 - fill_ratio))
        r_ch = int(255 * fill_ratio)
        cv2.rectangle(img, (bx, by), (bx + fill_px, by + bh_bar), (b_ch, g_ch, r_ch), -1)
    cv2.rectangle(img, (bx, by), (bx + bw_bar, by + bh_bar), (120, 120, 120), 1)

    # Etiquetas del rango en la barra
    cv2.putText(img, f'{z_min}m', (bx, by + bh_bar + 12),
                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (180, 180, 180), 1)
    cv2.putText(img, f'{z_max}m', (bx + bw_bar - 24, by + bh_bar + 12),
                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (180, 180, 180), 1)


# ---------------------------------------------------------------------------
# Bucle principal
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description='Filtro visual de distorsión barrel proporcional a la profundidad estéreo.'
    )
    ap.add_argument('--calib',   default='../config/stereo_calib.yml',
                    help='Ruta al archivo de calibración estéreo (.yml)')
    ap.add_argument('--config',  default='../config/cameras.json',
                    help='Ruta a cameras.json con URLs de las cámaras')
    ap.add_argument('--timeout', type=float, default=3.0,
                    help='Tiempo máximo de espera por snapshot (segundos)')
    ap.add_argument('--z-min',   type=float, default=0.3,
                    help='Distancia mínima para activar el efecto (metros)')
    ap.add_argument('--z-max',   type=float, default=2.5,
                    help='Distancia a la que se alcanza el efecto máximo (metros)')
    ap.add_argument('--k-max',   type=float, default=1.8,
                    help='Coeficiente máximo de distorsión barrel (recomendado 0.5–2.5)')
    args = ap.parse_args()

    # --- Carga de calibración ---
    print('[INFO] Cargando calibración estéreo...')
    (w, h), map1x, map1y, map2x, map2y, Q = load_calib(args.calib)
    print(f'[INFO] Resolución de calibración: {w}×{h}')

    # --- Configuración de cámaras ---
    url_izq, url_der = load_config(args.config)
    print(f'[INFO] Cámara izquierda : {url_izq}')
    print(f'[INFO] Cámara derecha   : {url_der}')
    print(f'[INFO] Rango de efecto  : {args.z_min} m → {args.z_max} m (k_max={args.k_max})')
    print('[INFO] Presiona Q o ESC para salir.')

    # --- Configuración del algoritmo SGBM ---
    # StereoSGBM (Semi-Global Block Matching) genera el mapa de disparidad.
    # Los parámetros P1 y P2 controlan la suavidad de la superficie de disparidad.
    sgbm = cv2.StereoSGBM_create(
        minDisparity=0,
        numDisparities=128,   # múltiplo de 16; mayor valor = más rango de profundidad
        blockSize=5           # tamaño del bloque de correlación (píxeles)
    )
    sgbm.setP1(8  * 3 * 5 * 5)   # penalización por cambio de disparidad de 1
    sgbm.setP2(32 * 3 * 5 * 5)   # penalización por cambio de disparidad > 1
    sgbm.setMode(cv2.STEREO_SGBM_MODE_SGBM_3WAY)

    # Estado persistente entre frames
    depth_m = None   # profundidad media de la escena (metros)
    k_actual = 0.0   # coeficiente de distorsión activo en este frame

    # Precalcular mapas de distorsión nula (k=0) como punto de partida
    map_dx, map_dy = build_distortion_maps(w, h, 0.0)

    cv2.namedWindow('Filtro Distorsion Barrel', cv2.WINDOW_NORMAL)
    cv2.namedWindow('Mapa de Disparidad',       cv2.WINDOW_NORMAL)

    while True:
        # --- Captura simultánea del par estéreo ---
        try:
            with ThreadPoolExecutor(max_workers=2) as ex:
                fut_izq = ex.submit(fetch_frame, url_izq, args.timeout)
                fut_der = ex.submit(fetch_frame, url_der, args.timeout)
                raw_izq = fut_izq.result()
                raw_der = fut_der.result()
        except Exception as e:
            print(f'[ERROR] No se pudo capturar el par estéreo: {e}')
            if cv2.waitKey(300) in (27, ord('q')):
                break
            continue

        # --- Redimensión si es necesario ---
        if raw_izq.shape[1] != w or raw_izq.shape[0] != h:
            raw_izq = cv2.resize(raw_izq, (w, h))
        if raw_der.shape[1] != w or raw_der.shape[0] != h:
            raw_der = cv2.resize(raw_der, (w, h))

        # --- Rectificación de las imágenes ---
        # cv2.remap elimina la distorsión de lente y alinea las filas epipolares,
        # condición necesaria para que SGBM funcione correctamente.
        rect_izq = cv2.remap(raw_izq, map1x, map1y, cv2.INTER_LINEAR)
        rect_der = cv2.remap(raw_der, map2x, map2y, cv2.INTER_LINEAR)

        # --- Cálculo del mapa de disparidad ---
        gray_izq = cv2.cvtColor(rect_izq, cv2.COLOR_BGR2GRAY)
        gray_der = cv2.cvtColor(rect_der, cv2.COLOR_BGR2GRAY)

        # Suavizado Gaussiano para reducir ruido de alta frecuencia antes de SGBM
        gray_izq = cv2.GaussianBlur(gray_izq, (5, 5), 0)
        gray_der = cv2.GaussianBlur(gray_der, (5, 5), 0)

        # SGBM retorna disparidad en formato Q16.4 (hay que dividir entre 16)
        disp16 = sgbm.compute(gray_izq, gray_der)
        disp32 = disp16.astype(np.float32) / 16.0

        # Invalidar disparidades no confiables (≤ 0 son artefactos de SGBM)
        disp32[disp32 <= 0.5] = np.nan

        # --- Reproyección a coordenadas 3D ---
        # Q transforma (x, y, disparidad) → (X, Y, Z) en metros.
        points3d = cv2.reprojectImageTo3D(
            np.nan_to_num(disp32, nan=0.0), Q, handleMissingValues=True
        )

        # --- Estimación de profundidad media de la escena ---
        depth_m = median_scene_depth(points3d)

        # --- Cálculo del coeficiente de distorsión para este frame ---
        if depth_m is not None:
            k_actual = depth_to_k(depth_m, args.z_min, args.z_max, args.k_max)
            # Recalcular mapas de remapeo solo si k cambió significativamente
            map_dx, map_dy = build_distortion_maps(w, h, k_actual)

        # --- Aplicación del filtro de distorsión barrel ---
        # cv2.remap interpola la imagen fuente con los mapas generados.
        # BORDER_REFLECT_101 rellena los bordes con reflexión para evitar bordes negros.
        img_filtrada = cv2.remap(
            rect_izq, map_dx, map_dy,
            interpolation=cv2.INTER_LINEAR,
            borderMode=cv2.BORDER_REFLECT_101
        )

        # --- HUD sobre la imagen filtrada ---
        draw_hud(img_filtrada, depth_m, k_actual, args.z_min, args.z_max)

        # --- Visualización del mapa de disparidad (coloreado) ---
        disp_vis = cv2.normalize(disp32, None, 0, 255, cv2.NORM_MINMAX)
        disp_vis = np.nan_to_num(disp_vis, nan=0).astype(np.uint8)
        disp_color = cv2.applyColorMap(disp_vis, cv2.COLORMAP_TURBO)

        cv2.imshow('Filtro Distorsion Barrel', img_filtrada)
        cv2.imshow('Mapa de Disparidad',       disp_color)

        # Salir con Q o ESC
        k_key = cv2.waitKey(1) & 0xFF
        if k_key in (27, ord('q')):
            break

    cv2.destroyAllWindows()
    print('[INFO] Filtro finalizado.')


if __name__ == '__main__':
    main()
