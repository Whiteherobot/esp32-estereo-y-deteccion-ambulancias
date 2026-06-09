/**
 * VIEW — DisplayManager.cpp
 * Gestión de ventanas, HUD de métricas y controles interactivos (trackbars).
 */
#include "../../include/view/DisplayManager.hpp"
#include <cmath>
#include <string>

void DisplayManager::init() {
    cv::namedWindow(WIN_LEFT,  cv::WINDOW_NORMAL);
    cv::namedWindow(WIN_RIGHT, cv::WINDOW_NORMAL);
    cv::namedWindow(WIN_DISP,  cv::WINDOW_NORMAL);
    cv::resizeWindow(WIN_LEFT,  640, 480);
    cv::resizeWindow(WIN_RIGHT, 640, 480);
    cv::resizeWindow(WIN_DISP,  640, 480);

    // ── Ventana de controles ─────────────────────────────────────────────
    cv::namedWindow(WIN_CTRL, cv::WINDOW_NORMAL);
    cv::resizeWindow(WIN_CTRL, 520, 320);

    // ── SGBM ──────────────────────────────────────────────────────────
    cv::createTrackbar("numDisp  (x16 px)",   WIN_CTRL, &tb_numDispIdx_,    16);
    cv::createTrackbar("blockSize  (2n+3)",   WIN_CTRL, &tb_blockIdx_,       9);
    cv::createTrackbar("uniqueness",          WIN_CTRL, &tb_uniqueness_,    25);
    cv::createTrackbar("speckle window",      WIN_CTRL, &tb_speckleWin_,   200);
    cv::createTrackbar("speckle range",       WIN_CTRL, &tb_speckleRange_,  64);

    // ── CLAHE ─────────────────────────────────────────────────────────
    cv::createTrackbar("CLAHE  clip  (x0.1)", WIN_CTRL, &tb_clipX10_,       80);

    // ── WLS ───────────────────────────────────────────────────────────
    cv::createTrackbar("WLS  lambda  (x1000)",WIN_CTRL, &tb_wlsLambdaK_,    20);
    cv::createTrackbar("WLS  sigma  (x0.1)",  WIN_CTRL, &tb_wlsSigmaX10_,   30);

    // ── Kalman ────────────────────────────────────────────────────────
    cv::createTrackbar("Kalman  Q  (x1e-4)",  WIN_CTRL, &tb_kalmanQ_,      100);
    cv::createTrackbar("Kalman  R  (x1e-3)",  WIN_CTRL, &tb_kalmanR_,      100);

    // Asegurar valores mínimos válidos al arrancar
    if (tb_numDispIdx_  < 1)  cv::setTrackbarPos("numDisp  (x16 px)",    WIN_CTRL, 1);
    if (tb_clipX10_     < 1)  cv::setTrackbarPos("CLAHE  clip  (x0.1)", WIN_CTRL, 1);
    if (tb_wlsLambdaK_  < 1)  cv::setTrackbarPos("WLS  lambda  (x1000)",WIN_CTRL, 1);
    if (tb_wlsSigmaX10_ < 1)  cv::setTrackbarPos("WLS  sigma  (x0.1)",  WIN_CTRL, 1);
    if (tb_kalmanQ_     < 1)  cv::setTrackbarPos("Kalman  Q  (x1e-4)",  WIN_CTRL, 1);
    if (tb_kalmanR_     < 1)  cv::setTrackbarPos("Kalman  R  (x1e-3)",  WIN_CTRL, 1);
}

void DisplayManager::close() {
    cv::destroyAllWindows();
}

void DisplayManager::readConfig(ProcessorConfig& pc, double& kq, double& kr) const {
    const int numDispIdx  = std::max(1, tb_numDispIdx_);
    const int blockIdx    = std::max(0, tb_blockIdx_);

    pc.numDisp       = ((numDispIdx * 16 + 15) / 16) * 16;   // múltiplo de 16
    pc.blockSize     = blockIdx * 2 + 3;                       // impar ≥ 3
    pc.minDisp       = 0;
    pc.uniquenessRatio   = tb_uniqueness_;
    pc.speckleWindowSize = tb_speckleWin_;
    pc.speckleRange      = std::max(1, tb_speckleRange_);
    pc.useCLAHE      = true;
    pc.useWLS        = true;
    pc.wlsLambda     = std::max(1, tb_wlsLambdaK_) * 1000.0;
    pc.wlsSigma      = std::max(1, tb_wlsSigmaX10_) / 10.0;
    // clipLimit se ajusta en reconfigure vía ProcessorConfig
    pc.claheClipLimit = std::max(1, tb_clipX10_) / 10.0;

    kq = std::max(1, tb_kalmanQ_) * 1e-4;
    kr = std::max(1, tb_kalmanR_) * 1e-3;
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

void DisplayManager::updateControlsTitle(const ProcessorConfig& pc, double kq, double kr) const {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "numDisp=%dpx  block=%dpx  uniq=%d  spkW=%d  spkR=%d  |  "
        "CLAHE=%.1f  |  WLS(λ=%.0f σ=%.1f)  |  Q=%.4f  R=%.3f",
        pc.numDisp, pc.blockSize, pc.uniquenessRatio,
        pc.speckleWindowSize, pc.speckleRange,
        pc.claheClipLimit, pc.wlsLambda, pc.wlsSigma,
        kq, kr);
    cv::setWindowTitle(WIN_CTRL, buf);
}

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
