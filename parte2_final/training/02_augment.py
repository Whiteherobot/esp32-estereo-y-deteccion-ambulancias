"""
02_augment.py — Aumento de datos con Albumentations hasta ≥4000 positivas y ≥4000 negativas.

Toma las imágenes crudas normalizadas de raw/{positives,negatives} y genera variantes
(brillo/contraste, rotaciones leves, motion blur, ruido, recorte, flip horizontal)
hasta alcanzar TARGET_POS / TARGET_NEG en augmented/{positives,negatives}.

El flip horizontal es seguro para una ambulancia (aproximadamente simétrica); las
rotaciones se mantienen leves (±12°) para no romper la pose típica del vehículo.

Uso:
  python 02_augment.py
  python 02_augment.py --pos 4000 --neg 4000
"""
import argparse
import random
import cv2

import config as C

try:
    import albumentations as A
except ImportError:
    A = None


def build_pipeline():
    """Pipeline de aumento coherente con vehículos en vía."""
    return A.Compose([
        A.HorizontalFlip(p=0.5),
        A.RandomBrightnessContrast(brightness_limit=0.25,
                                   contrast_limit=0.25, p=0.7),
        A.Rotate(limit=12, border_mode=cv2.BORDER_REFLECT_101, p=0.5),
        A.MotionBlur(blur_limit=5, p=0.3),
        A.GaussNoise(std_range=(0.03, 0.12), p=0.3),
        A.RandomShadow(p=0.2),
        A.HueSaturationValue(hue_shift_limit=6, sat_shift_limit=15,
                             val_shift_limit=10, p=0.3),
    ])


def load_raw(src_dir):
    imgs = []
    for p in sorted(src_dir.iterdir()):
        if p.suffix.lower() in C.IMG_EXTS:
            im = cv2.imread(str(p))
            if im is not None:
                imgs.append((p.stem, im))
    return imgs


def augment_class(raw, out_dir, target, pipeline):
    """Copia crudas + genera variantes hasta llegar a `target`."""
    for name, im in raw:
        cv2.imwrite(str(out_dir / f"{name}_orig.jpg"),
                    cv2.resize(im, C.WIN_SIZE))
    produced = len(raw)
    i = 0
    while produced < target and raw:
        name, im = raw[i % len(raw)]
        aug = pipeline(image=im)["image"]
        aug = cv2.resize(aug, C.WIN_SIZE)
        cv2.imwrite(str(out_dir / f"{name}_aug{produced:05d}.jpg"), aug)
        produced += 1
        i += 1
    return produced


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pos", type=int, default=C.TARGET_POS)
    ap.add_argument("--neg", type=int, default=C.TARGET_NEG)
    args = ap.parse_args()

    if A is None:
        raise SystemExit("Instala albumentations:  pip install albumentations")

    random.seed(C.RANDOM_SEED)
    C.ensure_dirs()
    pipe = build_pipeline()

    raw_pos = load_raw(C.RAW_POS)
    raw_neg = load_raw(C.RAW_NEG)
    if not raw_pos or not raw_neg:
        raise SystemExit("Faltan imágenes crudas. Ejecuta primero 01_collect.py.")

    print(f"Positivas crudas: {len(raw_pos)} → objetivo {args.pos}")
    np_ = augment_class(raw_pos, C.AUG_POS, args.pos, pipe)
    print(f"  generadas: {np_}")

    print(f"Negativas crudas: {len(raw_neg)} → objetivo {args.neg}")
    nn_ = augment_class(raw_neg, C.AUG_NEG, args.neg, pipe)
    print(f"  generadas: {nn_}")

    print(f"\nDataset aumentado: {np_} positivas + {nn_} negativas")


if __name__ == "__main__":
    main()
