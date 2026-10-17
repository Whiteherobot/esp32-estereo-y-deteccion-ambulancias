#!/usr/bin/env bash
# ============================================================================
# run_demo.sh — Levanta el servicio Python (FastAPI + YOLO + Telegram) y la app
# C++ de detección de ambulancias, en un solo comando.
#
# Uso:
#   ./scripts/run_demo.sh                      # video de demo (trafico -> ambulancia)
#   ./scripts/run_demo.sh 0                    # webcam del laptop
#   ./scripts/run_demo.sh ~/Downloads/clip.mp4 # video del celular
#   ./scripts/run_demo.sh ~/Downloads/foto.jpg # foto (usa --trigger-frames 1)
#   ./scripts/run_demo.sh http://192.168.0.50:8080/video   # celular como cámara IP
#
# Opciones extra van al final y se pasan tal cual al binario, por ejemplo:
#   ./scripts/run_demo.sh 0 --thresh 0.5 --cooldown 20
#
# Variables de entorno:
#   VENV_PY   ruta al python del venv (def: ~/venvs/cv-ai311/bin/python)
#   HEADLESS  =1 para correr sin ventana (útil para pruebas)
# ============================================================================
set -euo pipefail

# --- rutas (relativas a este script) ---------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"                       # parte2_final/
BIN="$ROOT/cpp_detector/build/ambulance_detector"
MODEL="$ROOT/models/hog_detector.yml"
BOT_DIR="$ROOT/telegram_bot"
DEMO_VIDEO="$ROOT/demo/demo_trafico_ambulancia.mp4"
PY="${VENV_PY:-$HOME/venvs/cv-ai311/bin/python}"
ENDPOINT="http://127.0.0.1:8000/alert"
HEALTH="http://127.0.0.1:8000/health"

# --- fuente y opciones ------------------------------------------------------
SOURCE="${1:-$DEMO_VIDEO}"
shift || true                                        # el resto son extras
EXTRA_ARGS=("$@")

# Si la fuente es una imagen, un solo frame no llega a 5 consecutivos:
# bajamos el debounce a 1 salvo que el usuario ya lo haya pasado.
TRIGGER_FRAMES=5
case "${SOURCE,,}" in
  *.jpg|*.jpeg|*.png|*.bmp) TRIGGER_FRAMES=1 ;;
esac
HEADLESS_FLAG=()
[[ "${HEADLESS:-0}" == "1" ]] && HEADLESS_FLAG=(--headless)

# --- comprobaciones ---------------------------------------------------------
[[ -x "$BIN" ]]   || { echo "ERROR: no existe el binario $BIN"; echo "  Compilá: cd cpp_detector && mkdir -p build && cd build && cmake .. && make -j"; exit 1; }
[[ -f "$MODEL" ]] || { echo "ERROR: falta el modelo $MODEL (corré scripts/run_training.sh)"; exit 1; }
[[ -x "$PY" ]]    || { echo "ERROR: no encuentro el python del venv en $PY (definí VENV_PY)"; exit 1; }
[[ -f "$BOT_DIR/.env" ]] || echo "AVISO: no hay $BOT_DIR/.env — el envío a Telegram fallará (copiá config.example.env a .env)."

# --- servidor: reusar si ya está, si no levantarlo --------------------------
SERVER_PID=""
cleanup() {
  if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    echo ""; echo "Deteniendo el servidor (PID $SERVER_PID)…"
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

if curl -s --max-time 2 "$HEALTH" >/dev/null 2>&1; then
  echo "Servidor ya activo en :8000 — lo reuso."
else
  echo "Levantando el servicio Python (FastAPI + YOLO)…"
  ( cd "$BOT_DIR" && exec "$PY" server.py ) &
  SERVER_PID=$!
  # esperar a que /health responda (torch/ultralytics tardan en importar)
  echo -n "Esperando al servidor"
  for _ in $(seq 1 40); do
    if curl -s --max-time 2 "$HEALTH" >/dev/null 2>&1; then echo " listo."; break; fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then echo ""; echo "ERROR: el servidor murió al arrancar."; exit 1; fi
    echo -n "."; sleep 2
  done
  curl -s --max-time 2 "$HEALTH" >/dev/null 2>&1 || { echo ""; echo "ERROR: el servidor no respondió a tiempo."; exit 1; }
fi

# --- app C++ ----------------------------------------------------------------
echo "Fuente: $SOURCE   (trigger-frames=$TRIGGER_FRAMES)"
echo "Iniciando detector…  (q para salir)"
"$BIN" \
  --video "$SOURCE" \
  --model "$MODEL" \
  --endpoint "$ENDPOINT" \
  --thresh 0.0 --trigger-frames "$TRIGGER_FRAMES" --cooldown 15 \
  "${HEADLESS_FLAG[@]}" "${EXTRA_ARGS[@]}"

echo "Detector finalizado."
