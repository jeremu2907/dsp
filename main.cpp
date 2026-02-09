#include <cstdlib>
#include <thread>

#include "pch.hpp"

#include "Sdr/RtlSdrV4.hpp"
#include "Sdr/LimeSdrMini2.hpp"

int main()
{
    try
    {
        Sdr::RtlSdrV4 sdr;
        sdr.configure(314e6, 2.4e6, 0);
        sdr.run();
        std::this_thread::sleep_for(std::chrono::seconds(600));
        sdr.stop();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    catch (std::runtime_error e)
    {
        LOG(SOAPY_SDR_FATAL, "FATAL RUNTIME ERROR: %s", e.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        LOG(SOAPY_SDR_FATAL, "FATAL UNEXPECTED ERROR");
        return EXIT_FAILURE;
    }

    LOG(SOAPY_SDR_INFO, "PROGRAM EXIT SUCCESS");
    return EXIT_SUCCESS;
}