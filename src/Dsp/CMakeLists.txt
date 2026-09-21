add_library(Dsp
    PowerSpectralDensity.cpp
    AnomalyDetection.cpp
)

find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)
pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3f)

target_include_directories(Dsp
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(Dsp
    PUBLIC
    PkgConfig::FFTW3
)