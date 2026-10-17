"""
06_export_detector.py — Exporta el detector para el C++.

A partir del SVM entrenado deriva el vector de detección lineal (w|b) que consume
cv::HOGDescriptor::setSVMDetector, y lo guarda en models/hog_detector.yml junto con
los parámetros de la ventana/HOG para que el C++ construya un HOGDescriptor idéntico.

Uso:
  python 06_export_detector.py
"""
import numpy as np
import cv2

import config as C


def main():
    model = np.load(C.MODELS / "hog_svm.npz")
    w, b = model["w"].astype(np.float32), np.float32(model["b"])

    # cv::HOGDescriptor::setSVMDetector espera [w..., b]; HOG detecta cuando
    # w·features + b > hitThreshold, igual que LinearSVC.decision_function > 0.
    detector = np.concatenate([w, [b]]).astype(np.float32)

    out = C.MODELS / "hog_detector.yml"
    fs = cv2.FileStorage(str(out), cv2.FILE_STORAGE_WRITE)
    fs.write("win_width", C.WIN_W)
    fs.write("win_height", C.WIN_H)
    fs.write("block", C.HOG_BLOCK[0])
    fs.write("block_stride", C.HOG_BLOCK_STRIDE[0])
    fs.write("cell", C.HOG_CELL[0])
    fs.write("nbins", C.HOG_NBINS)
    fs.write("detector", detector)
    fs.release()
    print(f"Detector exportado ({detector.shape[0]} coef.) → {out}")


if __name__ == "__main__":
    main()
