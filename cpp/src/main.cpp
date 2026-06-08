/**
 * main.cpp — Punto de entrada del sistema de visión estéreo con AR
 * ================================================================
 * Universidad Politécnica Salesiana — Computación / Visión Artificial
 * Proyecto Integrador Parte I: Visión Estéreo y Realidad Aumentada
 * Autores: Michael Lata · Jordy Romero
 *
 * Uso:
 *   ./stereo_measure <calib.yml> <fuente_izq> <fuente_der> [opciones]
 *
 * Ejemplos:
 *   # Con ESP32-CAM via HTTP (modo producción):
 *   ./stereo_measure ../config/stereo_calib.yml \
 *       http://172.20.10.2/capture http://172.20.10.3/capture
 *
 *   # Con webcams locales (modo prueba):
 *   ./stereo_measure ../config/stereo_calib.yml 0 1
 *
 * Opciones:
 *   --num-disp  N   Número de disparidades (múltiplo de 16, default: 128)
 *   --block-sz  N   Tamaño de bloque SGBM (impar, default: 5)
 *   --no-clahe      Desactiva preprocesamiento CLAHE
 *   --no-wls        Desactiva posprocesamiento WLS
 *   --kalman-q  V   Ruido de proceso del filtro Kalman (default: 0.001)
 *   --kalman-r  V   Ruido de medición del filtro Kalman (default: 0.01)
 */

#include "../include/model/StereoCalibration.hpp"
#include "../include/model/CameraSource.hpp"
#include "../include/controller/StereoController.hpp"

#include <iostream>
#include <string>

static void printUsage(const char* prog) {
    std::cerr
        << "Uso: " << prog << " <calib.yml> <izq_src> <der_src> [opciones]\n"
        << "  --num-disp  N    disparidades SGBM (múltiplo de 16)\n"
        << "  --block-sz  N    bloque SGBM (impar)\n"
        << "  --no-clahe       desactiva CLAHE\n"
        << "  --no-wls         desactiva WLS\n"
        << "  --kalman-q  V    ruido de proceso Kalman\n"
        << "  --kalman-r  V    ruido de medición Kalman\n";
}

int main(int argc, char** argv) {
    if (argc < 4) { printUsage(argv[0]); return 1; }

    const std::string calibPath = argv[1];
    const std::string leftSrc   = argv[2];
    const std::string rightSrc  = argv[3];

    // --- Parseo de opciones ---
    ControllerConfig cfg;

    for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--num-disp"  && i + 1 < argc) cfg.processor.numDisp  = std::stoi(argv[++i]);
        else if (arg == "--block-sz" && i + 1 < argc) cfg.processor.blockSize = std::stoi(argv[++i]);
        else if (arg == "--no-clahe")                  cfg.processor.useCLAHE  = false;
        else if (arg == "--no-wls")                    cfg.processor.useWLS    = false;
        else if (arg == "--kalman-q" && i + 1 < argc) cfg.kalmanProcessNoise   = std::stod(argv[++i]);
        else if (arg == "--kalman-r" && i + 1 < argc) cfg.kalmanMeasurementNoise = std::stod(argv[++i]);
        else { std::cerr << "Opción desconocida: " << arg << "\n"; printUsage(argv[0]); return 1; }
    }

    // --- MODELO: cargar calibración ---
    // Pasar 640×480 para que escale correctamente desde la calibración 320×240
    StereoCalibration calib;
    if (!calib.load(calibPath, 640, 480)) return 2;

    // --- MODELO: crear fuentes de cámara ---
    // createCameraSource detecta automáticamente si es HTTP o local
    auto leftCam  = createCameraSource(leftSrc);
    auto rightCam = createCameraSource(rightSrc);

    // --- CONTROLADOR: inicializar y ejecutar ---
    StereoController controller;
    if (!controller.init(calib, std::move(leftCam), std::move(rightCam), cfg))
        return 3;

    return controller.run();
}
