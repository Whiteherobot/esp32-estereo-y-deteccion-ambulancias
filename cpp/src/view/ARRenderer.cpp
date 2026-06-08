/**
 * VIEW — ARRenderer.cpp
 * =====================
 * Efecto AR de bola de energía sobre la mano detectada.
 * Dos capas de manipulación de píxeles:
 *   Capa 1: tinte radial centrado en la mano (applyPixelVignette)
 *   Capa 2: aura pulsante con anillos y destellos orbitales (drawEnergyAura)
 *
 * Autores : Michael Lata · Jordy Romero
 * Materia : Visión por Computador — Proyecto Integrador
 */

#include "../../include/view/ARRenderer.hpp"
#include <cmath>

// ---------------------------------------------------------------------------
// CAPA 1 — Campo de profundidad: manipulación directa píxel a píxel
// ---------------------------------------------------------------------------
void ARRenderer::applyDepthField(cv::Mat& image,
                                  const cv::Mat& points3D,
                                  float zFocusMeters) {
    if (!std::isfinite(zFocusMeters) || zFocusMeters <= 0.0f) return;

    const cv::Mat src = image.clone();
    const int rows = image.rows;
    const int cols = image.cols;
    const double t = static_cast<double>(cv::getTickCount()) / cv::getTickFrequency();

    // Z³ cae mucho más rápido que Z²:
    //  0.20m→75px  0.30m→22px  0.40m→9px  0.50m→5px  0.70m→2px  0.80m+→nada
    const float z3      = zFocusMeters * zFocusMeters * zFocusMeters;
    const float strength = std::clamp(0.6f / std::max(z3, 1e-4f), 0.0f, 70.0f);
    const float speed    = 10.0f / std::max(0.15f, zFocusMeters);
    if (strength < 2.0f) return;

    const float fgLimit = zFocusMeters + 0.50f;   // umbral primer plano

    for (int y = 0; y < rows; ++y) {
        cv::Vec3b*       dst = image.ptr<cv::Vec3b>(y);
        const cv::Vec3f* dep = points3D.ptr<cv::Vec3f>(y);

        for (int x = 0; x < cols; ++x) {
            const float Z = dep[x][2];
            if (!std::isfinite(Z) || Z < 0.05f || Z > fgLimit) continue;

            // Misma amplitud para toda la capa → onda coherente, sin grano
            const float ox = strength * std::sin(y * 0.10f + static_cast<float>(t) * speed);
            const float oy = strength * std::cos(x * 0.10f + static_cast<float>(t) * speed * 0.75f);

            const int sx = std::clamp(x + static_cast<int>(ox), 0, cols - 1);
            const int sy = std::clamp(y + static_cast<int>(oy), 0, rows - 1);

            dst[x] = src.at<cv::Vec3b>(sy, sx);
        }
    }
}

