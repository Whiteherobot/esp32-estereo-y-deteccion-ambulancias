/**
 * MODEL — HandDetector.cpp
 * ========================
 * Detección de hasta dos manos por segmentación de piel en YCrCb.
 *
 * Autores: Jordy Romero · Michael Lata
 * Materia: Visión por Computador — Proyecto Integrador
 */

#include "../../include/model/HandDetector.hpp"
#include <algorithm>

std::array<HandDetector::Hand, 2> HandDetector::detect(const cv::Mat& bgr) {
    std::array<Hand, 2> result{};

    // --- 1. Conversión YCrCb ---
    // YCrCb separa luminancia de crominancia; Cr y Cb son robustos ante
    // cambios de brillo, ideal cuando AWB=OFF produce cast de color.
    cv::Mat ycrcb;
    cv::cvtColor(bgr, ycrcb, cv::COLOR_BGR2YCrCb);

    // --- 2. Máscara de piel (Chai & Ngan 1999) ---
    cv::Mat skinMask;
    cv::inRange(ycrcb,
                cv::Scalar(0,   133, 77),
                cv::Scalar(255, 173, 127),
                skinMask);

    // --- 3. Limpieza morfológica ---
    // CLOSE: rellena huecos dentro de la palma (pliegues, sombra de dedos)
    // OPEN : elimina parches pequeños de ruido de fondo
    const cv::Mat ke = cv::getStructuringElement(cv::MORPH_ELLIPSE, {11, 11});
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_CLOSE, ke);
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_OPEN,  ke);

    // --- 4. Contornos → ordenar por área descendente ---
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return result;

    std::sort(contours.begin(), contours.end(),
              [](const auto& a, const auto& b){
                  return cv::contourArea(a) > cv::contourArea(b);
              });

    // --- 5. Tomar los dos mayores contornos que superen el área mínima ---
    int slot = 0;
    for (const auto& c : contours) {
        if (slot >= kMaxHands) break;
        if (cv::contourArea(c) < kMinArea) break;  // ya están ordenados, el resto es menor

        const cv::Moments m = cv::moments(c);
        if (m.m00 == 0.0) continue;

        cv::Point2f encCenter;
        float encR;
        cv::minEnclosingCircle(c, encCenter, encR);

        Hand& h   = result[slot++];
        h.found   = true;
        h.center  = cv::Point(static_cast<int>(m.m10 / m.m00),
                              static_cast<int>(m.m01 / m.m00));
        h.radius  = static_cast<int>(encR * 0.45f);
    }

    return result;
}
