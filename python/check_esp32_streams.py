import argparse
import json
import os
import time
from datetime import datetime

import cv2
import requests


def load_config(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def build_url(base, path):
    base = base.rstrip('/')
    path = path if path.startswith('/') else '/' + path
    return base + path


def check_snapshot(cam, out_dir):
    snap_path = cam.get('snapshot_path', '/capture')
    if not snap_path:
        return None, 'snapshot_skipped'

    snap_url = build_url(cam['base_url'], snap_path)
    name = cam['name']
    try:
        r = requests.get(snap_url, timeout=6)
        if r.status_code != 200:
            return False, f'snapshot_http_{r.status_code}'
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        out = os.path.join(out_dir, f'{name}_{ts}.jpg')
        with open(out, 'wb') as f:
            f.write(r.content)
        return True, out
    except Exception as e:
        return False, str(e)


def check_stream(cam, seconds=4):
    stream_url = build_url(cam['base_url'], cam.get('stream_path', '/stream'))

    # Intento 1: backend FFMPEG explícito
    cap = cv2.VideoCapture(stream_url, cv2.CAP_FFMPEG)
    if not cap.isOpened():
        # Intento 2: backend por defecto
        cap = cv2.VideoCapture(stream_url)

    if not cap.isOpened():
        return False, 'stream_not_opened'

    t0 = time.time()
    frames = 0
    ok = True

    while time.time() - t0 < seconds:
        ret, frame = cap.read()
        if not ret or frame is None:
            ok = False
            break
        frames += 1

    cap.release()
    if not ok:
        return False, f'stream_read_fail_frames={frames}'
    return True, f'stream_ok_frames={frames}'


def main():
    p = argparse.ArgumentParser(description='Check ESP32-CAM streams and snapshots')
    p.add_argument('--config', default='../config/cameras.json', help='Path to cameras.json')
    p.add_argument('--out', default='../data/output', help='Output directory for snapshots')
    p.add_argument('--seconds', type=int, default=4, help='Seconds to test each stream')
    args = p.parse_args()

    cfg = load_config(args.config)
    cams = cfg.get('cameras', [])

    if not cams:
        print('No cameras found in config')
        return

    os.makedirs(args.out, exist_ok=True)

    print('=== ESP32 CAMERA CHECK ===')
    for cam in cams:
        print(f"\n[{cam['name']}] base={cam['base_url']}")

        s_ok, s_msg = check_snapshot(cam, args.out)
        if s_ok is None:
            print('snapshot: SKIP |', s_msg)
        else:
            print('snapshot:', 'OK' if s_ok else 'FAIL', '|', s_msg)

        v_ok, v_msg = check_stream(cam, args.seconds)
        print('stream  :', 'OK' if v_ok else 'FAIL', '|', v_msg)

        if (s_ok is True) and (v_ok is False):
            print('nota    : OpenCV no abre MJPEG en este entorno; usa capture_stereo_pairs.py (modo snapshot paralelo).')


if __name__ == '__main__':
    main()
