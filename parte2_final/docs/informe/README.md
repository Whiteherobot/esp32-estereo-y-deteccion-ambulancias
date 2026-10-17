# Informe Parte II (Final)

Aquí irá el informe en **inglés** (rúbrica: Video-Blog en inglés, sobre 4 pts) más el
guion del video-blog. Se construirá reutilizando el formato del informe del Interciclo
(`docs/informe/main.tex` de la Parte I): tema azul UPS, portada, secciones y bibliografía.

## Contenido exigido por la rúbrica del informe (5 pts)
1. **Estructura y descripción:** introducción, problema (tráfico vehicular ecuatoriano),
   justificación del dataset de ambulancias, propuesta (arquitectura C++ + Bot), conclusiones,
   bibliografía.
2. **Análisis cuantitativo y cualitativo:**
   - Detector HOG/SVM: matriz de confusión, precisión, sensibilidad, especificidad
     (figuras generadas por `training/05_evaluate.py` → `docs/figs/`).
   - Sistema integrado: RAM (app C++ y script Python), FPS, latencia de la API de Telegram.
   - YOLO: evaluación cualitativa del ajuste de las máscaras de segmentación.
3. **Formato y arquitectura:** redacción correcta, citas a papers y **diagrama** de la
   comunicación OpenCV C++ ↔ Bot Telegram (Python).

## Estado
- [x] `main_EN.tex` (informe EN) → compila a `main_EN.pdf` (10 pág., `plainnat`)
- [x] `figs/arquitectura.png` (diagrama del flujo C++↔Python; fuente `arquitectura.tex`)
- [x] `figs/cm_hog_svm.png` + `pr_hog_svm.png` (matriz confusión + curva PR)
- [x] `references.bib` (Dalal&Triggs, Cortes&Vapnik, YOLOv8, Albumentations, GoF)
- [x] `video_script.txt` (guion del video-blog en inglés)

### Compilar
```bash
cd docs/informe && latexmk -pdf main_EN.tex     # -> main_EN.pdf
# diagrama suelto: cd figs && pdflatex arquitectura.tex && pdftoppm -png -r 200 arquitectura.pdf arquitectura
```

## Pendiente (a completar tras la demo integrada)
- [ ] Rellenar métricas de sistema marcadas `[to measure: ...]` (RAM/FPS C++ y Python, latencia Telegram).
- [ ] Añadir figuras + discusión cualitativa de las máscaras YOLO.
- [ ] Reemplazar `<REPOSITORY-URL>` por el enlace real del repo.
