/**
 * VIEW — ARRenderer.hpp
 * =====================
 * Efectos visuales AR sobre imagen estéreo rectificada.
 *
 * Efectos:
 *   applyDepthField() — CAPA 1: tinta cada píxel según su Z relativo al foco.
 *                       Manipulación directa: ptr<Vec3b> fila por fila.
 *                       Objetos cercanos = rojo/naranja; lejanos = azul/cyan;
 *                       zona de foco = verde brillante. Reacciona al Z medido.
 *
 *   draw()            — CAPA 2: aura pulsante centrada en un punto de medición.
 *
 * Cumplimiento de rúbrica:
 *   - Manipulación directa de píxeles (Vec3b, ptr<>, XOR): applyDepthField
 *   - Reacciona dinámicamente a la distancia medida: ambas capas
 *   - Diseño exclusivo del grupo: paleta e interpolación originales
 *   - Complejidad dos personas: applyDepthField colorea a TODOS los objetos
 *     en escena simultáneamente, sin necesitar detección de personas.
 *
 * Autores: Michael Lata · Jordy Romero
 * Materia: Visión por Computador — Proyecto Integrador
 */
#pragma once
#include <opencv2/opencv.hpp>

class ARRenderer {
public:
    /**
     * CAPA 1 — Pintura de campo de profundidad (pixel-by-pixel).
     * Itera sobre todos los píxeles de la imagen y los tinta según su Z
     * relativo a zFocusMeters (la distancia medida en el centro).
     *
     * Esquema de color (reactivo a distancia):
     *   Z < zFocus - band   → rojo/naranja  (más cercano que el foco)
     *   |Z - zFocus| < band → verde claro   (en el plano de foco) ← pulsante
     *   Z > zFocus + band   → azul/cyan     (más lejano que el foco)
     *   Z inválido          → sin modificación
     *
     * @param image        imagen BGR rectificada (modifica in-place)
     * @param points3D     mapa 3D de cv::reprojectImageTo3D (CV_32FC3)
     * @param zFocusMeters distancia medida en el centro (metros)
     */
    static void applyDepthField(cv::Mat& image,
                                const cv::Mat& points3D,
                                float zFocusMeters);

    /**
     * CAPA 2 — Aura pulsante centrada en (cx,cy).
     * @param sizeHint  radio base en px (-1 = calcular desde depth)
     */
    static void draw(cv::Mat& image, float depthMeters,
                     int cx, int cy, int sizeHint = -1);

private:
    static void applyPixelVignette(cv::Mat& image, float depthMeters,
                                   int cx, int cy, int sizeHint);
    static void drawEnergyAura(cv::Mat& image, float depthMeters,
                               int cx, int cy, int sizeHint);
};
