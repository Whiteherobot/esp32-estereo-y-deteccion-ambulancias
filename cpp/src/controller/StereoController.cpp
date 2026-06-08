/**
 * CONTROLLER — StereoController.cpp
 * Bucle principal: conecta Modelo con Vista y gestiona eventos de usuario.
 *
 * Teclas disponibles:
 *   H       — activa/desactiva seguimiento de mano (energy ball sobre la mano)
 *   R       — iniciar/detener grabación CSV
 *   C       — reiniciar filtro Kalman central
 *   Q/ESC   — salir
 */
#include "../../include/controller/StereoController.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

using namespace cv;

// ---------------------------------------------------------------------------
// Logger CSV — helpers
// ---------------------------------------------------------------------------

void StereoController::openCsvLog() {
    // Nombre con timestamp: depth_log_YYYYMMDD_HHMMSS.csv
    std::time_t t = std::time(nullptr);
    std::tm tm    = *std::localtime(&t);
    std::ostringstream fname;
    fname << "../../data/measurements/depth_log_"
          << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".csv";

    csvFile_.open(fname.str());
    if (!csvFile_.is_open()) {
        std::cerr << "[Logger] No se pudo abrir: " << fname.str() << std::endl;
        return;
    }
    csvFile_ << "frame,elapsed_ms,z_raw_cm,z_kalman_cm,fps\n";
    logFrame_  = 0;
    recording_ = true;
    std::cout << "[Logger] Grabando en: " << fname.str() << std::endl;
}

void StereoController::closeCsvLog() {
    if (csvFile_.is_open()) {
        csvFile_.flush();
        csvFile_.close();
    }
    recording_ = false;
    std::cout << "[Logger] CSV cerrado. " << logFrame_ << " frames guardados." << std::endl;
}

// ---------------------------------------------------------------------------
// Inicialización
// ---------------------------------------------------------------------------

bool StereoController::init(const StereoCalibration& calib,
                              std::unique_ptr<CameraSource> leftCam,
                              std::unique_ptr<CameraSource> rightCam,
                              const ControllerConfig& cfg)
{
    width_  = calib.width;
    height_ = calib.height;

    // Inicializar el procesador estéreo (SGBM + CLAHE + WLS)
    if (!processor_.init(calib, cfg.processor)) return false;

    // Inicializar filtros de Kalman con los parámetros de ruido configurados
    kalmanCenter_ = KalmanDepth(cfg.kalmanProcessNoise, cfg.kalmanMeasurementNoise);
    kalmanClick_  = KalmanDepth(cfg.kalmanProcessNoise, cfg.kalmanMeasurementNoise);

    leftCam_  = std::move(leftCam);
    rightCam_ = std::move(rightCam);

    // Esperar primer frame de ambas cámaras antes de continuar
    std::cout << "[Controller] Esperando primer frame de ambas cámaras..." << std::endl;
    Mat probe;
    int attempts = 0;
    while ((!leftCam_->read(probe) || !rightCam_->read(probe)) && attempts < 80) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++attempts;
    }
    if (attempts >= 80) {
        std::cerr << "[Controller] Tiempo de espera agotado. Verificar conexión." << std::endl;
        return false;
    }
    std::cout << "[Controller] Ambas cámaras listas." << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// Callback de mouse (estático, requerido por OpenCV)
// ---------------------------------------------------------------------------

void StereoController::onMouseCallback(int event, int x, int y, int, void* userdata) {
    if (event != EVENT_LBUTTONDOWN) return;
    auto* self = reinterpret_cast<StereoController*>(userdata);
    self->clickX_ = x;
    self->clickY_ = y;
    self->kalmanClick_.reset();   // reiniciar filtro al cambiar de punto
    std::cout << "[Mouse] Pixel seleccionado: (" << x << ", " << y << ")" << std::endl;
}

// ---------------------------------------------------------------------------
// Bucle principal
// ---------------------------------------------------------------------------

