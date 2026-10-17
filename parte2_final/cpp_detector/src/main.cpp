// main.cpp — App de escritorio: detector clásico de ambulancias (HOG+SVM) +
// disparador que envía imagen y clip de 5 s al servicio Python (YOLO + Telegram).
//
// Uso:
//   ./ambulance_detector --video trafico.mp4 --model ../../models/hog_detector.yml \
//                        --endpoint http://127.0.0.1:8000/alert
//
// Controles:  q = salir
#include "VideoSource.hpp"
#include "AmbulanceDetector.hpp"
#include "Hud.hpp"
#include "ClipRecorder.hpp"
#include "TelegramApiClient.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

static std::string arg(int argc, char** argv, const std::string& key,
                       const std::string& def) {
    for (int i = 1; i + 1 < argc; ++i)
        if (key == argv[i]) return argv[i + 1];
    return def;
}

static bool flag(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i)
        if (key == argv[i]) return true;
    return false;
}

int main(int argc, char** argv) {
    const std::string videoSpec = arg(argc, argv, "--video", "0");
    const std::string modelPath = arg(argc, argv, "--model",
                                      "../../models/hog_detector.yml");
    const std::string endpoint = arg(argc, argv, "--endpoint",
                                     "http://127.0.0.1:8000/alert");
    // Umbral sobre el score del SVM (w·f + b). 0.0 = frontera natural del
    // clasificador (recall ~98.6% / falso-positivo ~0% en validación).
    const double scoreThresh = std::stod(arg(argc, argv, "--thresh", "0.0"));
    const int trigFrames = std::stoi(arg(argc, argv, "--trigger-frames", "5"));
    const double cooldownS = std::stod(arg(argc, argv, "--cooldown", "15"));
    const bool headless = flag(argc, argv, "--headless");
    const int maxFrames = std::stoi(arg(argc, argv, "--max-frames", "0"));
    const std::string saveAnnotated = arg(argc, argv, "--save-annotated", "");

    try {
        VideoSource source(videoSpec);
        AmbulanceDetector detector(modelPath);
        ClipRecorder recorder(source.fps(), 5.0);
        TelegramApiClient api(endpoint);

        cv::Mat frame;
        int consecutive = 0;
        auto lastSent = std::chrono::steady_clock::now() -
                        std::chrono::seconds(static_cast<long>(cooldownS));
        auto tPrev = std::chrono::steady_clock::now();
        double fps = 0.0;

        std::cout << "Detector iniciado. Fuente=" << videoSpec
                  << (headless ? "  [headless]" : "") << "\n";

        long frameNo = 0;
        double lastRam = 0.0;
        bool saveAnnotatedDone = false;
        while (source.read(frame)) {
            ++frameNo;
            recorder.push(frame);

            // Clasificación a nivel de frame (disparador clásico).
            double s = detector.score(frame);
            bool detected = s > scoreThresh;
            // Confianza legible 0-1 (logística del score) para HUD/API.
            double conf = 1.0 / (1.0 + std::exp(-s));
            consecutive = detected ? consecutive + 1 : 0;

            // Copia limpia (sin HUD ni caja) para enviar como "imagen original"
            // al bot; el box/HUD se dibujan solo para la visualización.
            cv::Mat cleanFrame;
            if (detected) cleanFrame = frame.clone();

            // --- FPS ---
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - tPrev).count();
            tPrev = now;
            if (dt > 0) fps = 0.9 * fps + 0.1 * (1.0 / dt);

            lastRam = Hud::processRamMb();
            Hud::draw(frame, fps, lastRam, conf, detected);

            // Guardar una captura anotada (box + HUD) para documentación.
            if (!saveAnnotated.empty() && detected && !saveAnnotatedDone) {
                cv::imwrite(saveAnnotated, frame);
                std::cout << "  [captura anotada] " << saveAnnotated << "\n";
                saveAnnotatedDone = true;
            }

            // --- disparador con debounce + cooldown ---
            double sinceSent =
                std::chrono::duration<double>(now - lastSent).count();
            if (consecutive >= trigFrames && sinceSent >= cooldownS) {
                std::cout << "[TRIGGER] Ambulancia confirmada (score=" << s
                          << " conf=" << conf << "). Enviando alerta...\n";
                cv::imwrite("/tmp/amb_frame.jpg", cleanFrame);  // original limpio
                recorder.dump("/tmp/amb_clip.mp4");
                bool ok = api.sendAlert("/tmp/amb_frame.jpg",
                                        "/tmp/amb_clip.mp4", conf);
                std::cout << (ok ? "  → enviado OK\n" : "  → fallo al enviar\n");
                // El envío es síncrono y puede tardar decenas de segundos
                // (YOLO segmenta el clip). Reiniciamos el cooldown DESPUÉS del
                // envío para no re-disparar de inmediato al volver.
                lastSent = std::chrono::steady_clock::now();
                tPrev = lastSent;          // no contaminar el FPS con el envío
                consecutive = 0;
            }

            if (headless) {
                if (frameNo % 30 == 0)
                    std::cout << "  frame " << frameNo
                              << "  score=" << s
                              << "  fps=" << fps
                              << "  ram=" << lastRam << "MB"
                              << (detected ? "  [AMBULANCIA]" : "") << "\n";
                if (maxFrames > 0 && frameNo >= maxFrames) break;
            } else {
                cv::imshow("Detector de Ambulancias — HOG+SVM", frame);
                if (cv::waitKey(1) == 'q')
                    break;
            }
        }
        std::cout << "Fin. Frames procesados=" << frameNo
                  << "  RAM final=" << lastRam << "MB  FPS(EMA)=" << fps << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
