#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VENV="/home/michi/venvs/cv-ai311"

echo "[1/4] Verificando entorno virtual: $VENV"
if [ ! -x "$VENV/bin/python" ]; then
  echo "No existe $VENV. Créalo primero con Python 3.11.9"
  exit 1
fi

source "$VENV/bin/activate"

echo "[2/4] Instalando dependencias base del proyecto"
pip install -r "$BASE_DIR/python/requirements.txt" --extra-index-url https://download.pytorch.org/whl/cu128

echo "[3/4] Copiando config ejemplo si no existe"
if [ ! -f "$BASE_DIR/config/cameras.json" ]; then
  cp "$BASE_DIR/config/cameras.example.json" "$BASE_DIR/config/cameras.json"
  echo "Creado: $BASE_DIR/config/cameras.json"
fi

echo "[4/4] Prueba de entorno"
python "$BASE_DIR/python/check_environment.py"

echo "Listo. Edita config/cameras.json y ejecuta check_esp32_streams.py"
