# Proyecto Integrador — Visión Estéreo y AR

**Universidad Politécnica Salesiana | Visión Artificial | Autor: Michael Lata**

Sistema de estimación de profundidad en tiempo real con dos ESP32-CAM y efecto AR,
implementado en C++ (OpenCV) con arquitectura MVC.

---

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