void ARRenderer::draw(cv::Mat& image, float depthMeters, int cx, int cy, int sizeHint) {
    if (!std::isfinite(depthMeters) || depthMeters <= 0.0f || depthMeters > 10.0f)
        return;
    applyPixelVignette(image, depthMeters, cx, cy, sizeHint);
    drawEnergyAura    (image, depthMeters, cx, cy, sizeHint);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// CAPA 1 — Tinte radial centrado en la mano (manipulación directa de píxeles)
// ---------------------------------------------------------------------------
void ARRenderer::applyPixelVignette(cv::Mat& image, float depthMeters,
                                     int cx, int cy, int sizeHint) {
    // Radio de influencia: preferir sizeHint (px reales de la mano detectada).
    // Si no hay hint, estimarlo desde la profundidad.
    float influence;
    if (sizeHint > 0)
        influence = static_cast<float>(sizeHint) * 1.4f;   // ligeramente mayor que la mano
    else
        influence = std::clamp(12.0f / std::max(0.1f, depthMeters), 20.0f, 80.0f);

    // Fuerza del tinte: más cerca → más intenso, pero nunca invasivo
    const float strength = std::clamp(0.55f - depthMeters * 0.25f, 0.20f, 0.55f);

    // Color de tinte según distancia (BGR)
    float tB, tG, tR;
    const float depthCm = depthMeters * 100.0f;
    if (depthCm < 40.0f)      { tB = 20;  tG = 60;  tR = 255; }  // rojo
    else if (depthCm < 80.0f) { tB = 20;  tG = 220; tR = 80;  }  // verde
    else                       { tB = 240; tG = 160; tR = 20;  }  // cyan

    const int yMin = std::max(0,           cy - (int)influence);
    const int yMax = std::min(image.rows,  cy + (int)influence + 1);
    const int xMin = std::max(0,           cx - (int)influence);
    const int xMax = std::min(image.cols,  cx + (int)influence + 1);

    for (int y = yMin; y < yMax; ++y) {
        cv::Vec3b* row = image.ptr<cv::Vec3b>(y);
        for (int x = xMin; x < xMax; ++x) {
            const float dx = static_cast<float>(x - cx);
            const float dy = static_cast<float>(y - cy);
            const float r  = std::sqrt(dx * dx + dy * dy) / influence;
            if (r >= 1.0f) continue;
            // w=1 en el centro, cae suavemente hacia el borde
            const float w     = (1.0f - r) * (1.0f - r);
            const float alpha = w * strength;
            cv::Vec3b& px = row[x];
            px[0] = static_cast<uchar>(px[0] * (1.0f - alpha) + tB * alpha);
            px[1] = static_cast<uchar>(px[1] * (1.0f - alpha) + tG * alpha);
            px[2] = static_cast<uchar>(px[2] * (1.0f - alpha) + tR * alpha);
        }
    }
}

// ---------------------------------------------------------------------------
// CAPA 2 — Aura energética pulsante
// ---------------------------------------------------------------------------
void ARRenderer::drawEnergyAura(cv::Mat& image, float depthMeters,
                                 int cx, int cy, int sizeHint) {
    const float depthCm = depthMeters * 100.0f;

    // Radio base: usar tamaño real de la mano en px si está disponible,
    // si no, estimar desde profundidad con valores mucho más conservadores.
    int baseRadius;
    if (sizeHint > 0)
        baseRadius = std::clamp(sizeHint, 12, 60);
    else
        baseRadius = std::clamp(int(400.0f / std::max(10.0f, depthCm)), 12, 50);

    // Pulso suave a 3 Hz
    const double phase  = static_cast<double>(cv::getTickCount()) / cv::getTickFrequency();
    const float  pulse  = 0.90f + 0.10f * static_cast<float>(std::sin(phase * 3.0));
    const int    radius = std::max(12, int(baseRadius * pulse));

    // Proximidad para modular alpha y velocidad de destellos
    const float proximity = std::clamp(20.0f / std::max(10.0f, depthCm), 0.15f, 1.0f);

    // Color reactivo a la distancia (BGR)
    cv::Scalar innerColor, midColor, outerColor;
    if (depthCm < 40.0f) {
        innerColor = {20, 80, 255};  midColor = {0, 40, 200};  outerColor = {0, 20, 140};
    } else if (depthCm < 80.0f) {
        innerColor = {20, 240, 100}; midColor = {0, 180, 50};  outerColor = {0, 100, 20};
    } else {
        innerColor = {240, 180, 20}; midColor = {180, 120, 0}; outerColor = {100, 60, 0};
    }

    // Relleno semitransparente — alpha moderado para no tapar la imagen
    cv::Mat overlay = image.clone();
    cv::circle(overlay, {cx, cy}, radius,             innerColor, -1, cv::LINE_AA);
    cv::circle(overlay, {cx, cy}, int(radius * 1.5f), midColor,    5, cv::LINE_AA);
    cv::circle(overlay, {cx, cy}, int(radius * 2.0f), outerColor,  3, cv::LINE_AA);

    const float alpha = 0.35f + 0.25f * proximity;   // máx 0.60 → nunca tapa todo
    cv::addWeighted(overlay, alpha, image, 1.0f - alpha, 0.0, image);

    // Anillos finos de brillo
    cv::circle(image, {cx, cy}, int(radius * 1.15f), cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    cv::circle(image, {cx, cy}, int(radius * 1.70f), cv::Scalar(200, 255, 255), 1, cv::LINE_AA);

    // Destellos orbitales
    const int    sparkCount = 6 + int(8.0f * proximity);
    const double spinSpeed  = 1.0 + 2.0 * proximity;
    for (int i = 0; i < sparkCount; ++i) {
        const double angle = phase * spinSpeed + (2.0 * CV_PI * i / sparkCount);
        const int    sr    = int(radius * 1.3f) + (i % 3) * 4;
        const int    sx    = cx + int(std::cos(angle) * sr);
        const int    sy    = cy + int(std::sin(angle) * sr);
        if (sx >= 0 && sx < image.cols && sy >= 0 && sy < image.rows)
            cv::circle(image, {sx, sy}, 1 + (i % 2), cv::Scalar(255, 255, 255), -1, cv::LINE_AA);
    }

    // Etiqueta de distancia
    const std::string label = std::to_string(static_cast<int>(depthCm + 0.5f)) + " cm";
    cv::putText(image, label,
                cv::Point(cx + radius + 6, cy - 4),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
}
