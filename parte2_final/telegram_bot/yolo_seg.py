"""
yolo_seg.py — Segmentación de instancias con YOLO (ultralytics).

Envuelve un modelo YOLOv8/YOLOv11-seg preentrenado para:
  - segmentar una imagen y devolver el frame con máscaras translúcidas + etiquetas
  - segmentar un video frame a frame y escribir un MP4 segmentado

Imprime métricas de inferencia (FPS, confianza media) que el informe exige.
"""
import time
import os
import cv2
import numpy as np

try:
    from ultralytics import YOLO
except ImportError:
    YOLO = None


class YoloSegmenter:
    def __init__(self, weights=None):
        if YOLO is None:
            raise RuntimeError("Instala ultralytics:  pip install ultralytics")
        weights = weights or os.getenv("YOLO_MODEL", "yolov8n-seg.pt")
        self.model = YOLO(weights)
        self.name = weights

    def segment_image(self, img_bgr, conf=0.35):
        """Devuelve (imagen_segmentada, confianza_media, fps)."""
        t0 = time.time()
        res = self.model(img_bgr, conf=conf, verbose=False)[0]
        fps = 1.0 / max(time.time() - t0, 1e-6)
        overlay = res.plot()                       # máscaras + cajas + etiquetas
        confs = res.boxes.conf.cpu().numpy() if res.boxes is not None else []
        mean_conf = float(np.mean(confs)) if len(confs) else 0.0
        return overlay, mean_conf, fps

    def segment_video(self, in_path, out_path, conf=0.35):
        """Segmenta cada frame y escribe un MP4. Devuelve (fps_medio, conf_media)."""
        cap = cv2.VideoCapture(in_path)
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        src_fps = cap.get(cv2.CAP_PROP_FPS) or 25
        writer = cv2.VideoWriter(out_path,
                                 cv2.VideoWriter_fourcc(*"mp4v"),
                                 src_fps, (w, h))
        fps_list, conf_list = [], []
        while True:
            ok, frame = cap.read()
            if not ok:
                break
            seg, c, f = self.segment_image(frame, conf)
            writer.write(seg)
            fps_list.append(f)
            if c:
                conf_list.append(c)
        cap.release()
        writer.release()
        mean_fps = float(np.mean(fps_list)) if fps_list else 0.0
        mean_conf = float(np.mean(conf_list)) if conf_list else 0.0
        return mean_fps, mean_conf
