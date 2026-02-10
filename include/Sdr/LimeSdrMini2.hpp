#pragma once

#include "SdrBase.hpp"

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
    class LimeSdrMini2 : public SdrBase
    {
    public:
        LimeSdrMini2();

        void processThread() override;

        void configure(double frequency,
                       double bandwidth,
                       double gain,
                       double sampleRate = -9999) override;

    private:
        std::unique_ptr<Dsp::PowerSpectralDensity> m_psd;
        std::unique_ptr<Dsp::AnomalyDetection> m_anomDet;
    };
}