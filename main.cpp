#include <cstdlib>
#include <thread>

#include "pch.hpp"

#include "Sdr/RtlSdrV4.hpp"
#include "Sdr/LimeSdrMini2.hpp"

int main()
{
    try
    {
        // Sdr::RtlSdrV4 rtlSdr;
        // rtlSdr.setFrequencies({460e6});

        Sdr::LimeSdrMini2 limeSdr;
        limeSdr.configure(460e6, 30e6, 0);

        // rtlSdr.run();
        limeSdr.run();

        std::this_thread::sleep_for(std::chrono::seconds(180));

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