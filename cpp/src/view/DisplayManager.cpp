/**
 * VIEW — DisplayManager.cpp
 * Gestión de ventanas y HUD de métricas en pantalla.
 */
#include "../../include/view/DisplayManager.hpp"
#include <cmath>
#include <string>

void DisplayManager::init() {
    cv::namedWindow(WIN_LEFT,  cv::WINDOW_NORMAL);
    cv::namedWindow(WIN_RIGHT, cv::WINDOW_NORMAL);
    cv::namedWindow(WIN_DISP,  cv::WINDOW_NORMAL);
    // Tamaño inicial 1:1 con la resolución VGA para evitar interpolación visible
    cv::resizeWindow(WIN_LEFT,  640, 480);
    cv::resizeWindow(WIN_RIGHT, 640, 480);
    cv::resizeWindow(WIN_DISP,  640, 480);
}

void DisplayManager::close() {
    cv::destroyAllWindows();
}

void DisplayManager::show(cv::Mat& leftDisplay,
                           const cv::Mat& rightRect,
                           const cv::Mat& disparity,
                           const HUDData& hud)
{
    drawHUD(leftDisplay, hud);

    cv::Mat disp8, dispColor;
    cv::normalize(disparity, disp8, 0, 255, cv::NORM_MINMAX, CV_8U);
    cv::applyColorMap(disp8, dispColor, cv::COLORMAP_TURBO);

    cv::imshow(WIN_LEFT,  leftDisplay);
    cv::imshow(WIN_RIGHT, rightRect);
    cv::imshow(WIN_DISP,  dispColor);
}

// Fondo oscuro + texto de métricas superpuesto sobre la imagen izquierda.
// Usa ROI directo en lugar de clonar la imagen completa.
void DisplayManager::drawHUD(cv::Mat& img, const HUDData& hud) {
    const int roiW = std::min(310, img.cols - 6);
    const int roiH = std::min(92,  img.rows - 4);
    cv::Mat roi = img(cv::Rect(6, 4, roiW, roiH));
    roi *= 0.45;

    auto txt = [&](const std::string& s, int y, cv::Scalar color) {
        cv::putText(img, s, {12, y}, cv::FONT_HERSHEY_SIMPLEX, 0.50, color, 1, cv::LINE_AA);
    };

    txt("FPS: " + std::to_string(static_cast<int>(hud.fps + 0.5)), 22, {180, 255, 100});

    if (std::isfinite(hud.zRaw) && hud.zRaw > 0) {
        txt("Z raw:    " + std::to_string(static_cast<int>(hud.zRaw    * 100)) + " cm", 42, {0, 200, 255});
        txt("Z Kalman: " + std::to_string(static_cast<int>(hud.zKalman * 100)) + " cm", 62, {0, 255, 200});
    } else {
        txt("Z: calculando...", 42, {120, 120, 120});
    }

    txt(std::string("CLAHE:") + (hud.claheOn ? "ON" : "OFF") +
        "  WLS:" + (hud.wlsOn ? "ON" : "OFF"), 82, {180, 180, 180});

    if (hud.recording) {
        cv::putText(img, "REC",
                    {img.cols - 68, 22},
                    cv::FONT_HERSHEY_SIMPLEX, 0.60,
                    cv::Scalar(0, 0, 230), 2, cv::LINE_AA);
        cv::circle(img, {img.cols - 78, 16}, 6, cv::Scalar(0, 0, 230), -1, cv::LINE_AA);
    }
}
