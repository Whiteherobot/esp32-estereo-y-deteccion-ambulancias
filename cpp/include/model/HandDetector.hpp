/**
 * MODEL — HandDetector.hpp
 * ========================
 * Detecta hasta dos manos en un frame BGR mediante segmentación de piel
 * en espacio YCrCb (robusto frente a cambios de iluminación y AWB=OFF).
 *
 * Algoritmo:
 *   1. Convertir BGR → YCrCb
 *   2. Segmentar piel: Cr ∈ [133,173], Cb ∈ [77,127]  (Chai & Ngan 1999)
 *   3. Limpieza morfológica (CLOSE + OPEN)
 *   4. Encontrar contornos, seleccionar los dos mayores ≥ kMinArea
 *   5. Calcular centroide y radio enclosing por contorno
 *
 * Complejidad de dos personas:
 *   detect() retorna exactamente dos slots (hand[0] y hand[1]).
 *   Cuando ambos tienen found=true el sistema puede renderizar efectos
 *   de interacción (rayo de energía, distancia entre manos, etc.).
 *
 * Autores: Jordy Romero · Michael Lata
 * Materia: Visión por Computador — Proyecto Integrador
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <array>

class HandDetector {
public:
    struct Hand {
        bool      found  = false;
        cv::Point center = {-1, -1};
        int       radius = 0;
    };

    // Siempre retorna exactamente 2 slots; hand[i].found indica si es válido.
    // hand[0] = mano más grande detectada, hand[1] = segunda mano (si existe).
    static std::array<Hand, 2> detect(const cv::Mat& bgr);

private:
    static constexpr int kMinArea = 2000;   // área mínima (px²)
    static constexpr int kMaxHands = 2;
};
