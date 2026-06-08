cmake_minimum_required(VERSION 3.16)
project(stereo_esp32_cv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED
    COMPONENTS core imgproc calib3d highgui imgcodecs videoio ximgproc)
find_package(CURL    REQUIRED)
find_package(Threads REQUIRED)

set(OPENCV_MODULES
    opencv_core opencv_imgproc opencv_calib3d
    opencv_highgui opencv_imgcodecs opencv_videoio opencv_ximgproc)

# ------------------------------------------------------------------
# stereo_measure — aplicación principal (MVC)
# ------------------------------------------------------------------
add_executable(stereo_measure
    src/main.cpp
    # MODEL
    src/model/StereoCalibration.cpp
    src/model/StereoProcessor.cpp
    src/model/KalmanDepth.cpp
    src/model/CameraSource.cpp
    src/model/HandDetector.cpp
    # VIEW
    src/view/ARRenderer.cpp
    src/view/DisplayManager.cpp
    # CONTROLLER
    src/controller/StereoController.cpp
)

target_include_directories(stereo_measure PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(stereo_measure PRIVATE
    ${OPENCV_MODULES}
    ${CURL_LIBRARIES}
    Threads::Threads
)

# ------------------------------------------------------------------
# stereo_calibrate — herramienta de calibración offline (sin cambios)
# ------------------------------------------------------------------
add_executable(stereo_calibrate src/stereo_calibrate.cpp)
target_include_directories(stereo_calibrate PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(stereo_calibrate PRIVATE ${OPENCV_MODULES})
