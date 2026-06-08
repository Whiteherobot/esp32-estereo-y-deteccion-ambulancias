/**
 * MODEL — StereoProcessor.cpp
 * Pipeline: rectificación → CLAHE → SGBM → WLS → reproyección 3D.
 */
#include "../../include/model/StereoProcessor.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using namespace cv;

bool StereoProcessor::init(const StereoCalibration& calib, const ProcessorConfig& cfg) {
    calib_  = &calib;
    width_  = calib.width;
    height_ = calib.height;

    // Normalizar parámetros SGBM
    int numDisp  = std::max(16, cfg.numDisp);
    numDisp = ((numDisp + 15) / 16) * 16;          // debe ser múltiplo de 16
    int blockSz  = std::max(3, cfg.blockSize);
    if (blockSz % 2 == 0) ++blockSz;               // debe ser impar

    // Crear el matcher SGBM.
    // P1 y P2 controlan la suavidad del mapa de disparidad resultante.
    // La relación P2 = 4*P1 es la recomendación estándar de Hirschmuller.
    sgbm_ = StereoSGBM::create(cfg.minDisp, numDisp, blockSz);
    sgbm_->setP1(8  * 3 * blockSz * blockSz);
    sgbm_->setP2(32 * 3 * blockSz * blockSz);
    sgbm_->setMode(StereoSGBM::MODE_SGBM_3WAY);

    useCLAHE_ = cfg.useCLAHE;
    if (useCLAHE_) {
        // clipLimit=2.0: limita la amplificación máxima del histograma.
        // tileSize=8×8: divide la imagen en teselas para ecualización local.
        clahe_ = createCLAHE(2.0, Size(8, 8));
    }

    useWLS_ = cfg.useWLS;
    if (useWLS_) {
        try {
            // El matcher derecho calcula disparidad R→L, requerida por el filtro WLS.
            rightMatcher_ = ximgproc::createRightMatcher(sgbm_);
            wlsFilter_    = ximgproc::createDisparityWLSFilter(sgbm_);
            wlsFilter_->setLambda(cfg.wlsLambda);
            wlsFilter_->setSigmaColor(cfg.wlsSigma);
        } catch (...) {
            std::cerr << "[StereoProcessor] WLS no disponible. Desactivado." << std::endl;
            useWLS_ = false;
        }
    }

    std::cout << "[StereoProcessor] SGBM minDisp=" << cfg.minDisp
              << " numDisp=" << numDisp << " blockSize=" << blockSz << std::endl;
    std::cout << "[StereoProcessor] CLAHE=" << (useCLAHE_ ? "ON" : "OFF")
              << "  WLS=" << (useWLS_ ? "ON" : "OFF") << std::endl;
    return true;
}

void StereoProcessor::process(const Mat& rawLeft, const Mat& rawRight,
                               ProcessorResult& result) {
    // --- Paso 1: Rectificación ---
    // cv::remap aplica los mapas precalculados de StereoCalibration para
    // eliminar distorsión de lente y alinear las líneas epipolares.
    cv::remap(rawLeft,  result.leftRect,  calib_->map1x, calib_->map1y, INTER_LINEAR);
    cv::remap(rawRight, result.rightRect, calib_->map2x, calib_->map2y, INTER_LINEAR);

    // Ajustar resolución si el frame no coincide con la calibración
    if (result.leftRect.cols  != width_ || result.leftRect.rows  != height_)
        resize(result.leftRect,  result.leftRect,  Size(width_, height_));
    if (result.rightRect.cols != width_ || result.rightRect.rows != height_)
        resize(result.rightRect, result.rightRect, Size(width_, height_));

    // --- Paso 2: Conversión a grises ---
    Mat grayL, grayR;
    cvtColor(result.leftRect,  grayL, COLOR_BGR2GRAY);
    cvtColor(result.rightRect, grayR, COLOR_BGR2GRAY);

    // --- Paso 3: CLAHE (preprocesamiento) ---
    // Mejora el contraste local para que SGBM encuentre correspondencias
    // en zonas con iluminación desigual o sombras.
    result.claheActive = useCLAHE_;
    if (useCLAHE_ && clahe_) {
        clahe_->apply(grayL, grayL);
        clahe_->apply(grayR, grayR);
    }

    // --- Paso 4: SGBM (cálculo de disparidad) ---
    // Retorna disparidad en formato Q16.4 (se divide entre 16 para obtener px).
    Mat disp16L, disp16R;
    sgbm_->compute(grayL, grayR, disp16L);

    // --- Paso 5: WLS (posprocesamiento) ---
    result.wlsActive = useWLS_;
    if (useWLS_ && rightMatcher_ && wlsFilter_) {
        // Disparidad derecha (R→L) necesaria para el filtro WLS
        rightMatcher_->compute(grayR, grayL, disp16R);
        Mat filtered;
        wlsFilter_->filter(disp16L, result.leftRect, filtered, disp16R);
        filtered.convertTo(result.disparity, CV_32F, 1.0 / 16.0);
    } else {
        disp16L.convertTo(result.disparity, CV_32F, 1.0 / 16.0);
    }

    // --- Paso 6: Rellenar zona inválida izquierda ---
    // SGBM deja ~numDisp columnas sin datos a la izquierda (búsqueda sin par).
    // Extendemos la primera columna válida hacia la izquierda para que el efecto
    // de distorsión cubra toda la imagen (corrección visual, no física).
    for (int y = 0; y < result.disparity.rows; ++y) {
        float* row = result.disparity.ptr<float>(y);
        float fill = 0.0f;
        // Buscar primer valor positivo en la fila
        for (int x = 0; x < result.disparity.cols; ++x) {
            if (row[x] > 0.5f) { fill = row[x]; break; }
        }
        if (fill <= 0.0f) continue;
        // Rellenar columnas inválidas a la izquierda con ese valor
        for (int x = 0; x < result.disparity.cols; ++x) {
            if (row[x] > 0.5f) break;
            row[x] = fill;
        }
    }

    // --- Paso 7: Reproyección 3D ---
    reprojectImageTo3D(result.disparity, result.points3D, calib_->Q, true);
}

float StereoProcessor::medianDepthAt(const Mat& points3D, int cx, int cy, int r) {
    const int x0 = std::max(0, cx - r);
    const int x1 = std::min(points3D.cols - 1, cx + r);
    const int y0 = std::max(0, cy - r);
    const int y1 = std::min(points3D.rows - 1, cy + r);

    std::vector<float> zVals;
    zVals.reserve((2 * r + 1) * (2 * r + 1));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float z = points3D.at<Vec3f>(y, x)[2];
            // Filtrar valores no finitos y fuera del rango plausible del sistema
            if (std::isfinite(z) && z > 0.05f && z < 10.0f) {
                zVals.push_back(z);
            }
        }
    }

    if (static_cast<int>(zVals.size()) < 4)
        return std::numeric_limits<float>::quiet_NaN();

    const size_t mid = zVals.size() / 2;
    std::nth_element(zVals.begin(), zVals.begin() + mid, zVals.end());
    return zVals[mid];
}
