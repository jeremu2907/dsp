cmake_minimum_required(VERSION 3.16)
project(sdr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src/Dsp)
add_subdirectory(src/Sdr)

add_executable(sdr main.cpp)

target_include_directories(sdr
    PRIVATE
        ${SoapySDR_INCLUDE_DIRS}
        ${LimeSuite_INCLUDE_DIRS}
)

target_link_libraries(sdr
    PRIVATE
        Dsp
        Sdr
)
