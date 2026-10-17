#pragma once
#include <opencv2/opencv.hpp>
#include <deque>
#include <string>

// Modelo: mantiene un buffer circular de los últimos N segundos de video para
// poder guardar un clip de >=5 s alrededor del instante de la detección.
class ClipRecorder {
public:
    ClipRecorder(double fps, double seconds = 5.0);

    // Agrega un frame al buffer (descarta los más antiguos).
    void push(const cv::Mat& frame);

    // Vuelca el buffer actual a un MP4. Devuelve true si escribió algo.
    bool dump(const std::string& path);

private:
    double fps_;
    size_t maxFrames_;
    std::deque<cv::Mat> buffer_;
};
