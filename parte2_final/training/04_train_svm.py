"""
04_train_svm.py — Entrenamiento del SVM lineal + hard-negative mining.

Entrena un SVM lineal (sklearn LinearSVC / liblinear) sobre los descriptores HOG
del paso 03 y lo refina con hard-negative mining: busca falsos positivos en imágenes
de fondo grandes (dataset/raw/negatives_full/), recorta las ventanas mal clasificadas
y reentrena agregándolas como negativas difíciles.

Por qué LinearSVC y no cv2.ml.SVM: el solver SMO de OpenCV (SVM_C_SVC) es
inviablemente lento sobre 5600 muestras x 5940 dims (>9 min sin converger).
liblinear entrena el mismo SVM lineal en segundos. El detector final para el C++
es el vector (w|b), que es idéntico sin importar el solver.

Salida:
  models/hog_svm.npz   (w: pesos del hiperplano, b: sesgo)

Uso:
  python 04_train_svm.py
  python 04_train_svm.py --rounds 2 --c 1.0
"""
import argparse
import numpy as np
import cv2
from sklearn.svm import LinearSVC

import config as C


def train_linear_svm(X, y, c_val):
    """SVM lineal via liblinear. y: 1=ambulancia, 0=fondo."""
    clf = LinearSVC(C=c_val, max_iter=20000, tol=1e-4)
    clf.fit(X, y)
    return clf


def build_detector_vector(clf):
    """Vector (w|b) que consume cv::HOGDescriptor::setSVMDetector.
    HOG calcula score = w·features + b y detecta si score > hitThreshold;
    LinearSVC da decision_function = coef_·x + intercept_ (>0 => clase 1)."""
    w = clf.coef_.ravel().astype(np.float32)
    b = np.float32(clf.intercept_[0])
    detector = np.concatenate([w, [b]]).astype(np.float32)
    return detector


def hard_negative_mining(clf, hog, rounds):
    """Barre dataset/raw/negatives_full/ con el detector actual; cada ventana
    detectada es un falso positivo (no hay ambulancias ahí) -> negativa difícil.
    Devuelve descriptores HOG extra (o vacío si no hay material full-frame)."""
    full_neg = C.DATASET / "raw" / "negatives_full"
    if not full_neg.exists():
        return np.empty((0, 0), dtype=np.float32)
    hog.setSVMDetector(build_detector_vector(clf))
    extra = []
    for _ in range(rounds):
        for p in sorted(full_neg.iterdir()):
            if p.suffix.lower() not in C.IMG_EXTS:
                continue
            img = cv2.imread(str(p), cv2.IMREAD_GRAYSCALE)
            if img is None:
                continue
            rects, _ = hog.detectMultiScale(img, hitThreshold=0)
            for (x, y0, w, h) in rects:
                crop = img[max(0, y0):y0 + h, max(0, x):x + w]
                if crop.size == 0:
                    continue
                crop = cv2.resize(crop, C.WIN_SIZE)
                extra.append(hog.compute(crop).ravel())
    if not extra:
        return np.empty((0, 0), dtype=np.float32)
    return np.asarray(extra, dtype=np.float32)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=C.HARD_NEG_ROUNDS)
    ap.add_argument("--c", type=float, default=C.SVM_C)
    args = ap.parse_args()

    X = np.load(C.MODELS / "hog_train_X.npy")
    y = np.load(C.MODELS / "hog_train_y.npy").astype(np.int32)
    print(f"Entrenando SVM lineal (liblinear) sobre {X.shape[0]} muestras "
          f"(dim={X.shape[1]}) ...")

    clf = train_linear_svm(X, y, args.c)

    hog = cv2.HOGDescriptor(C.WIN_SIZE, C.HOG_BLOCK, C.HOG_BLOCK_STRIDE,
                            C.HOG_CELL, C.HOG_NBINS)
    extra = hard_negative_mining(clf, hog, args.rounds)
    if extra.size:
        print(f"Hard-negative mining: +{extra.shape[0]} negativas difíciles")
        X = np.vstack([X, extra])
        y = np.concatenate([y, np.zeros(extra.shape[0], dtype=np.int32)])
        clf = train_linear_svm(X, y, args.c)

    w = clf.coef_.ravel().astype(np.float32)
    b = np.float32(clf.intercept_[0])
    out = C.MODELS / "hog_svm.npz"
    np.savez(out, w=w, b=b)
    print(f"Modelo guardado en {out}  (|w|={w.shape[0]}, b={b:.4f})")


if __name__ == "__main__":
    main()
