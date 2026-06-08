"""
collect_measurements.py — Recolecta mediciones Z a distancias reales conocidas.
Conecta al sistema estéreo compilado, toma N muestras a cada distancia objetivo
y guarda los resultados en data/measurements/validation_TIMESTAMP.csv

Uso:
    python collect_measurements.py <IP_IZQ> <IP_DER> [--samples 50]

Ejemplo:
    python collect_measurements.py 192.168.0.102 192.168.0.105

Protocolo:
    El script lanza ./stereo_measure en background (modo headless no disponible),
    así que este script conecta directamente a los ESP32-CAM via HTTP, captura
    pares de imágenes, aplica la calibración y calcula Z con cv2.reprojectImageTo3D.
    Esto permite correr sin pantalla y guardar los datos automáticamente.
"""

import argparse
import csv
import os
import sys
import time
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np
import requests

# Ruta base del proyecto
PROJECT_ROOT = Path(__file__).parent.parent
CALIB_FILE   = PROJECT_ROOT / "config" / "stereo_calib.yml"
OUTPUT_DIR   = PROJECT_ROOT / "data" / "measurements"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

DISTANCES_CM = [30, 60, 90]   # distancias de validación requeridas por rúbrica
SAMPLES      = 50              # muestras por distancia


# ---------------------------------------------------------------------------
# Calibración estéreo
# ---------------------------------------------------------------------------
def load_calibration(path: Path):
    fs = cv2.FileStorage(str(path), cv2.FILE_STORAGE_READ)
    if not fs.isOpened():
        raise FileNotFoundError(f"No se pudo abrir calibración: {path}")

    K1 = fs.getNode("K1").mat()
    D1 = fs.getNode("D1").mat()
    K2 = fs.getNode("K2").mat()
    D2 = fs.getNode("D2").mat()
    R1 = fs.getNode("R1").mat()
    R2 = fs.getNode("R2").mat()
    P1 = fs.getNode("P1").mat()
    P2 = fs.getNode("P2").mat()
    Q  = fs.getNode("Q").mat()
    width  = int(fs.getNode("image_width").real())
    height = int(fs.getNode("image_height").real())
    fs.release()
    return K1, D1, K2, D2, R1, R2, P1, P2, Q, (width, height)


