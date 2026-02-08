#pragma once

#include <atomic>

namespace Dsp
{
    class PowerSpectralDensity;
}

namespace SoapySDR
{
    class Device;
}

namespace Sdr
{
    class LimeSdrMini2
    {
    public:
        LimeSdrMini2();
        ~LimeSdrMini2();

        void configure(double frequency,
                       double bandwidth,
                       double gain);

        void run();

        void stop();

    private:
        std::atomic<bool> m_running;
        std::unique_ptr<SoapySDR::Device> m_device;
        std::unique_ptr<Dsp::PowerSpectralDensity> m_psd;

        double m_gain = 0.0f;
        double m_frequency = 0.0f;
        double m_bandwidth = 0.0f;
        double m_sampleRate = 0.0f;
    };
}