# esp32-estereo-y-deteccion-ambulancias

[![CI](https://github.com/Whiteherobot/esp32-estereo-y-deteccion-ambulancias/actions/workflows/ci.yml/badge.svg)](https://github.com/Whiteherobot/esp32-estereo-y-deteccion-ambulancias/actions/workflows/ci.yml)

Proyecto integrador de Visión Artificial (Universidad Politécnica Salesiana,
grupo 8, abril–agosto de 2026), en dos partes:

- **Parte I — visión estéreo:** dos ESP32-CAM calibradas como par estéreo miden
  la distancia a los objetos en tiempo real (C++ con OpenCV, arquitectura MVC)
  y la muestran con un efecto de realidad aumentada que reacciona a la
  profundidad. Error medido de 0 a 4 cm entre 30 y 90 cm.
- **Parte II — [detección de ambulancias](parte2_final/README.md):** un
  detector clásico HOG + SVM en C++ reconoce ambulancias del MSP y dispara un
  servicio en Python que segmenta la imagen con YOLOv8 y avisa por Telegram
  con la foto y un clip de 5 segundos.

Los dos programas en C++ usan **OpenCV 4** (en OpenCV 5 `HOGDescriptor` salió
del módulo principal) y `ximgproc` de opencv-contrib. En Ubuntu:
`sudo apt install libopencv-dev libopencv-contrib-dev libcurl4-openssl-dev`.
La CI compila los dos.

La red WiFi del firmware va en `arduino/*/secretos.h` (se copia de
`secretos.ejemplo.h` y no se versiona); el token del bot, en
`parte2_final/telegram_bot/.env`.

---

# Parte I: visión estéreo con ESP32-CAM

## Estructura del proyecto

```
├── arduino/
│   ├── ESP32_CAM_Stereo/        ← firmware de producción (flashear esto)
│   └── dev/                     ← sketches de prueba (serial, wifi, snapshot)
│
├── cpp/                         ← aplicación principal C++ (MVC)
│   ├── CMakeLists.txt
│   ├── include/  (model/ view/ controller/)
│   └── src/      (model/ view/ controller/ main.cpp stereo_calibrate.cpp)
│
├── python/                      ← scripts de soporte
│   ├── capture_stereo_pairs.py  ← captura imágenes para calibración
│   ├── filter_calib_pairs.py    ← filtra pares válidos
│   ├── check_esp32_streams.py   ← verifica conectividad de cámaras
│   └── depth_distortion_filter.py ← filtro visual experimental (Python)
│
├── data/
│   ├── calib/                   ← imágenes raw de calibración
│   ├── calib_filtered/          ← imágenes filtradas (usadas para calibrar)
│   └── output/                  ← snapshots de prueba
│
├── config/
│   ├── cameras.json             ← URLs de las ESP32-CAM
│   └── stereo_calib.yml         ← parámetros de calibración estéreo
│
├── docs/                        ← documentación y tutoriales
└── scripts/                     ← utilidades de shell
```

---

## Inicio rápido

### 1. Verificar cámaras
```bash
cd python && source ~/venvs/cv-ai311/bin/activate.fish
python check_esp32_streams.py
```

### 2. Calibrar (si es necesario)
```bash
# Capturar pares con tablero 9×6, cuadros 22mm
python capture_stereo_pairs.py --bw 9 --bh 6

# Filtrar pares válidos
python filter_calib_pairs.py --bw 9 --bh 6

# Calibrar
cd ../cpp/build
./stereo_calibrate --left=../../data/calib_filtered/left \
                   --right=../../data/calib_filtered/right \
                   --bw=9 --bh=6 --sq=0.022 \
                   --out=../../config/stereo_calib.yml
```

### 3. Ejecutar sistema principal
```bash
cd cpp/build
./stereo_measure ../../config/stereo_calib.yml \
    http://172.20.10.2/capture \
    http://172.20.10.3/capture
```

---

## Compilar C++
```bash
cd cpp && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j4
```

---

## Precisión del sistema (baseline=5.44cm, RMS=0.46px)

| Distancia | Precisión |
|-----------|-----------|
| 0.30 m    | ±0.5 cm   |
| 0.50 m    | ±1.4 cm   |
| 1.00 m    | ±5.6 cm   |

Rango óptimo: **30 cm – 1 m**

---

## Autores

- Michael Lata ([@Whiteherobot](https://github.com/Whiteherobot))
- Jordy Romero ([@JordyRomeroa](https://github.com/JordyRomeroa))

Licencia MIT.
