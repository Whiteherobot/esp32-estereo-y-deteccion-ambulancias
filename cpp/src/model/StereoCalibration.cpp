/**
 * MODEL — StereoCalibration.cpp
 * Carga del archivo .yml y cálculo de mapas de rectificación.
 *
 * NOTA sobre escalado:
 *   La calibración fue realizada a 320×240 (QVGA), pero las cámaras capturan
 *   a 640×480 (VGA) cuando tienen PSRAM. Si la resolución del archivo difiere
 *   de targetW×targetH, las matrices K, P y Q se escalan proporcionalmente
 *   para que los mapas de rectificación sean correctos a la resolución real.
 */
#include "../../include/model/StereoCalibration.hpp"
#include <iostream>

bool StereoCalibration::load(const std::string& path, int targetW, int targetH) {
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        std::cerr << "[Calibración] No se pudo abrir: " << path << std::endl;
        return false;
    }

    int w = 0, h = 0;
    fs["image_width"]  >> w;
    fs["image_height"] >> h;

    cv::Mat K1, D1, K2, D2, R1, R2, P1, P2;
    fs["K1"] >> K1;  fs["D1"] >> D1;
    fs["K2"] >> K2;  fs["D2"] >> D2;
    fs["R1"] >> R1;  fs["R2"] >> R2;
    fs["P1"] >> P1;  fs["P2"] >> P2;
    fs["Q"]  >> Q;
    fs.release();

    // Si no se especifica target, usar la resolución del archivo
    if (targetW <= 0) targetW = w;
    if (targetH <= 0) targetH = h;

    // Escalar matrices si la resolución objetivo difiere de la calibrada
    if (targetW != w || targetH != h) {
        const double sx = static_cast<double>(targetW) / w;
        const double sy = static_cast<double>(targetH) / h;
        std::cout << "[Calibración] Escalando " << w << "×" << h
                  << " → " << targetW << "×" << targetH
                  << " (sx=" << sx << " sy=" << sy << ")" << std::endl;

        // K intrínseca: escalar fx, fy, cx, cy
        K1.at<double>(0,0) *= sx;  K1.at<double>(0,2) *= sx;
        K1.at<double>(1,1) *= sy;  K1.at<double>(1,2) *= sy;
        K2.at<double>(0,0) *= sx;  K2.at<double>(0,2) *= sx;
        K2.at<double>(1,1) *= sy;  K2.at<double>(1,2) *= sy;

        // P (nueva proyección rectificada): escalar fx, fy, cx, cy y Tx
        P1.at<double>(0,0) *= sx;  P1.at<double>(0,2) *= sx;
        P1.at<double>(1,1) *= sy;  P1.at<double>(1,2) *= sy;
        P2.at<double>(0,0) *= sx;  P2.at<double>(0,2) *= sx;
        P2.at<double>(1,1) *= sy;  P2.at<double>(1,2) *= sy;
        P2.at<double>(0,3) *= sx;  // Tx en px escala con la resolución

        // Q: las componentes en coordenadas de píxel escalan con la resolución.
        // Q[2,3]=f, Q[0,3]=-cx, Q[1,3]=-cy escalan. Q[3,2]=1/Tx_metros NO escala.
        Q.at<double>(0,3) *= sx;   // -cx
        Q.at<double>(1,3) *= sy;   // -cy
        Q.at<double>(2,3) *= sx;   // f (escala con el eje X por simetría estéreo)
    }

    width  = targetW;
    height = targetH;

    cv::initUndistortRectifyMap(K1, D1, R1, P1, {targetW, targetH}, CV_32FC1, map1x, map1y);
    cv::initUndistortRectifyMap(K2, D2, R2, P2, {targetW, targetH}, CV_32FC1, map2x, map2y);

    std::cout << "[Calibración] Cargada. Resolución efectiva: "
              << targetW << "×" << targetH << std::endl;
    return true;
}
