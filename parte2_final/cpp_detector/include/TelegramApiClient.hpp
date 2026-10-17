#pragma once
#include <string>

// Controlador de salida: envía la alerta (imagen + clip de video + confianza)
// al servicio Python (FastAPI) mediante una petición HTTP multipart con libcurl.
// Reutiliza el enfoque de libcurl empleado en la Parte I.
class TelegramApiClient {
public:
    // endpoint: p.ej. "http://127.0.0.1:8000/alert"
    explicit TelegramApiClient(std::string endpoint);

    // Envía imagen + (opcional) video + confianza. Devuelve true si HTTP 2xx.
    bool sendAlert(const std::string& imagePath,
                   const std::string& videoPath,
                   double confidence);

private:
    std::string endpoint_;
};
