#pragma once
#include <opencv2/opencv.hpp>
#include <string>

// Vista/Modelo de adquisición: abstrae el origen del video
// (archivo .mp4, webcam por índice, o URL de stream de la ESP32-CAM).
class VideoSource {
public:
    // spec: ruta de archivo, "0"/"1"... para webcam, o URL http.
    explicit VideoSource(const std::string& spec);
    bool read(cv::Mat& frame);
    double fps() const;

private:
    cv::VideoCapture cap_;
};
