"""
03_extract_hog.py — Extracción de descriptores HOG del dataset aumentado.

Divide augmented/{positives,negatives} en train/val/test (sin fuga: el split es a
nivel de archivo con semilla fija), calcula el descriptor HOG de cada ventana con
cv2.HOGDescriptor (mismos parámetros que usará el C++) y guarda las matrices X,y en
models/ como .npy para el entrenamiento.

Salida:
  models/hog_{train,val,test}_X.npy
  models/hog_{train,val,test}_y.npy

Uso:
  python 03_extract_hog.py
"""
import numpy as np
import cv2

import config as C


def make_hog():
    """HOGDescriptor con los parámetros compartidos con el C++."""
    return cv2.HOGDescriptor(
        _winSize=C.WIN_SIZE,
        _blockSize=C.HOG_BLOCK,
        _blockStride=C.HOG_BLOCK_STRIDE,
        _cellSize=C.HOG_CELL,
        _nbins=C.HOG_NBINS,
    )


def list_images(d):
    return [p for p in sorted(d.iterdir()) if p.suffix.lower() in C.IMG_EXTS]


def split(items, seed):
    rng = np.random.default_rng(seed)
    idx = rng.permutation(len(items))
    n = len(items)
    n_tr = int(n * C.SPLIT[0])
    n_va = int(n * C.SPLIT[1])
    tr = [items[i] for i in idx[:n_tr]]
    va = [items[i] for i in idx[n_tr:n_tr + n_va]]
    te = [items[i] for i in idx[n_tr + n_va:]]
    return tr, va, te


def describe(hog, paths, label):
    X, y = [], []
    for p in paths:
        img = cv2.imread(str(p), cv2.IMREAD_GRAYSCALE)
        if img is None:
            continue
        if (img.shape[1], img.shape[0]) != C.WIN_SIZE:
            img = cv2.resize(img, C.WIN_SIZE)
        feat = hog.compute(img)
        X.append(feat.ravel())
        y.append(label)
    return X, y


def main():
    C.ensure_dirs()
    hog = make_hog()

    pos = list_images(C.AUG_POS)
    neg = list_images(C.AUG_NEG)
    if not pos or not neg:
        raise SystemExit("Faltan imágenes aumentadas. Ejecuta 02_augment.py.")

    pos_tr, pos_va, pos_te = split(pos, C.RANDOM_SEED)
    neg_tr, neg_va, neg_te = split(neg, C.RANDOM_SEED + 1)

    for name, (pp, nn) in {
        "train": (pos_tr, neg_tr),
        "val": (pos_va, neg_va),
        "test": (pos_te, neg_te),
    }.items():
        Xp, yp = describe(hog, pp, 1)
        Xn, yn = describe(hog, nn, 0)
        X = np.asarray(Xp + Xn, dtype=np.float32)
        y = np.asarray(yp + yn, dtype=np.int32)
        np.save(C.MODELS / f"hog_{name}_X.npy", X)
        np.save(C.MODELS / f"hog_{name}_y.npy", y)
        print(f"{name}: X{X.shape}  (pos={len(yp)}, neg={len(yn)})")

    print(f"\nDimensión del descriptor HOG: {X.shape[1]}")


if __name__ == "__main__":
    main()
