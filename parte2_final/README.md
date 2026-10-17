# Parte II Final — Detección de Ambulancias (HOG+SVM) + Bot Telegram (YOLO-seg)

**Proyecto Integrador · Visión Artificial (G8) · UPS**
Autores: Michael Lata · Jordy Romero — Docente: Vladimir Robles Bykbaev — Período: Abril–Agosto 2026

Vehículo objetivo: **Ambulancia del MSP ecuatoriano** (patrón Battenberg rojo/azul,
estrella de la vida, texto "AMBULANCIA"). Detector clásico **HOG + SVM** en C++.

---

## Arquitectura end-to-end

```
Cámara/video ──▶ APP C++ (HOG+SVM, MVC, HUD RAM/FPS/conf)
                   │  detecta ambulancia → POST imagen + clip 5s (libcurl)
                   ▼
              SERVICIO PYTHON (FastAPI) ── YOLOv8-seg ──▶ BOT TELEGRAM
                                                          1) imagen + alerta
                                                          2) imagen segmentada
                                                          3) video segmentado 5s
```

El detector en C++ es **clásico** (HOG+SVM) y actúa como *disparador*. Todo lo de
Deep Learning (YOLO) vive únicamente en el servicio Python, tal como permite la rúbrica.

---

## Estructura

```
parte2_final/
├── dataset/          imágenes crudas, aumentadas y splits train/val/test
├── training/         pipeline Python: colección → augment → HOG → SVM → evaluación → export
├── models/           SVM entrenado y vector detector para C++
├── cpp_detector/     app de escritorio C++ (MVC + OpenCV + libcurl)
├── telegram_bot/     servicio FastAPI + YOLO-seg + bot de Telegram
├── docs/             diagrama de arquitectura + informe Parte II (inglés)
└── scripts/          utilidades de orquestación
```

---

## Puesta en marcha (resumen)

### 1. Entrenamiento del detector (Python, venv `cv-ai311`)
```bash
source ~/venvs/cv-ai311/bin/activate.fish
cd parte2_final/training
python 01_collect.py        # organiza fotos + extrae fotogramas de videos
python 02_augment.py        # Albumentations → ≥4000 positivas + ≥4000 negativas
python 03_extract_hog.py    # descriptores HOG → .npy
python 04_train_svm.py      # SVM lineal + hard-negative mining
python 05_evaluate.py       # matriz de confusión + métricas
python 06_export_detector.py# exporta detector para C++
```

### 2. App de escritorio (C++)
```bash
cd parte2_final/cpp_detector && mkdir -p build && cd build
cmake .. && make -j
./ambulance_detector --video /ruta/trafico.mp4 --model ../../models/hog_detector.yml \
                     --thresh 0.0 --trigger-frames 5 --cooldown 15
```

**Atajo — servidor + app en un comando** (`scripts/run_demo.sh` levanta el servicio
Python, espera a que esté listo, corre el detector y apaga el servidor al salir):
```bash
./scripts/run_demo.sh                       # video de demo (tráfico -> ambulancia)
./scripts/run_demo.sh 0                      # webcam del laptop
./scripts/run_demo.sh ~/Downloads/clip.mp4   # video del celular
./scripts/run_demo.sh ~/Downloads/foto.jpg   # foto (auto --trigger-frames 1)
./scripts/run_demo.sh http://192.168.0.50:8080/video   # celular como cámara IP
```
> El detector clásico actúa como **disparador a nivel de frame**: clasifica el
> frame completo con HOG+SVM (score `w·f + b`) y dispara la alerta tras
> `--trigger-frames` frames positivos (`score > --thresh`) consecutivos. La
> localización/segmentación fina la hace YOLO en el bot. (Validación frame-level:
> recall ambulancia ~98.6% / falso-positivo tráfico ~0% con `--thresh 0`.)

### 3. Bot de Telegram + YOLO (Python)
```bash
cd parte2_final/telegram_bot
cp config.example.env .env    # rellenar BOT_TOKEN y CHAT_ID
pip install -r requirements.txt
python server.py              # levanta FastAPI en :8000 (recibe del C++)
```

---

## Estado de fases

- [x] **Fase A — Scaffolding** (estructura, configs, esqueletos)
- [x] **Fase B — Dataset** (976 pos + 304 neg crudas → 4000/4000 augmentadas; 153 fondos full-res)
- [x] **Fase C — Entrenamiento HOG+SVM** (liblinear + hard-negative mining; test: acc 0.976, prec 0.982, recall 0.970, espec 0.982, F1 0.976)
- [x] **Fase D — App C++** (trigger frame-level; validada end-to-end headless: 1 trigger → API → YOLO → Telegram; RAM ~340 MB, detección >1600 FPS)
- [x] **Fase E — Bot Telegram + YOLO-seg** (3 entregables) — validado end-to-end vía `POST /alert` (@PracticaFinalx_Bot; YOLOv8n-seg)
- [x] **Fase F — Diagrama de arquitectura** (`docs/informe/figs/arquitectura.{tex,png}`)
- [x] **Fase G — Informe Parte II (inglés) + video-blog** (`docs/informe/main_EN.pdf` + `video_script.txt`; faltan métricas de sistema + link repo tras la demo)

## Insumos pendientes del usuario
1. Fotos/videos reales de ambulancias ecuatorianas (semilla) + videos de tráfico (negativas).
2. `BOT_TOKEN` (de @BotFather) + `CHAT_ID` de Telegram.
3. Fuente de video para el C++ (archivo `.mp4` de tráfico real recomendado).
