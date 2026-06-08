/**
 * MODEL — CameraSource.hpp
 * ========================
 * Abstracción de fuente de cámara: HTTP snapshot (ESP32-CAM) o local (webcam/video).
 *
 * Rol en MVC:
 *   MODELO de datos de entrada: oculta el mecanismo de captura al Controlador.
 *   El Controlador llama a read() sin saber si la fuente es una URL HTTP o
 *   un dispositivo local.
 *
 * Implementaciones:
 *   HttpCameraSource  — descarga JPEGs de /capture en un hilo dedicado (ESP32-CAM)
 *   LocalCameraSource — usa cv::VideoCapture (webcam, archivo, MJPEG local)
 *
 * Patrón de diseño: Strategy + Factory (createFromUrl)
 *
 * Autor  : Michael Lata
 * Materia: Visión por Computador — Proyecto Integrador
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Interfaz base
// ---------------------------------------------------------------------------

class CameraSource {
public:
    virtual ~CameraSource() = default;

    /** Obtiene el último frame disponible. Retorna false si aún no hay frame. */
    virtual bool read(cv::Mat& out) = 0;

    /** Nombre identificador de la fuente (para logs). */
    virtual const std::string& name() const = 0;
};

// ---------------------------------------------------------------------------
// Fuente HTTP (ESP32-CAM con endpoint /capture)
// ---------------------------------------------------------------------------

/**
 * HttpCameraSource
 * ----------------
 * Lanza un hilo productor que descarga continuamente frames JPEG desde `url`.
 * El hilo usa libcurl internamente. El bucle principal lee con read() sin
 * bloquearse, obteniendo siempre el frame más reciente.
 */
class HttpCameraSource : public CameraSource {
public:
    explicit HttpCameraSource(std::string url, int timeoutMs = 2500);
    ~HttpCameraSource() override;

    bool read(cv::Mat& out) override;
    const std::string& name() const override { return name_; }

    int framesReceived() const { return framesReceived_.load(); }
    int errorCount()     const { return errors_.load(); }

private:
    void workerLoop();
    static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

    std::string        url_;
    std::string        name_;
    int                timeoutMs_;
    std::thread        worker_;
    std::atomic<bool>  running_{true};
    std::mutex         mutex_;
    cv::Mat            latestFrame_;
    bool               hasFrame_ = false;
    std::atomic<int>   framesReceived_{0};
    std::atomic<int>   errors_{0};
};

// ---------------------------------------------------------------------------
// Fuente local (webcam / archivo de video)
// ---------------------------------------------------------------------------

class LocalCameraSource : public CameraSource {
public:
    explicit LocalCameraSource(const std::string& source);
    bool read(cv::Mat& out) override;
    const std::string& name() const override { return name_; }
    bool isOpened() const { return cap_.isOpened(); }

private:
    cv::VideoCapture cap_;
    std::string      name_;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

/** Crea la fuente adecuada según si el string comienza por "http://". */
std::unique_ptr<CameraSource> createCameraSource(const std::string& source,
                                                  int httpTimeoutMs = 2500);
