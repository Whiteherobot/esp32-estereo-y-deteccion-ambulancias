#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;
using namespace cv;
using namespace std;

struct Cli {
    string leftDir;
    string rightDir;
    string outFile = "../config/stereo_calib.yml";
    Size boardSize{9, 6};
    float squareSize = 0.0245f; // metros
};

bool parseArgs(int argc, char** argv, Cli& cli) {
    const String keys =
        "{help h usage ?||}"
        "{left||left images folder}"
        "{right||right images folder}"
        "{out|../config/stereo_calib.yml|output yml file}"
        "{bw|9|board width corners}"
        "{bh|6|board height corners}"
        "{sq|0.0245|square size in meters}";

    CommandLineParser parser(argc, argv, keys);
    if (parser.has("help") || !parser.has("left") || !parser.has("right")) {
        parser.printMessage();
        return false;
    }

    cli.leftDir = parser.get<string>("left");
    cli.rightDir = parser.get<string>("right");
    cli.outFile = parser.get<string>("out");
    cli.boardSize = Size(parser.get<int>("bw"), parser.get<int>("bh"));
    cli.squareSize = parser.get<float>("sq");

    return true;
}

vector<string> listImages(const string& dir) {
    vector<string> files;
    for (auto& p : fs::directory_iterator(dir)) {
        if (!p.is_regular_file()) continue;
        auto ext = p.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".bmp") {
            files.push_back(p.path().string());
        }
    }
    sort(files.begin(), files.end());
    return files;
}

vector<Point3f> makeBoard3D(Size board, float squareSize) {
    vector<Point3f> obj;
    for (int y = 0; y < board.height; ++y)
        for (int x = 0; x < board.width; ++x)
            obj.emplace_back(x * squareSize, y * squareSize, 0.0f);
    return obj;
}

bool detectCorners(const Mat& gray, Size board, vector<Point2f>& corners) {
    bool ok = findChessboardCornersSB(gray, board, corners);
    if (!ok) {
        ok = findChessboardCorners(gray, board, corners,
            CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE);
        if (ok) {
            cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
                         TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.01));
        }
    }
    return ok;
}

int main(int argc, char** argv) {
    Cli cli;
    if (!parseArgs(argc, argv, cli)) {
        cerr << "Uso: stereo_calibrate --left <dir> --right <dir> [--out ...] [--bw 9 --bh 6 --sq 0.0245]" << endl;
        return 1;
    }

    auto leftFiles = listImages(cli.leftDir);
    auto rightFiles = listImages(cli.rightDir);

    if (leftFiles.empty() || rightFiles.empty() || leftFiles.size() != rightFiles.size()) {
        cerr << "Error: carpetas vacias o numero de imagenes no coincide (left=" << leftFiles.size()
             << ", right=" << rightFiles.size() << ")" << endl;
        return 2;
    }

    vector<vector<Point2f>> imgPtsL, imgPtsR;
    vector<vector<Point3f>> objPts;
    auto obj = makeBoard3D(cli.boardSize, cli.squareSize);

    Size imageSize;

    for (size_t i = 0; i < leftFiles.size(); ++i) {
        Mat l = imread(leftFiles[i], IMREAD_COLOR);
        Mat r = imread(rightFiles[i], IMREAD_COLOR);
        if (l.empty() || r.empty()) continue;

        if (imageSize.empty()) imageSize = l.size();

        Mat gl, gr;
        cvtColor(l, gl, COLOR_BGR2GRAY);
        cvtColor(r, gr, COLOR_BGR2GRAY);

        vector<Point2f> cL, cR;
        bool fL = detectCorners(gl, cli.boardSize, cL);
        bool fR = detectCorners(gr, cli.boardSize, cR);

        if (fL && fR) {
            imgPtsL.push_back(cL);
            imgPtsR.push_back(cR);
            objPts.push_back(obj);
            cout << "[OK] pair " << i << " corners detected" << endl;
        } else {
            cout << "[SKIP] pair " << i << " corners not found" << endl;
        }
    }

    if (imgPtsL.size() < 10) {
        cerr << "Error: se requieren al menos 10 pares validos (ideal 20+)" << endl;
        return 3;
    }

    // 1) Calibración monocular por cámara (más estable)
    Mat K1 = Mat::eye(3, 3, CV_64F), D1;
    Mat K2 = Mat::eye(3, 3, CV_64F), D2;
    vector<Mat> rvecs1, tvecs1, rvecs2, tvecs2;

    // Modelo más estable para sensores pequeños (evita sobreajuste numérico)
    int monoFlags = CALIB_FIX_K3 | CALIB_FIX_K4 | CALIB_FIX_K5 | CALIB_FIX_K6;
    double rms1 = calibrateCamera(objPts, imgPtsL, imageSize, K1, D1, rvecs1, tvecs1, monoFlags);
    double rms2 = calibrateCamera(objPts, imgPtsR, imageSize, K2, D2, rvecs2, tvecs2, monoFlags);

    // 2) Estéreo con intrínsecos fijos
    Mat R, T, E, F;
    int stereoFlags = CALIB_FIX_INTRINSIC;
    double stereoRms = stereoCalibrate(
        objPts, imgPtsL, imgPtsR,
        K1, D1, K2, D2,
        imageSize,
        R, T, E, F,
        stereoFlags,
        TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 100, 1e-5)
    );

    Mat R1, R2, P1, P2, Q;
    Rect roi1, roi2;
    stereoRectify(K1, D1, K2, D2, imageSize, R, T, R1, R2, P1, P2, Q,
                  CALIB_ZERO_DISPARITY, 0, imageSize, &roi1, &roi2);

    fs::create_directories(fs::path(cli.outFile).parent_path());
    FileStorage fsw(cli.outFile, FileStorage::WRITE);
    fsw << "image_width" << imageSize.width;
    fsw << "image_height" << imageSize.height;
    fsw << "board_width" << cli.boardSize.width;
    fsw << "board_height" << cli.boardSize.height;
    fsw << "square_size_m" << cli.squareSize;
    fsw << "mono_rms_left" << rms1;
    fsw << "mono_rms_right" << rms2;
    fsw << "rms" << stereoRms;
    fsw << "K1" << K1 << "D1" << D1 << "K2" << K2 << "D2" << D2;
    fsw << "R" << R << "T" << T << "E" << E << "F" << F;
    fsw << "R1" << R1 << "R2" << R2 << "P1" << P1 << "P2" << P2 << "Q" << Q;
    fsw.release();

    cout << "\n=== CALIBRACION COMPLETA ===" << endl;
    cout << "Pares usados: " << imgPtsL.size() << endl;
    cout << "RMS mono left : " << rms1 << endl;
    cout << "RMS mono right: " << rms2 << endl;
    double baseline = norm(T);
    cout << "RMS stereo    : " << stereoRms << endl;
    cout << "Baseline (m)  : " << baseline << endl;
    cout << "Guardado en: " << cli.outFile << endl;

    return 0;
}
