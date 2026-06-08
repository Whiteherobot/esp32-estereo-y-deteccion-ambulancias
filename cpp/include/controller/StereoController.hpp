/**
 * CONTROLLER — StereoController.hpp
 * ===================================
 * Orquestador del bucle principal: conecta el Modelo con la Vista.
 *
 * Rol en MVC:
 *   CONTROLADOR: recibe eventos del usuario (clic de mouse, tecla de salida),
 *   solicita frames al Modelo (CameraSource → StereoProcessor → KalmanDepth)
 *   y entrega los resultados a la Vista (ARRenderer → DisplayManager).
 *
 * Flujo por frame:
 *   1. Leer par estéreo de las dos CameraSource (hilos HTTP o VideoCapture)
 *   2. Pedir a StereoProcessor que compute rectificación + disparidad + 3D
 *   3. Extraer Z en el centro y en el punto de clic con medianDepthAt()
 *   4. Filtrar ambos Z con sus KalmanDepth respectivos
 *   5. (Modo H) HandDetector detecta la mano → energy ball sobre ella
 *   6. Pedir a ARRenderer que dibuje el efecto en esas posiciones
 *   7. Pasar todo a DisplayManager para mostrar en pantalla
 *   8. Calcular FPS y manejar teclas de salida (H=toggle mano, R=CSV, C=reset)
 *
 * Autores : Michael Lata · Jordy Romero
 * Materia : Visión por Computador — Proyecto Integrador
 */
#pragma once

#include "../model/StereoCalibration.hpp"
#include "../model/StereoProcessor.hpp"
#include "../model/KalmanDepth.hpp"
#include "../model/CameraSource.hpp"
#include "../model/HandDetector.hpp"
#include "../view/ARRenderer.hpp"
#include "../view/DisplayManager.hpp"

#include <fstream>
#include <memory>

struct ControllerConfig {
    ProcessorConfig processor;
    double kalmanProcessNoise      = 1e-3;
    double kalmanMeasurementNoise  = 1e-2;
};

class StereoController {
public:
    /**
     * Inicializa el controlador con la calibración y las fuentes de cámara.
     * @return false si algún componente no pudo inicializarse
     */
    bool init(const StereoCalibration& calib,
              std::unique_ptr<CameraSource> leftCam,
              std::unique_ptr<CameraSource> rightCam,
              const ControllerConfig& cfg);

    /**
     * Ejecuta el bucle principal hasta que el usuario presione Q o ESC.
     * @return 0 si terminó normalmente, código de error en otro caso
     */
    int run();

private:
    // Callback estático de mouse (OpenCV requiere función estática)
    static void onMouseCallback(int event, int x, int y, int flags, void* userdata);

    // --- Modelo ---
    StereoProcessor                processor_;
    KalmanDepth                    kalmanCenter_;   // filtro para el centro
    KalmanDepth                    kalmanClick_;    // filtro para el punto de clic
    std::unique_ptr<CameraSource>  leftCam_;
    std::unique_ptr<CameraSource>  rightCam_;

    // --- Vista ---
    DisplayManager display_;

    // --- Estado del usuario ---
    int  clickX_ = -1;
    int  clickY_ = -1;

    int width_  = 0;
    int height_ = 0;

    // --- Logger CSV ---
    bool          recording_ = false;
    int           logFrame_  = 0;
    std::ofstream csvFile_;

    void openCsvLog();
    void closeCsvLog();

    // --- FPS real (basado en frames únicos de la cámara) ---
    int    prevFramesLeft_  = 0;
    int    prevFramesRight_ = 0;
};
