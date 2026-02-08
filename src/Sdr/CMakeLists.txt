add_library(Sdr
    SdrBase.cpp
    LimeSdrMini2.cpp
    RtlSdrV4.cpp
)

find_package(SoapySdr REQUIRED)
find_package(LimeSuite REQUIRED)

target_include_directories(Sdr
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(Sdr
    PUBLIC
        SoapySDR    #Uses Soapy SDR LOGGER accross project
    PRIVATE
        LimeSuite
)
