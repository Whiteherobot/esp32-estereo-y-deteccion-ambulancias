#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// Modelo: detector clásico HOG + SVM lineal de ambulancias, usado como
// DISPARADOR A NIVEL DE FRAME (no localización). Carga el vector (w|b) y los
// parámetros HOG exportados por training/06_export_detector.py
// (models/hog_detector.yml) y clasifica el frame COMPLETO: letterbox a la
// ventana de entrenamiento -> HOG.compute -> score = w·features + b.
// score > threshold  =>  el frame contiene una ambulancia.
class AmbulanceDetector {
public:
    // Carga el .yml exportado desde Python. Lanza si falla.
    explicit AmbulanceDetector(const std::string& detectorYml);

    // Score con signo del frame (BGR o gris). Mayor = más 'ambulancia';
    // el sesgo b ya está incluido, así que el umbral natural es 0.
    double score(const cv::Mat& frame) const;

    cv::Size winSize() const { return winSize_; }

private:
    // Replica EXACTA del letterbox de training/01_collect.py: reescala
    // conservando aspecto y rellena con negro centrado hasta winSize_.
    cv::Mat letterbox(const cv::Mat& img) const;

    cv::HOGDescriptor hog_;
    std::vector<float> weights_;   // w (tamaño = descriptorSize)
    float bias_ = 0.f;             // b
    cv::Size winSize_;
};
