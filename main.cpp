#include <cstdlib>
#include <thread>

#include "pch.hpp"

#include "Sdr/RtlSdrV4.hpp"
#include "Sdr/LimeSdrMini2.hpp"

int main()
{
    try
    {
        Sdr::RtlSdrV4 rtlSdr;
        rtlSdr.setFrequencies({461e6});
        // rtlSdr.setFrequencies({460e6, 470e6, 480e6, 490e6, 500e6, 510e6, 520e6, 530e6, 540e6, 550e6});
        rtlSdr.run();

        // Sdr::LimeSdrMini2 limeSdr;
        // limeSdr.configure(460e6, 30e6);
        // limeSdr.run();
        
        std::this_thread::sleep_for(std::chrono::seconds(6000));
        
        rtlSdr.stop();
        // limeSdr.stop();

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