int StereoController::run() {
    display_.init();

    // Registrar callback de mouse en la ventana de la cámara izquierda
    setMouseCallback("Camara Izquierda (Rectificada)", onMouseCallback, this);

    std::cout << "[Controller] Click en la ventana izquierda para medir. Q/ESC para salir."
              << std::endl;
    std::cout << "[Controller] H = activar/desactivar energy ball sobre la mano." << std::endl;
    std::cout << "[Controller] R = iniciar/detener grabacion CSV." << std::endl;
    std::cout << "[Controller] C = reiniciar filtro Kalman central." << std::endl;

    // Variables de FPS real (contando frames únicos de la cámara izquierda)
    auto   tStart  = std::chrono::steady_clock::now();
    auto   tFpsPrev = tStart;
    double fps     = 0.0;
    int    frameNo = 0;

    ProcessorResult result;

    while (true) {
        // ------------------------------------------------------------------
        // 1. MODELO — Adquisición del par estéreo
        // ------------------------------------------------------------------
        Mat rawLeft, rawRight;
        if (!leftCam_->read(rawLeft) || !rightCam_->read(rawRight)) {
            waitKey(10);
            continue;
        }

        // ------------------------------------------------------------------
        // 2. MODELO — Pipeline estéreo (rectificación + CLAHE + SGBM + WLS + 3D)
        // ------------------------------------------------------------------
        processor_.process(rawLeft, rawRight, result);

        // ------------------------------------------------------------------
        // 3. MODELO — Estimación de profundidad Z con Kalman
        // ------------------------------------------------------------------

        // Profundidad en el CENTRO de la imagen (AR automático)
        const int   cenX      = width_  / 2;
        const int   cenY      = height_ / 2;
        const float zCenterRaw = StereoProcessor::medianDepthAt(result.points3D, cenX, cenY, 5);
        const double zCenterK  = std::isfinite(zCenterRaw)
                                 ? kalmanCenter_.update(zCenterRaw)
                                 : std::numeric_limits<double>::quiet_NaN();

        // Profundidad en el punto de CLIC del usuario
        constexpr float  kNaNf = std::numeric_limits<float>::quiet_NaN();
        constexpr double kNaN  = std::numeric_limits<double>::quiet_NaN();
        float  zClickRaw = kNaNf;
        double zClickK   = kNaN;
        if (clickX_ >= 0 && clickY_ >= 0) {
            zClickRaw = StereoProcessor::medianDepthAt(result.points3D, clickX_, clickY_, 3);
            if (std::isfinite(zClickRaw)) {
                zClickK = kalmanClick_.update(zClickRaw);
            }
        }

        // ------------------------------------------------------------------
        // 4. VISTA — Renderizado AR sobre la imagen izquierda
        // ------------------------------------------------------------------
        Mat leftDisplay = result.leftRect.clone();

        // CAPA 1: pintura de campo de profundidad (manipulación directa de píxeles)
        // Tinta cada píxel según su Z relativo al foco medido.
        if (std::isfinite(zCenterK) && zCenterK > 0)
            ARRenderer::applyDepthField(leftDisplay, result.points3D,
                                        static_cast<float>(zCenterK));

        // CAPA 2: aura animada en el centro y en el clic
        if (std::isfinite(zCenterK) && zCenterK > 0)
            ARRenderer::draw(leftDisplay, static_cast<float>(zCenterK), cenX, cenY);

        if (clickX_ >= 0 && std::isfinite(zClickK) && zClickK > 0) {
            ARRenderer::draw(leftDisplay, static_cast<float>(zClickK), clickX_, clickY_);
            const std::string zTxt = "Z: " +
                std::to_string(static_cast<int>(zClickK * 100)) + " cm";
            putText(leftDisplay, zTxt,
                    Point(clickX_ + 10, clickY_ - 10),
                    FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 200), 2, LINE_AA);
        }

        // ------------------------------------------------------------------
        // 5. VISTA — Mostrar ventanas con HUD
        // ------------------------------------------------------------------
        ++frameNo;
        const auto   tNow     = std::chrono::steady_clock::now();
        const double elapsedMs = std::chrono::duration<double, std::milli>(tNow - tStart).count();

        // FPS real: contar frames únicos entregados por la cámara izquierda
        if (frameNo % 20 == 0) {
            auto* httpLeft = dynamic_cast<HttpCameraSource*>(leftCam_.get());
            if (httpLeft) {
                const int  newFrames = httpLeft->framesReceived() - prevFramesLeft_;
                const double seg     = std::chrono::duration<double>(tNow - tFpsPrev).count();
                if (seg > 0.0) fps   = newFrames / seg;
                prevFramesLeft_      = httpLeft->framesReceived();
            } else {
                // Fuente local: contar iteraciones del loop
                const double seg = std::chrono::duration<double>(tNow - tFpsPrev).count();
                if (seg > 0.0) fps = 20.0 / seg;
            }
            tFpsPrev = tNow;
        }

        // Escribir fila CSV si hay grabación activa y Z válida
        if (recording_ && std::isfinite(zCenterRaw) && std::isfinite(zCenterK)) {
            csvFile_ << std::fixed << std::setprecision(2)
                     << ++logFrame_              << ","
                     << elapsedMs               << ","
                     << (zCenterRaw * 100.0f)   << ","
                     << (zCenterK  * 100.0)     << ","
                     << fps                     << "\n";
        }

        HUDData hud;
        hud.fps       = fps;
        hud.zRaw      = zCenterRaw;
        hud.zKalman   = zCenterK;
        hud.claheOn   = result.claheActive;
        hud.wlsOn     = result.wlsActive;
        hud.recording = recording_;

        display_.show(leftDisplay, result.rightRect, result.disparity, hud);

        // ------------------------------------------------------------------
        // 6. Manejo de teclas
        // ------------------------------------------------------------------
        const int key = waitKey(1);
        if (key == 27 || key == 'q' || key == 'Q') {
            if (recording_) closeCsvLog();
            break;
        }
        if (key == 'r' || key == 'R') {
            if (recording_) closeCsvLog();
            else            openCsvLog();
        }
        if (key == 'c' || key == 'C') {
            kalmanCenter_.reset();
            std::cout << "[Controller] Kalman central reiniciado." << std::endl;
        }
    }

    display_.close();
    return 0;
}
