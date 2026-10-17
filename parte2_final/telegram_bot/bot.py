"""
bot.py — Envío de los tres entregables a Telegram vía Bot API.

Dado el frame original y sus versiones segmentadas por YOLO, envía al chat registrado:
  1. Imagen original + mensaje de alerta.
  2. Imagen segmentada (máscaras de instancias).
  3. Video corto segmentado (>=5 s).

Usa la HTTP Bot API de Telegram directamente con requests (sin dependencias pesadas).
"""
import os
import requests

VEHICLE_NAME = "Ambulancia"


def _api(method):
    token = os.environ["TELEGRAM_BOT_TOKEN"]
    return f"https://api.telegram.org/bot{token}/{method}"


def send_photo(path, caption):
    chat = os.environ["TELEGRAM_CHAT_ID"]
    with open(path, "rb") as f:
        r = requests.post(_api("sendPhoto"),
                          data={"chat_id": chat, "caption": caption},
                          files={"photo": f}, timeout=60)
    r.raise_for_status()
    return r.json()


def send_video(path, caption=""):
    chat = os.environ["TELEGRAM_CHAT_ID"]
    with open(path, "rb") as f:
        r = requests.post(_api("sendVideo"),
                          data={"chat_id": chat, "caption": caption},
                          files={"video": f}, timeout=180)
    r.raise_for_status()
    return r.json()


def deliver(original_path, segmented_path, video_path, confidence):
    """Envía los tres entregables exigidos por la rúbrica."""
    alert = (f"🚑 Vehículo objetivo [{VEHICLE_NAME}] detectado en la escena "
             f"(confianza detector: {confidence:.2f})")
    send_photo(original_path, alert)
    send_photo(segmented_path,
               f"Segmentación de instancias (YOLO) — {VEHICLE_NAME}")
    send_video(video_path, "Secuencia segmentada frame a frame (YOLO)")
