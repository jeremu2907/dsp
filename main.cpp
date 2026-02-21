#include <cstdlib>
#include <thread>

#include <SoapySDR/Constants.h>

#include "pch.hpp"

#include "Sdr/RtlSdrV4.hpp"
#include "Sdr/LimeSdrMini2.hpp"

int main()
{
    try
    {
        // Sdr::RtlSdrV4 rtlSdr;
        // rtlSdr.setFrequencies({461e6});
        // rtlSdr.setFrequencies({460e6, 470e6, 480e6, 490e6, 500e6});
        // rtlSdr.run();

        Sdr::LimeSdrMini2 limeSdr;
        limeSdr.setMode(SOAPY_SDR_TX);
        limeSdr.setTxFrequencies({30e6, 35e6, 37e6, 41e6, 42e6, 44e6, 47e6, 50e6, 51e6, 52e6, 53e6, 59e6, 60e6, 66e6});
        limeSdr.run();
        
        std::this_thread::sleep_for(std::chrono::seconds(6000));
        
        // rtlSdr.stop();
        limeSdr.stop();

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