#include "Hud.hpp"
#include <fstream>
#include <sstream>
#include <string>

double Hud::processRamMb() {
    // VmRSS desde /proc/self/status (Linux).
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line.substr(6));
            long kb = 0;
            iss >> kb;
            return kb / 1024.0;
        }
    }
    return 0.0;
}

void Hud::draw(cv::Mat& frame, double fps, double ramMb,
               double confidence, bool detected) {
    const cv::Scalar bg(0, 0, 0);
    cv::rectangle(frame, cv::Rect(0, 0, 260, 92), bg, cv::FILLED);

    auto put = [&](const std::string& t, int y, cv::Scalar col) {
        cv::putText(frame, t, cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX,
                    0.55, col, 1, cv::LINE_AA);
    };
    std::ostringstream s1, s2, s3;
    s1 << "FPS: " << cv::format("%.1f", fps);
    s2 << "RAM: " << cv::format("%.0f MB", ramMb);
    s3 << "Conf: " << cv::format("%.2f", confidence);
    put(s1.str(), 24, cv::Scalar(255, 255, 255));
    put(s2.str(), 48, cv::Scalar(255, 255, 255));
    put(s3.str(), 72, cv::Scalar(0, 255, 255));

    if (detected) {
        // El detector es a nivel de frame (clasifica la escena completa), así
        // que la caja marca "vehículo objetivo presente en la escena" con un
        // recuadro inset. La localización fina la hace YOLO en el bot.
        const int mx = frame.cols / 12, my = frame.rows / 12;
        const cv::Rect box(mx, my, frame.cols - 2 * mx, frame.rows - 2 * my);
        const cv::Scalar red(0, 0, 255);
        cv::rectangle(frame, box, red, 3, cv::LINE_AA);

        // Etiqueta tipo detector en la BASE de la caja (lejos del HUD de la
        // esquina superior, que muestra FPS/RAM/Conf): "AMBULANCIA 0.87".
        std::ostringstream tag;
        tag << "AMBULANCIA " << cv::format("%.2f", confidence);
        int base = 0;
        const cv::Size ts = cv::getTextSize(tag.str(),
            cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, &base);
        const int by = box.y + box.height - ts.height - 8;
        cv::rectangle(frame,
            cv::Rect(box.x, by, ts.width + 8, ts.height + 8),
            red, cv::FILLED);
        cv::putText(frame, tag.str(),
                    cv::Point(box.x + 4, by + ts.height + 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
    }
}
