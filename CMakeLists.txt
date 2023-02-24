cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(videoSrtEdit LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "d:/javaAndApp/Qt/6.4.2/mingw_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 COMPONENTS Widgets Multimedia REQUIRED) # Qt COMPONENTS
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY	${CMAKE_CURRENT_SOURCE_DIR}/bin)

include_directories(include)

link_directories(lib)

aux_source_directory(./src srcs)
aux_source_directory(./src/widget widgets)
aux_source_directory(./src/common common)

# Specify MSVC UTF-8 encoding   
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(${PROJECT_NAME}
    # WIN32 # If you need a terminal for debug, please comment this statement 
    ${srcs} ${widgets} ${common}
) 
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Multimedia) # Qt5 Shared Library

target_link_libraries(${PROJECT_NAME} PRIVATE 
    SDL2/SDL2
    ffmpeg/avcodec
    ffmpeg/avdevice
    ffmpeg/avfilter
    ffmpeg/avformat
    ffmpeg/avutil
    ffmpeg/postproc
    ffmpeg/swresample
    ffmpeg/swscale
)