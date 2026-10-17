"""
server.py — Servicio FastAPI que recibe la alerta del detector C++.

Endpoint:
  POST /alert   (multipart)
      image : imagen JPEG del frame clave (obligatorio)
      video : clip MP4 de >=5 s        (opcional pero recomendado, +puntaje rúbrica)
      confidence : confianza del detector clásico (form field)

Flujo:
  1. Guarda imagen y video recibidos.
  2. Segmenta con YOLO (imagen + video).
  3. Envía los tres entregables al Telegram del usuario.
  4. Devuelve JSON con métricas (RAM, FPS de inferencia, confianza).

Ejecutar:
  python server.py           # lee host/puerto de .env
"""
import os
import time
import tempfile

import cv2
import psutil
from dotenv import load_dotenv
from fastapi import FastAPI, UploadFile, File, Form
import uvicorn

from yolo_seg import YoloSegmenter
import bot

load_dotenv()
app = FastAPI(title="Ambulance Alert Service")
segmenter = None          # perezoso: carga YOLO en el primer arranque


def get_segmenter():
    global segmenter
    if segmenter is None:
        segmenter = YoloSegmenter()
    return segmenter


def _ram_mb():
    return psutil.Process(os.getpid()).memory_info().rss / (1024 * 1024)


@app.get("/health")
def health():
    return {"status": "ok", "ram_mb": round(_ram_mb(), 1)}


@app.post("/alert")
async def alert(image: UploadFile = File(...),
                video: UploadFile = File(None),
                confidence: float = Form(0.0)):
    seg = get_segmenter()
    tmp = tempfile.mkdtemp(prefix="amb_")
    t0 = time.time()

    # --- imagen ---
    img_path = os.path.join(tmp, "original.jpg")
    with open(img_path, "wb") as f:
        f.write(await image.read())
    frame = cv2.imread(img_path)
    seg_img, mean_conf, img_fps = seg.segment_image(frame)
    seg_img_path = os.path.join(tmp, "segmented.jpg")
    cv2.imwrite(seg_img_path, seg_img)

    # --- video (opcional) ---
    seg_video_path = None
    vid_fps = vid_conf = 0.0
    if video is not None:
        vin = os.path.join(tmp, "clip.mp4")
        with open(vin, "wb") as f:
            f.write(await video.read())
        seg_video_path = os.path.join(tmp, "clip_seg.mp4")
        vid_fps, vid_conf = seg.segment_video(vin, seg_video_path)

    # --- envío a Telegram ---
    try:
        if seg_video_path:
            bot.deliver(img_path, seg_img_path, seg_video_path, confidence)
        else:
            bot.send_photo(img_path, "🚑 Ambulancia detectada")
            bot.send_photo(seg_img_path, "Segmentación YOLO")
    except Exception as e:                       # no romper si falta el token
        print(f"[telegram] no se pudo enviar: {e}")

    return {
        "elapsed_s": round(time.time() - t0, 3),
        "ram_mb": round(_ram_mb(), 1),
        "yolo_fps_image": round(img_fps, 1),
        "yolo_fps_video": round(vid_fps, 1),
        "yolo_mean_conf": round(max(mean_conf, vid_conf), 3),
        "detector_conf": confidence,
        "model": seg.name,
    }


if __name__ == "__main__":
    host = os.getenv("API_HOST", "0.0.0.0")
    port = int(os.getenv("API_PORT", "8000"))
    print(f"Servicio de alertas escuchando en http://{host}:{port}")
    uvicorn.run(app, host=host, port=port)
