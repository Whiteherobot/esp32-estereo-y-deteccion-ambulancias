import argparse
import json
import os
import time
from datetime import datetime

import cv2
import numpy as np
import requests
from concurrent.futures import ThreadPoolExecutor


def build_url(base, path):
    base = base.rstrip('/')
    path = path if path.startswith('/') else '/' + path
    return base + path


def load_sources(config_path):
    with open(config_path, 'r', encoding='utf-8') as f:
        cfg = json.load(f)
    cams = cfg.get('cameras', [])
    if len(cams) < 2:
        raise ValueError('Se requieren al menos 2 cámaras en config/cameras.json')

    l = cams[0]
    r = cams[1]
    left_stream = build_url(l['base_url'], l.get('stream_path', '/stream'))
    right_stream = build_url(r['base_url'], r.get('stream_path', '/stream'))
    left_snap = build_url(l['base_url'], l.get('snapshot_path', '/capture'))
    right_snap = build_url(r['base_url'], r.get('snapshot_path', '/capture'))
    return left_stream, right_stream, left_snap, right_snap, l.get('name', 'left'), r.get('name', 'right')


def ensure_dirs(base):
    left_dir = os.path.join(base, 'left')
    right_dir = os.path.join(base, 'right')
    os.makedirs(left_dir, exist_ok=True)
    os.makedirs(right_dir, exist_ok=True)
    return left_dir, right_dir


def fetch_snapshot(url, timeout=3):
    r = requests.get(url, timeout=timeout)
    r.raise_for_status()
    arr = np.frombuffer(r.content, dtype=np.uint8)
    img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    return img


def detect_board(gray, board_size):
    ok, corners = cv2.findChessboardCornersSB(gray, board_size)
    if ok:
        return True, corners

    ok2, corners2 = cv2.findChessboardCorners(
        gray,
        board_size,
        cv2.CALIB_CB_ADAPTIVE_THRESH | cv2.CALIB_CB_NORMALIZE_IMAGE,
    )
    if ok2:
        cv2.cornerSubPix(
            gray,
            corners2,
            (11, 11),
            (-1, -1),
            (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_COUNT, 30, 0.01),
        )
        return True, corners2

    return False, None


def main():
    p = argparse.ArgumentParser(description='Captura pares estéreo sincronizados')
    p.add_argument('--config', default='../config/cameras.json', help='Ruta cameras.json')
    p.add_argument('--out', default='../data/calib', help='Carpeta base de salida (crea left/right)')
    p.add_argument('--width', type=int, default=640)
    p.add_argument('--height', type=int, default=480)
    p.add_argument('--auto-every', type=float, default=0.0, help='Auto captura cada N segundos (0 desactiva)')
    p.add_argument('--bw', type=int, default=9, help='Esquinas internas tablero (ancho)')
    p.add_argument('--bh', type=int, default=6, help='Esquinas internas tablero (alto)')
    p.add_argument('--require-board', action='store_true', default=True,
                   help='Guardar solo si detecta tablero en ambas cámaras (default: activo)')
    p.add_argument('--allow-any', action='store_true', help='Permite guardar aunque no detecte tablero')
    args = p.parse_args()

    board_size = (args.bw, args.bh)
    require_board = args.require_board and not args.allow_any

    left_stream, right_stream, left_snap, right_snap, left_name, right_name = load_sources(args.config)
    left_dir, right_dir = ensure_dirs(args.out)

    capL = cv2.VideoCapture(left_stream)
    capR = cv2.VideoCapture(right_stream)
    stream_mode = capL.isOpened() and capR.isOpened()

    if stream_mode:
        capL.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        capL.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        capR.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        capR.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    else:
        print('[INFO] Stream OpenCV no disponible. Usando modo snapshot (/capture) en paralelo.')

    print('=== CAPTURA ESTÉREO ===')
    print('Cam L stream:', left_stream)
    print('Cam R stream:', right_stream)
    print('Cam L snap  :', left_snap)
    print('Cam R snap  :', right_snap)
    print('Teclas:')
    print('  s  -> guardar par actual')
    print('  q/ESC -> salir')
    print(f'Tablero: {board_size[0]}x{board_size[1]} esquinas internas')
    print('Modo guardado:', 'solo con tablero en ambas' if require_board else 'sin validación de tablero')
    if args.auto_every and args.auto_every > 0:
        print(f'Auto captura activa cada {args.auto_every:.2f}s')

    saved = 0
    last_auto = time.time()

    while True:
        if stream_mode:
            okL, frameL = capL.read()
            okR, frameR = capR.read()
            if not okL or frameL is None or not okR or frameR is None:
                print('Advertencia: frame inválido, reintentando...')
                time.sleep(0.05)
                continue
        else:
            try:
                with ThreadPoolExecutor(max_workers=2) as ex:
                    f1 = ex.submit(fetch_snapshot, left_snap)
                    f2 = ex.submit(fetch_snapshot, right_snap)
                    frameL = f1.result()
                    frameR = f2.result()
                if frameL is None or frameR is None:
                    print('Advertencia: snapshot inválido, reintentando...')
                    time.sleep(0.05)
                    continue
            except Exception as e:
                print('Advertencia: error snapshot paralelo:', e)
                time.sleep(0.2)
                continue

        gL = cv2.cvtColor(frameL, cv2.COLOR_BGR2GRAY)
        gR = cv2.cvtColor(frameR, cv2.COLOR_BGR2GRAY)
        okL, cL = detect_board(gL, board_size)
        okR, cR = detect_board(gR, board_size)

        showL = frameL.copy()
        showR = frameR.copy()

        if okL:
            cv2.drawChessboardCorners(showL, board_size, cL, okL)
        if okR:
            cv2.drawChessboardCorners(showR, board_size, cR, okR)

        statusL = 'TABLERO OK' if okL else 'SIN TABLERO'
        statusR = 'TABLERO OK' if okR else 'SIN TABLERO'
        colL = (0, 255, 0) if okL else (0, 0, 255)
        colR = (0, 255, 0) if okR else (0, 0, 255)

        cv2.putText(showL, f'L: {left_name}', (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.putText(showR, f'R: {right_name}', (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.putText(showL, statusL, (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.6, colL, 2)
        cv2.putText(showR, statusR, (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.6, colR, 2)
        cv2.putText(showL, f'pares guardados: {saved}', (10, 75), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)
        cv2.putText(showR, f'pares guardados: {saved}', (10, 75), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)

        both = cv2.hconcat([showL, showR])
        cv2.imshow('Stereo Capture (L|R)', both)

        now = time.time()
        auto_trigger = args.auto_every > 0 and (now - last_auto) >= args.auto_every

        k = cv2.waitKey(1) & 0xFF
        manual_trigger = (k == ord('s'))

        if manual_trigger or auto_trigger:
            if require_board and not (okL and okR):
                print('[SKIP] no se guarda: tablero no detectado en ambas cámaras')
                last_auto = now
            else:
                ts = datetime.now().strftime('%Y%m%d_%H%M%S_%f')
                left_path = os.path.join(left_dir, f'{ts}.jpg')
                right_path = os.path.join(right_dir, f'{ts}.jpg')
                cv2.imwrite(left_path, frameL)
                cv2.imwrite(right_path, frameR)
                saved += 1
                print(f'[OK] par #{saved}: {left_path} | {right_path}')
                last_auto = now

        if k in (27, ord('q')):
            break

    if stream_mode:
        capL.release()
        capR.release()
    cv2.destroyAllWindows()
    print(f'Finalizado. pares guardados={saved}')


if __name__ == '__main__':
    main()
