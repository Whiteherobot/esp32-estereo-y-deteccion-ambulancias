/**
 * MODEL — StereoCalibration.hpp
 * ==============================
 * Carga y almacena los parámetros de calibración estéreo.
 *
 * Rol en MVC:
 *   MODELO puro: lee el archivo .yml generado por stereo_calibrate y expone
 *   los mapas de rectificación y la matriz Q al resto del sistema.
 *   No realiza procesamiento de imágenes ni dibuja nada.
 *
 * Parámetros que almacena:
 *   map1x, map1y  — mapas de rectificación para la cámara izquierda
 *   map2x, map2y  — mapas de rectificación para la cámara derecha
 *   Q             — matriz de reproyección 4×4 (disparidad → espacio 3D)
 *   width, height — resolución nominal de calibración
 *
 * Autor  : Michael Lata
 * Materia: Visión por Computador — Proyecto Integrador
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class StereoCalibration {
public:
    /**
     * Carga el archivo de calibración .yml y precalcula los mapas de rectificación.
     * @param path  Ruta al archivo stereo_calib.yml
     * @return      true si se cargó correctamente
     */
    // targetW/targetH: resolución real de las cámaras. -1 = usar la del archivo.
    bool load(const std::string& path, int targetW = -1, int targetH = -1);

    // Mapas de rectificación (usados con cv::remap en cada frame)
    cv::Mat map1x, map1y;   // cámara izquierda
    cv::Mat map2x, map2y;   // cámara derecha

    // Matriz de reproyección: (x, y, disparidad) → (X, Y, Z) en metros
    cv::Mat Q;

    int width  = 0;
    int height = 0;

    bool isLoaded() const { return width > 0 && height > 0; }
};
