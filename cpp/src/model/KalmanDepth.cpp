/**
 * MODEL — KalmanDepth.cpp
 * Implementación del filtro de Kalman 1D escalar para profundidad Z.
 */
#include "../../include/model/KalmanDepth.hpp"

KalmanDepth::KalmanDepth(double processNoise, double measurementNoise)
    : q_(processNoise), r_(measurementNoise),
      initialized_(false), estimate_(0.0), covariance_(1.0) {}

double KalmanDepth::update(double measurement) {
    if (!initialized_) {
        initialized_ = true;
        estimate_    = measurement;
        covariance_  = 1.0;
        return estimate_;
    }

    // Predicción: la incertidumbre crece con el tiempo (ruido de proceso q)
    covariance_ += q_;

    // Ganancia de Kalman: balancea confianza en predicción vs. medición
    const double gain = covariance_ / (covariance_ + r_);

    // Actualización: corrige la estimación con la nueva medición
    estimate_   += gain * (measurement - estimate_);
    covariance_  = (1.0 - gain) * covariance_;

    return estimate_;
}

void KalmanDepth::reset() {
    initialized_ = false;
    estimate_    = 0.0;
    covariance_  = 1.0;
}
