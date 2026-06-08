/**
 * MODEL — StereoProcessor.hpp
 * ============================
 * Pipeline de procesamiento estéreo: rectificación → CLAHE → SGBM → WLS → 3D.
 *
 * Rol en MVC:
 *   MODELO de procesamiento: recibe pares de imágenes crudas y produce el
 *   mapa de disparidad y el mapa de puntos 3D. No muestra nada por pantalla.
 *
 * Pipeline interno:
 *   1. Rectificación (cv::remap con los mapas de StereoCalibration)
 *   2. CLAHE         — ecualización adaptativa de histograma (mejora contraste local)
 *   3. SGBM          — Semi-Global Block Matching (cálculo de disparidad)
 *   4. WLS filter    — Weighted Least Squares (rellena huecos, suaviza bordes)
 *   5. Reproyección  — cv::reprojectImageTo3D con matriz Q (disparidad → metros)
 *
 * Autor  : Michael Lata
 * Materia: Visión por Computador — Proyecto Integrador
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/ximgproc.hpp>
#include "StereoCalibration.hpp"

struct ProcessorConfig {
    int    minDisp       = 0;
    int    numDisp       = 192;   // múltiplo de 16; cubre hasta ~23cm con VGA+baseline 6.7cm
    int    blockSize     = 5;     // impar, ≥ 3
    bool   useCLAHE     = true;
    bool   useWLS       = true;
    double wlsLambda    = 8000.0;
    double wlsSigma     = 1.5;
};

struct ProcessorResult {
    cv::Mat leftRect;    // imagen izquierda rectificada (BGR)
    cv::Mat rightRect;   // imagen derecha rectificada (BGR)
    cv::Mat disparity;   // mapa de disparidad CV_32F en unidades de disparidad
    cv::Mat points3D;    // mapa 3D CV_32FC3 con coordenadas (X,Y,Z) en metros
    bool    claheActive;
    bool    wlsActive;
};

class StereoProcessor {
public:
    /**
     * Inicializa el procesador con la calibración y la configuración dada.
     * Crea los objetos SGBM, CLAHE y WLS internamente.
     */
    bool init(const StereoCalibration& calib, const ProcessorConfig& cfg);

    /**
     * Procesa un par de imágenes crudas y llena el resultado.
     * @param rawLeft   Frame crudo de la cámara izquierda (cualquier resolución)
     * @param rawRight  Frame crudo de la cámara derecha
     * @param result    Estructura con todas las salidas del pipeline
     */
    void process(const cv::Mat& rawLeft, const cv::Mat& rawRight,
                 ProcessorResult& result);

    /**
     * Extrae la mediana de Z en un parche de radio `r` alrededor de (cx, cy).
     * Filtra valores no finitos y fuera del rango [0.05, 10] metros.
     * Retorna NaN si no hay suficientes puntos válidos.
     */
    static float medianDepthAt(const cv::Mat& points3D, int cx, int cy, int r = 4);

private:
    const StereoCalibration* calib_ = nullptr;
    int width_  = 0;
    int height_ = 0;

    cv::Ptr<cv::StereoSGBM>                    sgbm_;
    cv::Ptr<cv::CLAHE>                          clahe_;
    cv::Ptr<cv::ximgproc::DisparityWLSFilter>   wlsFilter_;
    cv::Ptr<cv::StereoMatcher>                  rightMatcher_;

    bool useCLAHE_ = false;
    bool useWLS_   = false;
};
