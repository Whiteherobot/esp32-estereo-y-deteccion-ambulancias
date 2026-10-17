#include "AmbulanceDetector.hpp"
#include <stdexcept>
#include <numeric>

AmbulanceDetector::AmbulanceDetector(const std::string& detectorYml) {
    cv::FileStorage fs(detectorYml, cv::FileStorage::READ);
    if (!fs.isOpened())
        throw std::runtime_error("No se pudo abrir el detector: " + detectorYml);

    int w, h, block, stride, cell, nbins;
    fs["win_width"] >> w;
    fs["win_height"] >> h;
    fs["block"] >> block;
    fs["block_stride"] >> stride;
    fs["cell"] >> cell;
    fs["nbins"] >> nbins;

    // Python (06_export_detector.py) escribe el vector como matriz OpenCV
    // (numpy array -> nodo opencv-matrix), así que lo leemos como cv::Mat y
    // lo aplanamos, no como secuencia plana std::vector<float>.
    cv::Mat detMat;
    fs["detector"] >> detMat;
    fs.release();

    if (detMat.empty())
        throw std::runtime_error("Detector vacío en " + detectorYml);
    detMat = detMat.reshape(1, 1);          // fila única
    detMat.convertTo(detMat, CV_32F);
    std::vector<float> detector(detMat.begin<float>(), detMat.end<float>());

    winSize_ = cv::Size(w, h);
    hog_ = cv::HOGDescriptor(winSize_, cv::Size(block, block),
                             cv::Size(stride, stride), cv::Size(cell, cell),
                             nbins);

    // El vector exportado es [w (descriptorSize) | b]. Separamos pesos y sesgo.
    bias_ = detector.back();
    weights_.assign(detector.begin(), detector.end() - 1);

    const size_t expected = hog_.getDescriptorSize();
    if (weights_.size() != expected)
        throw std::runtime_error(
            "Dimensión del detector (" + std::to_string(weights_.size()) +
            ") no coincide con HOG (" + std::to_string(expected) + ")");
}

cv::Mat AmbulanceDetector::letterbox(const cv::Mat& img) const {
    const int W = winSize_.width, H = winSize_.height;
    const int ih = img.rows, iw = img.cols;
    const double scale = std::min(static_cast<double>(W) / iw,
                                  static_cast<double>(H) / ih);
    const int nw = static_cast<int>(iw * scale);
    const int nh = static_cast<int>(ih * scale);
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(nw, nh), 0, 0, cv::INTER_AREA);
    const int top = (H - nh) / 2, bottom = H - nh - top;
    const int left = (W - nw) / 2, right = W - nw - left;
    cv::Mat canvas;
    cv::copyMakeBorder(resized, canvas, top, bottom, left, right,
                       cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    return canvas;
}

double AmbulanceDetector::score(const cv::Mat& frame) const {
    cv::Mat gray;
    if (frame.channels() == 3)
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    else
        gray = frame;

    cv::Mat win = letterbox(gray);
    std::vector<float> feats;
    hog_.compute(win, feats);

    // score = w·features + b  (misma fórmula que LinearSVC.decision_function)
    double s = std::inner_product(weights_.begin(), weights_.end(),
                                  feats.begin(), 0.0);
    return s + bias_;
}
