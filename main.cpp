#include <cstdlib>
#include <thread>

#include "pch.hpp"

#include "Sdr/RtlSdrV4.hpp"

int main()
{
    Sdr::RtlSdrV4 rtlSdr;
    try
    {
        rtlSdr.configure(460e6, Sdr::RtlSdrV4::BANDWIDTH_MHZ, Sdr::RtlSdrV4::GAIN_MHZ);
        rtlSdr.run();
        std::this_thread::sleep_for(std::chrono::seconds(600));
    }
    catch (std::runtime_error e)
    {
        rtlSdr.stop();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        LOG(SOAPY_SDR_FATAL, "FATAL RUNTIME ERROR: %s", e.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        rtlSdr.stop();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        LOG(SOAPY_SDR_FATAL, "FATAL UNEXPECTED ERROR");
        return EXIT_FAILURE;
    }

    rtlSdr.stop();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    LOG(SOAPY_SDR_INFO, "PROGRAM EXIT SUCCESS");
    return EXIT_SUCCESS;
}