# ---------------------------------------------------------------------------
# Captura de frame desde ESP32-CAM
# ---------------------------------------------------------------------------
def capture_frame(ip: str, timeout: float = 3.0) -> np.ndarray | None:
    url = f"http://{ip}/capture"
    try:
        r = requests.get(url, timeout=timeout)
        if r.status_code != 200:
            return None
        arr = np.frombuffer(r.content, dtype=np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
        return img
    except Exception as e:
        print(f"  [!] {ip}: {e}")
        return None


# ---------------------------------------------------------------------------
# Pipeline estéreo (simplificado para medición)
# ---------------------------------------------------------------------------
def compute_depth(left_raw: np.ndarray, right_raw: np.ndarray,
                  K1, D1, K2, D2, R1, R2, P1, P2, Q, size) -> float:
    # Redimensionar a la resolución de calibración (320×240) para usar
    # directamente K, P, Q sin reescalar. Más simple y robusto.
    left_raw  = cv2.resize(left_raw,  size)
    right_raw = cv2.resize(right_raw, size)

    map1L, map2L = cv2.initUndistortRectifyMap(K1, D1, R1, P1, size, cv2.CV_32FC1)
    map1R, map2R = cv2.initUndistortRectifyMap(K2, D2, R2, P2, size, cv2.CV_32FC1)

    left  = cv2.remap(left_raw,  map1L, map2L, cv2.INTER_LINEAR)
    right = cv2.remap(right_raw, map1R, map2R, cv2.INTER_LINEAR)

    # CLAHE
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    gL = clahe.apply(cv2.cvtColor(left,  cv2.COLOR_BGR2GRAY))
    gR = clahe.apply(cv2.cvtColor(right, cv2.COLOR_BGR2GRAY))

    # SGBM — numDisp=96 es suficiente para QVGA (f=328px, B=6.7cm):
    #   30cm → d=73px  60cm → d=37px  90cm → d=24px  (todos < 96)
    num_disp = 96
    block    = 5
    sgbm = cv2.StereoSGBM_create(
        minDisparity=0, numDisparities=num_disp, blockSize=block,
        P1=8*3*block**2, P2=32*3*block**2,
        disp12MaxDiff=1, uniquenessRatio=10,
        speckleWindowSize=100, speckleRange=32,
        preFilterCap=63,
        mode=cv2.STEREO_SGBM_MODE_SGBM_3WAY)

    disp = sgbm.compute(gL, gR).astype(np.float32) / 16.0

    # Reproyección 3D con Q original (ya en escala QVGA)
    pts3d = cv2.reprojectImageTo3D(disp, Q)

    # Z mediana en región central amplia (radio 15px sobre 320×240)
    cx, cy = size[0] // 2, size[1] // 2
    r = 15
    roi = pts3d[cy-r:cy+r, cx-r:cx+r, 2]
    valid = roi[(roi > 0.05) & (roi < 10.0)]
    if len(valid) == 0:
        return float('nan')
    return float(np.median(valid))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("left_ip",  help="IP de la cámara izquierda (ej. 192.168.0.102)")
    ap.add_argument("right_ip", help="IP de la cámara derecha (ej. 192.168.0.105)")
    ap.add_argument("--samples", type=int, default=SAMPLES)
    args = ap.parse_args()

    print(f"[*] Cargando calibración desde {CALIB_FILE}")
    K1, D1, K2, D2, R1, R2, P1, P2, Q, size = load_calibration(CALIB_FILE)
    print(f"    Tamaño calibrado: {size[0]}×{size[1]}")

    ts  = datetime.now().strftime("%Y%m%d_%H%M%S")
    out = OUTPUT_DIR / f"validation_{ts}.csv"

    rows = []
    errors_summary = {}

    for dist_cm in DISTANCES_CM:
        input(f"\n▶  Coloca el objeto a EXACTAMENTE {dist_cm} cm del arreglo. "
              f"Presiona ENTER para empezar a medir...")

        print(f"   Tomando {args.samples} muestras...")
        zs = []
        for i in range(args.samples):
            L = capture_frame(args.left_ip)
            R = capture_frame(args.right_ip)
            if L is None or R is None:
                print(f"   [!] Frame fallido en muestra {i+1}")
                continue
            z = compute_depth(L, R, K1, D1, K2, D2, R1, R2, P1, P2, Q, size)
            if not np.isnan(z):
                zs.append(z * 100)  # metros → cm
            time.sleep(0.05)
            print(f"   {i+1:3d}/{args.samples}  Z={zs[-1]:.2f} cm" if zs else
                  f"   {i+1:3d}/{args.samples}  Z=NaN", end='\r')

        print()
        if not zs:
            print(f"   [!] Sin mediciones válidas a {dist_cm} cm")
            continue

        z_arr = np.array(zs)
        # Filtrado IQR: eliminar outliers causados por frames corruptos o
        # errores de SGBM en zonas sin textura. Conserva el 75% central.
        q1, q3 = np.percentile(z_arr, 25), np.percentile(z_arr, 75)
        iqr     = q3 - q1
        z_arr   = z_arr[(z_arr >= q1 - 1.5*iqr) & (z_arr <= q3 + 1.5*iqr)]
        if len(z_arr) == 0:
            print(f"   [!] Sin mediciones tras filtro IQR a {dist_cm} cm")
            continue
        z_mean = np.mean(z_arr)
        z_std  = np.std(z_arr)
        z_err  = abs(z_mean - dist_cm)
        z_min  = np.min(z_arr)
        z_max  = np.max(z_arr)

        print(f"   Real: {dist_cm} cm | Media: {z_mean:.2f} cm | "
              f"Std: {z_std:.2f} cm | Error: {z_err:.2f} cm | "
              f"Rango: [{z_min:.2f}, {z_max:.2f}]")

        errors_summary[dist_cm] = {
            "n": len(zs), "mean": z_mean, "std": z_std,
            "error": z_err, "min": z_min, "max": z_max
        }

        for z in zs:
            rows.append({
                "target_cm":   dist_cm,
                "measured_cm": round(z, 3),
                "error_cm":    round(abs(z - dist_cm), 3),
            })

    # Guardar CSV
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["target_cm", "measured_cm", "error_cm"])
        w.writeheader()
        w.writerows(rows)

    print(f"\n[OK] Datos guardados en: {out}")
    print("\n=== RESUMEN ===")
    for d, s in errors_summary.items():
        print(f"  {d:3d} cm → media={s['mean']:.2f}  std={s['std']:.2f}  "
              f"error={s['error']:.2f}  (n={s['n']})")

    # Verificar rúbrica: fluctuación ≤ ±3 cm en entorno estático
    print("\n=== RÚBRICA (fluctuación ≤ ±3 cm) ===")
    for d, s in errors_summary.items():
        ok = "✓" if s['std'] <= 3.0 else "✗ REVISAR"
        print(f"  {d:3d} cm  std={s['std']:.2f} cm  [{ok}]")


if __name__ == "__main__":
    main()
