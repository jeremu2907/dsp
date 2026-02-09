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
    };
}