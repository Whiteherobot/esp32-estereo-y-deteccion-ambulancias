#include "VideoSource.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

VideoSource::VideoSource(const std::string& spec) {
    bool numeric = !spec.empty() &&
        std::all_of(spec.begin(), spec.end(), [](char c){ return std::isdigit(c); });
    if (numeric)
        cap_.open(std::stoi(spec));           // webcam por índice
    else
        cap_.open(spec);                      // archivo o URL de stream
    if (!cap_.isOpened())
        throw std::runtime_error("No se pudo abrir la fuente de video: " + spec);
}

bool VideoSource::read(cv::Mat& frame) {
    return cap_.read(frame);
}

double VideoSource::fps() const {
    double f = cap_.get(cv::CAP_PROP_FPS);
    return f > 1.0 ? f : 25.0;
}
