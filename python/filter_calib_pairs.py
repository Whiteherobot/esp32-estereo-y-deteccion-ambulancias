import argparse
import csv
import glob
import json
import os
import shutil
from pathlib import Path

import cv2
import numpy as np


def board_area_ratio(corners, img_w, img_h):
    pts = corners.reshape(-1, 2)
    x0, y0 = pts.min(axis=0)
    x1, y1 = pts.max(axis=0)
    area = max(0.0, (x1 - x0) * (y1 - y0))
    return area / float(img_w * img_h)


def sharpness(gray):
    # Varianza del Laplaciano: más alto = más nítido
    return float(cv2.Laplacian(gray, cv2.CV_64F).var())


def detect_board(gray, board_size):
    # Intento robusto (OpenCV moderno)
    ok, corners = cv2.findChessboardCornersSB(gray, board_size)
    if ok:
        return True, corners

    # Fallback clásico
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
    p = argparse.ArgumentParser(description="Filtra pares estéreo válidos para calibración")
    p.add_argument("--left", default="../data/calib/left", help="Carpeta left")
    p.add_argument("--right", default="../data/calib/right", help="Carpeta right")
    p.add_argument("--out", default="../data/calib_filtered", help="Carpeta salida")
    p.add_argument("--bw", type=int, default=9, help="Corners internos ancho")
    p.add_argument("--bh", type=int, default=6, help="Corners internos alto")
    p.add_argument("--min-sharpness", type=float, default=80.0, help="Nitidez mínima (var Laplaciano)")
    p.add_argument("--min-area-ratio", type=float, default=0.06, help="Área mínima tablero / imagen")
    args = p.parse_args()

    board_size = (args.bw, args.bh)

    left_files = sorted(glob.glob(os.path.join(args.left, "*.jpg"))) + sorted(glob.glob(os.path.join(args.left, "*.png")))
    right_files = sorted(glob.glob(os.path.join(args.right, "*.jpg"))) + sorted(glob.glob(os.path.join(args.right, "*.png")))

    left_map = {Path(f).name: f for f in left_files}
    right_map = {Path(f).name: f for f in right_files}
    common = sorted(set(left_map).intersection(set(right_map)))

    out_left = Path(args.out) / "left"
    out_right = Path(args.out) / "right"
    out_left.mkdir(parents=True, exist_ok=True)
    out_right.mkdir(parents=True, exist_ok=True)

    report_csv = Path(args.out) / "filter_report.csv"
    report_json = Path(args.out) / "filter_report.json"

    results = []
    valid = 0

    for name in common:
        lf = left_map[name]
        rf = right_map[name]

        imgL = cv2.imread(lf)
        imgR = cv2.imread(rf)
        if imgL is None or imgR is None:
            results.append({"file": name, "status": "bad_image"})
            continue

        gL = cv2.cvtColor(imgL, cv2.COLOR_BGR2GRAY)
        gR = cv2.cvtColor(imgR, cv2.COLOR_BGR2GRAY)

        okL, cL = detect_board(gL, board_size)
        okR, cR = detect_board(gR, board_size)

        if not (okL and okR):
            results.append({"file": name, "status": "no_board", "okL": okL, "okR": okR})
            continue

        sharpL = sharpness(gL)
        sharpR = sharpness(gR)
        areaL = board_area_ratio(cL, gL.shape[1], gL.shape[0])
        areaR = board_area_ratio(cR, gR.shape[1], gR.shape[0])

        pass_sharp = sharpL >= args.min_sharpness and sharpR >= args.min_sharpness
        pass_area = areaL >= args.min_area_ratio and areaR >= args.min_area_ratio

        status = "valid" if (pass_sharp and pass_area) else "weak"

        if status == "valid":
            shutil.copy2(lf, out_left / name)
            shutil.copy2(rf, out_right / name)
            valid += 1

        results.append(
            {
                "file": name,
                "status": status,
                "sharpL": float(round(sharpL, 2)),
                "sharpR": float(round(sharpR, 2)),
                "areaL": float(round(areaL, 4)),
                "areaR": float(round(areaR, 4)),
                "pass_sharp": bool(pass_sharp),
                "pass_area": bool(pass_area),
            }
        )

    with open(report_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=sorted({k for r in results for k in r.keys()}))
        w.writeheader()
        for r in results:
            w.writerow(r)

    with open(report_json, "w", encoding="utf-8") as f:
        json.dump(
            {
                "total_common_pairs": len(common),
                "valid_pairs": valid,
                "params": {
                    "board_size": board_size,
                    "min_sharpness": args.min_sharpness,
                    "min_area_ratio": args.min_area_ratio,
                },
                "results": results,
            },
            f,
            ensure_ascii=False,
            indent=2,
        )

    print("=== FILTRO COMPLETADO ===")
    print(f"Pares comunes: {len(common)}")
    print(f"Pares válidos: {valid}")
    print(f"Salida válida: {out_left} | {out_right}")
    print(f"Reporte: {report_csv}")


if __name__ == "__main__":
    main()
