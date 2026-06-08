/**
 * MODEL — CameraSource.cpp
 * Implementación de fuentes HTTP (ESP32-CAM) y local (VideoCapture).
 */
#include "../../include/model/CameraSource.hpp"

#include <curl/curl.h>
#include <chrono>
#include <iostream>
#include <thread>

// ---------------------------------------------------------------------------
// HttpCameraSource — captura con hilo dedicado y libcurl
// ---------------------------------------------------------------------------

size_t HttpCameraSource::curlWriteCallback(char* ptr, size_t size,
                                            size_t nmemb, void* userdata) {
    auto* buf = reinterpret_cast<std::vector<char>*>(userdata);
    const size_t total = size * nmemb;
    buf->insert(buf->end(), ptr, ptr + total);
    return total;
}

HttpCameraSource::HttpCameraSource(std::string url, int timeoutMs)
    : url_(std::move(url)), name_("HTTP:" + url_), timeoutMs_(timeoutMs)
{
    // Inicia el hilo productor al construir el objeto
    worker_ = std::thread(&HttpCameraSource::workerLoop, this);
}

HttpCameraSource::~HttpCameraSource() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
}

bool HttpCameraSource::read(cv::Mat& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasFrame_) return false;
    latestFrame_.copyTo(out);
    return true;
}

/**
 * workerLoop — hilo productor
 * ----------------------------
 * Descarga JPEG de /capture en bucle continuo usando un handle de libcurl
 * reutilizable (evita overhead de conexión TCP por cada petición).
 */
void HttpCameraSource::workerLoop() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[CameraSource] Error al inicializar libcurl para " << url_ << std::endl;
        return;
    }

    std::vector<char> buffer;
    buffer.reserve(32 * 1024);  // reservar 32 KB

    // Configurar opciones fijas del handle
    curl_easy_setopt(curl, CURLOPT_URL,               url_.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,     curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,         &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,        static_cast<long>(timeoutMs_));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,          1L);  // seguro en multi-hilo

    while (running_.load()) {
        buffer.clear();

        if (curl_easy_perform(curl) != CURLE_OK) {
            errors_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        // Decodificar JPEG en memoria (sin escritura a disco)
        const cv::Mat raw(1, static_cast<int>(buffer.size()), CV_8UC1, buffer.data());
        cv::Mat frame = cv::imdecode(raw, cv::IMREAD_COLOR);

        if (frame.empty()) {
            errors_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        // Actualizar frame bajo protección de mutex
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latestFrame_ = std::move(frame);
            hasFrame_    = true;
        }
        framesReceived_.fetch_add(1, std::memory_order_relaxed);
    }

    curl_easy_cleanup(curl);
}

// ---------------------------------------------------------------------------
// LocalCameraSource — VideoCapture estándar
// ---------------------------------------------------------------------------

LocalCameraSource::LocalCameraSource(const std::string& source) {
    const bool isIndex = !source.empty() &&
                         std::all_of(source.begin(), source.end(), ::isdigit);
    if (isIndex) {
        cap_.open(std::stoi(source));
        name_ = "Local:dev" + source;
    } else {
        cap_.open(source);
        name_ = "Local:" + source;
    }

    if (!cap_.isOpened()) {
        std::cerr << "[CameraSource] No se pudo abrir: " << source << std::endl;
    }
}

bool LocalCameraSource::read(cv::Mat& out) {
    return cap_.read(out) && !out.empty();
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<CameraSource> createCameraSource(const std::string& source,
                                                   int httpTimeoutMs) {
    const bool isHttp = source.rfind("http://",  0) == 0 ||
                        source.rfind("https://", 0) == 0;
    if (isHttp) {
        return std::make_unique<HttpCameraSource>(source, httpTimeoutMs);
    }
    return std::make_unique<LocalCameraSource>(source);
}
