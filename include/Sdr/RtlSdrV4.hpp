#pragma once

#include <vector>

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
    class RtlSdrV4 : public SdrBase
    {
    public:
        inline static const double GAIN_MHZ = 30.0e6;
        inline static const double BANDWIDTH_MHZ = 2.4e6;
        inline static const double SAMPLE_RATE_MHZ = 3.2e6;

        RtlSdrV4();

        void run() override;

        void processThreadWithRoundRobin();

        void setFrequencies(const std::vector<double>& frequencies);

    private:
        std::vector<double> m_frequencies;
    };
}