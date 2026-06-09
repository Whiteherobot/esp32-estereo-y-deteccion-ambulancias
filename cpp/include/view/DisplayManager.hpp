/**
 * VIEW — DisplayManager.hpp
 * =========================
 * Gestión de ventanas y HUD (Head-Up Display) de métricas.
 *
 * Rol en MVC:
 *   VISTA: gestiona todas las ventanas de OpenCV y dibuja los datos de
 *   diagnóstico sobre la imagen. Recibe datos procesados del Controlador
 *   y los traduce en píxeles. No procesa imágenes ni filtra datos.
 *
 * Ventanas que maneja:
 *   "Camara Izquierda (Rectificada)" — imagen con AR y HUD superpuestos
 *   "Camara Derecha (Rectificada)"   — imagen derecha rectificada
 *   "Mapa de Disparidad"             — disparidad coloreada con COLORMAP_TURBO
 *
 * HUD que dibuja:
 *   - FPS del bucle principal
 *   - Z raw (mediana de parche) en centímetros
 *   - Z Kalman (suavizado) en centímetros
 *   - Estado de CLAHE y WLS (ON/OFF)
 *
 * Autor  : Michael Lata
 * Materia: Visión por Computador — Proyecto Integrador
 */
#pragma once

#include <opencv2/opencv.hpp>
#include "../model/StereoProcessor.hpp"
#include <string>

struct HUDData {
    double fps       = 0.0;
    float  zRaw      = 0.0f;    // Z cruda en metros
    double zKalman   = 0.0;     // Z filtrada en metros
    bool   claheOn   = false;
    bool   wlsOn     = false;
    bool   recording = false;   // indica si el logger CSV está activo
};

class DisplayManager {
public:
    /** Crea las ventanas de imagen y la ventana de controles con trackbars. */
    void init();

    /** Destruye todas las ventanas. */
    void close();

    /**
     * Muestra los tres frames en sus ventanas correspondientes.
     * Dibuja el HUD sobre leftDisplay antes de mostrarlo.
     */
    void show(cv::Mat& leftDisplay, const cv::Mat& rightRect,
              const cv::Mat& disparity, const HUDData& hud);

    /**
     * Lee los valores actuales de los trackbars y los convierte a
     * ProcessorConfig y parámetros de Kalman listos para usar.
     */
    void readConfig(ProcessorConfig& pc, double& kq, double& kr) const;

    /** Actualiza el título de la ventana de controles con los valores reales. */
    void updateControlsTitle(const ProcessorConfig& pc, double kq, double kr) const;

private:
    static void drawHUD(cv::Mat& img, const HUDData& hud);

    const std::string WIN_LEFT  = "Camara Izquierda (Rectificada)";
    const std::string WIN_RIGHT = "Camara Derecha (Rectificada)";
    const std::string WIN_DISP  = "Mapa de Disparidad";
    const std::string WIN_CTRL  = "Controles - SGBM / Kalman";

    // Almacén entero de cada trackbar (OpenCV exige int*)
    mutable int tb_numDispIdx_   = 12;  // ×16 → 192
    mutable int tb_blockIdx_     = 1;   // ×2+3 → 5
    mutable int tb_uniqueness_   = 10;
    mutable int tb_speckleWin_   = 100;
    mutable int tb_speckleRange_ = 32;
    mutable int tb_clipX10_      = 20;  // ÷10 → 2.0
    mutable int tb_wlsLambdaK_   = 8;   // ×1000 → 8000
    mutable int tb_wlsSigmaX10_  = 15;  // ÷10 → 1.5
    mutable int tb_kalmanQ_      = 10;  // ×1e-4 → 0.001
    mutable int tb_kalmanR_      = 10;  // ×1e-3 → 0.01
};
