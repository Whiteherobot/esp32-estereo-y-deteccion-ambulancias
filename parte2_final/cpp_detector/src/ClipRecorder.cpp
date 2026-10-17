#include "ClipRecorder.hpp"

ClipRecorder::ClipRecorder(double fps, double seconds)
    : fps_(fps > 1.0 ? fps : 25.0),
      maxFrames_(static_cast<size_t>((fps > 1.0 ? fps : 25.0) * seconds)) {}

void ClipRecorder::push(const cv::Mat& frame) {
    buffer_.push_back(frame.clone());
    while (buffer_.size() > maxFrames_)
        buffer_.pop_front();
}

bool ClipRecorder::dump(const std::string& path) {
    if (buffer_.empty())
        return false;
    cv::Size sz = buffer_.front().size();
    cv::VideoWriter w(path, cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                      fps_, sz);
    if (!w.isOpened())
        return false;
    for (const auto& f : buffer_)
        w.write(f);
    w.release();
    return true;
}
