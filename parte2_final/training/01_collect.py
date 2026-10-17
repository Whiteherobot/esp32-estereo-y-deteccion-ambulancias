"""
01_collect.py — Organización y normalización del material crudo del dataset.

Qué hace:
  1. Extrae fotogramas de los videos colocados en dataset/raw/videos_{positives,negatives}/
     (1 fotograma cada FRAME_EVERY) y los guarda como imágenes en raw/{positives,negatives}.
  2. Normaliza TODAS las imágenes crudas a la ventana de detección (WIN_W x WIN_H),
     conservando relación de aspecto con letterbox, y las deja listas para augmentar.

Uso:
  python 01_collect.py                # procesa lo que haya en raw/
  python 01_collect.py --frame-every 15

NOTA: la curación (recortar la ambulancia del fondo en las positivas) es manual;
este script asume que las positivas ya están razonablemente centradas en el vehículo.
"""
import argparse
import cv2

import config as C


def extract_frames(video_dir, out_dir, every):
    """Extrae 1 fotograma cada `every` de cada video del directorio."""
    if not video_dir.exists():
        return 0
    n = 0
    for vid in sorted(video_dir.iterdir()):
        if vid.suffix.lower() not in (".mp4", ".avi", ".mov", ".mkv"):
            continue
        cap = cv2.VideoCapture(str(vid))
        idx, saved = 0, 0
        while True:
            ok, frame = cap.read()
            if not ok:
                break
            if idx % every == 0:
                out = out_dir / f"{vid.stem}_f{idx:05d}.jpg"
                cv2.imwrite(str(out), frame)
                saved += 1
            idx += 1
        cap.release()
        print(f"  [{vid.name}] → {saved} fotogramas")
        n += saved
    return n


def letterbox(img, size):
    """Redimensiona a `size` (w,h) conservando aspecto, rellenando con negro."""
    w, h = size
    ih, iw = img.shape[:2]
    scale = min(w / iw, h / ih)
    nw, nh = int(iw * scale), int(ih * scale)
    resized = cv2.resize(img, (nw, nh), interpolation=cv2.INTER_AREA)
    canvas = cv2.copyMakeBorder(
        resized, (h - nh) // 2, h - nh - (h - nh) // 2,
        (w - nw) // 2, w - nw - (w - nw) // 2,
        cv2.BORDER_CONSTANT, value=(0, 0, 0))
    return canvas


def normalize_dir(src_dir):
    """Reescribe cada imagen del directorio a la ventana de detección."""
    count = 0
    for img_path in sorted(src_dir.iterdir()):
        if img_path.suffix.lower() not in C.IMG_EXTS:
            continue
        img = cv2.imread(str(img_path))
        if img is None:
            continue
        norm = letterbox(img, C.WIN_SIZE)
        cv2.imwrite(str(img_path), norm)
        count += 1
    return count


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--frame-every", type=int, default=C.FRAME_EVERY)
    args = ap.parse_args()

    C.ensure_dirs()

    print("== Extrayendo fotogramas de videos ==")
    p = extract_frames(C.RAW_VIDEOS_POS, C.RAW_POS, args.frame_every)
    n = extract_frames(C.RAW_VIDEOS_NEG, C.RAW_NEG, args.frame_every)
    print(f"Positivas desde video: {p} · Negativas desde video: {n}")

    print("== Normalizando a ventana de detección "
          f"({C.WIN_W}x{C.WIN_H}) ==")
    np_ = normalize_dir(C.RAW_POS)
    nn_ = normalize_dir(C.RAW_NEG)
    print(f"Positivas crudas totales: {np_} · Negativas crudas totales: {nn_}")

    if np_ == 0 or nn_ == 0:
        print("\n[!] Faltan imágenes crudas. Coloca material en:")
        print(f"    positivas → {C.RAW_POS}")
        print(f"    negativas → {C.RAW_NEG}")
        print(f"    (o videos en {C.RAW_VIDEOS_POS} / {C.RAW_VIDEOS_NEG})")


if __name__ == "__main__":
    main()
