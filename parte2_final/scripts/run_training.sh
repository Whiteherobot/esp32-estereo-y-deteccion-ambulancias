#!/usr/bin/env bash
# Orquesta el pipeline completo de entrenamiento del detector HOG+SVM.
# Requiere el venv cv-ai311 activo.
set -e
cd "$(dirname "$0")/../training"

echo "== 01 · Colección y normalización =="
python 01_collect.py

echo "== 02 · Aumento de datos (Albumentations) =="
python 02_augment.py

echo "== 03 · Extracción de descriptores HOG =="
python 03_extract_hog.py

echo "== 04 · Entrenamiento SVM + hard-negative mining =="
python 04_train_svm.py

echo "== 05 · Evaluación (matriz de confusión + métricas) =="
python 05_evaluate.py

echo "== 06 · Exportación del detector para C++ =="
python 06_export_detector.py

echo "Pipeline completo. Detector en ../models/hog_detector.yml"
