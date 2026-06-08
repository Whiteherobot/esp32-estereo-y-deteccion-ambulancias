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
    /** Crea las tres ventanas de OpenCV. */
    void init();

    /** Destruye todas las ventanas. */
    void close();

    /**
     * Muestra los tres frames en sus ventanas correspondientes.
     * Dibuja el HUD sobre leftDisplay antes de mostrarlo.
     *
     * @param leftDisplay  Imagen izquierda con AR ya dibujado (se le añade HUD)
     * @param rightRect    Imagen derecha rectificada
     * @param disparity    Mapa de disparidad CV_32F (se colorea aquí)
     * @param hud          Datos para el HUD
     */
    void show(cv::Mat& leftDisplay, const cv::Mat& rightRect,
              const cv::Mat& disparity, const HUDData& hud);

private:
    static void drawHUD(cv::Mat& img, const HUDData& hud);

    const std::string WIN_LEFT  = "Camara Izquierda (Rectificada)";
    const std::string WIN_RIGHT = "Camara Derecha (Rectificada)";
    const std::string WIN_DISP  = "Mapa de Disparidad";
};
