#pragma once

#include "Dsp/AnomalyDetection.hpp"
#include "Dsp/PowerSpectralDensity.hpp"

namespace Model
{
    struct SdrRoundRobinConfig
    {
        bool anomaly;
        double frequency;
        double bandwidth;
        Dsp::PowerSpectralDensity psd;
        Dsp::AnomalyDetection anomDet;

        bool operator==(const SdrRoundRobinConfig &rhs)
        {
            return frequency == rhs.frequency;
        }
    };
}
