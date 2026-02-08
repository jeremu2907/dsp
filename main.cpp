#include <cstdlib>
#include <thread>

#include "pch.hpp"

#include "Sdr/LimeSdrMini2.hpp"

int main()
{
    Sdr::LimeSdrMini2 limeSdr;
    try
    {
        double frequency = 59e6;
        double bandwidth = 30e6;
        double gain = 30;

        limeSdr.configure(frequency, bandwidth, gain);

        limeSdr.run();
        std::this_thread::sleep_for(std::chrono::seconds(600));
    }
    catch (std::runtime_error e)
    {
        limeSdr.stop();
        LOG(SOAPY_SDR_FATAL, "FATAL RUNTIME ERROR: %s", e.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        limeSdr.stop();
        LOG(SOAPY_SDR_FATAL, "FATAL UNEXPECTED ERROR");
        return EXIT_FAILURE;
    }

    limeSdr.stop();
    LOG(SOAPY_SDR_INFO, "PROGRAM EXIT SUCCESS");
    return EXIT_SUCCESS;
}