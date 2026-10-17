"""
Configuración compartida del pipeline HOG + SVM para detección de ambulancias.

Todos los scripts (01..06) importan de aquí para mantener coherentes las rutas,
el tamaño de la ventana de detección y los parámetros HOG entre el entrenamiento
(Python) y la inferencia (C++).
"""
from pathlib import Path

# ---------------------------------------------------------------------------
# Rutas base
# ---------------------------------------------------------------------------
ROOT = Path(__file__).resolve().parent.parent          # parte2_final/
DATASET = ROOT / "dataset"
RAW_POS = DATASET / "raw" / "positives"
RAW_NEG = DATASET / "raw" / "negatives"
AUG_POS = DATASET / "augmented" / "positives"
AUG_NEG = DATASET / "augmented" / "negatives"
TRAIN_DIR = DATASET / "train"
VAL_DIR = DATASET / "val"
TEST_DIR = DATASET / "test"
MODELS = ROOT / "models"
MANIFEST = DATASET / "manifest.csv"

# Videos de entrada para extraer fotogramas (colócalos aquí o pasa --videos)
RAW_VIDEOS_POS = DATASET / "raw" / "videos_positives"
RAW_VIDEOS_NEG = DATASET / "raw" / "videos_negatives"

# ---------------------------------------------------------------------------
# Ventana de detección  (furgón: más ancho que alto)
# ---------------------------------------------------------------------------
WIN_W = 128
WIN_H = 96
WIN_SIZE = (WIN_W, WIN_H)          # (ancho, alto) — orden OpenCV Size

# ---------------------------------------------------------------------------
# Parámetros HOG  (deben COINCIDIR con los del cv::HOGDescriptor en C++)
# ---------------------------------------------------------------------------
HOG_BLOCK = (16, 16)               # tamaño de bloque en px
HOG_BLOCK_STRIDE = (8, 8)
HOG_CELL = (8, 8)
HOG_NBINS = 9

# ---------------------------------------------------------------------------
# Dataset / entrenamiento
# ---------------------------------------------------------------------------
TARGET_POS = 4000                  # mínimo exigido por la rúbrica
TARGET_NEG = 4000
SPLIT = (0.70, 0.15, 0.15)         # train / val / test
RANDOM_SEED = 42

# SVM
SVM_C = 1.0
HARD_NEG_ROUNDS = 2                # rondas de hard-negative mining

# Extracción de fotogramas de video
FRAME_EVERY = 10                   # 1 fotograma cada N

IMG_EXTS = (".jpg", ".jpeg", ".png", ".bmp", ".webp")


def ensure_dirs():
    """Crea las carpetas de salida si no existen."""
    for d in (RAW_POS, RAW_NEG, AUG_POS, AUG_NEG, TRAIN_DIR, VAL_DIR,
              TEST_DIR, MODELS, RAW_VIDEOS_POS, RAW_VIDEOS_NEG):
        d.mkdir(parents=True, exist_ok=True)
