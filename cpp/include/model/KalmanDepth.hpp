/**
 * MODEL — KalmanDepth.hpp
 * =======================
 * Filtro de Kalman 1D escalar para estabilizar la lectura de profundidad Z.
 *
 * Rol en MVC:
 *   Forma parte del MODELO: encapsula el estado matemático del filtro y
 *   la lógica de actualización. No sabe nada de cámaras ni de pantalla.
 *
 * Fundamento:
 *   Las lecturas de disparidad de SGBM fluctúan frame a frame (~±2-5 px)
 *   debido a ruido de iluminación. El filtro de Kalman modela Z como un
 *   proceso de posición constante y reduce esa varianza sin introducir
 *   retardo perceptible (a diferencia de una media móvil simple).
 *
 * Parámetros clave:
 *   processNoise (q)      — qué tan rápido puede cambiar Z en la realidad.
 *                           Mayor q = responde más rápido a movimientos reales.
 *   measurementNoise (r)  — cuánto ruido tiene cada medición de SGBM.
 *                           Mayor r = suavizado más agresivo.
 *
 * Autor  : Michael Lata
 * Materia: Visión por Computador — Proyecto Integrador
 */
#pragma once

class KalmanDepth {
public:
    KalmanDepth(double processNoise = 1e-3, double measurementNoise = 1e-2);

    /**
     * Actualiza el filtro con una nueva medición y retorna la estimación suavizada.
     * @param measurement  Valor Z crudo en metros (de medianZInPatch)
     * @return             Estimación filtrada por Kalman en metros
     */
    double update(double measurement);

    /** Reinicia el filtro (útil al cambiar de punto de medición). */
    void reset();

    /** Actualiza los parámetros de ruido en vivo (desde trackbars). */
    void setNoise(double q, double r) { q_ = q; r_ = r; }

private:
    double q_;            // ruido de proceso
    double r_;            // ruido de medición
    bool   initialized_;
    double estimate_;     // estimación actual x̂
    double covariance_;   // covarianza del error P
};
