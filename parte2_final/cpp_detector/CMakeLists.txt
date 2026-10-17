cmake_minimum_required(VERSION 3.16)
project(ambulance_detector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pedir solo los módulos usados evita arrastrar opencv_viz/opencv_hdf
# (que dependen de VTK/HDF5 y rompen el enlazado en algunas distros).
find_package(OpenCV REQUIRED COMPONENTS
    core imgproc imgcodecs videoio highgui objdetect ml)
find_package(CURL REQUIRED)

add_executable(ambulance_detector
    src/main.cpp
    src/VideoSource.cpp
    src/AmbulanceDetector.cpp
    src/Hud.cpp
    src/TelegramApiClient.cpp
    src/ClipRecorder.cpp
)

target_include_directories(ambulance_detector PRIVATE include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ambulance_detector PRIVATE ${OpenCV_LIBS} CURL::libcurl)
