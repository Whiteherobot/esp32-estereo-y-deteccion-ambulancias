#pragma once
#include <opencv2/opencv.hpp>

// Vista: superpone en el frame las métricas vitales que exige la rúbrica
// (uso de RAM del proceso, FPS de procesamiento y confianza de la detección).
class Hud {
public:
    // Dibuja el HUD sobre `frame` (in-place).
    static void draw(cv::Mat& frame, double fps, double ramMb,
                     double confidence, bool detected);

    // Lee el uso de memoria residente del proceso en MB (Linux /proc).
    static double processRamMb();
};
