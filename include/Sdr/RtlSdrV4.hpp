#pragma once

#include <map>
#include <vector>

#include "SdrBase.hpp"

#include "Dsp/AnomalyDetection.hpp"
#include "Dsp/PowerSpectralDensity.hpp"

#include "DataStructure/CircularLinkedList.hpp"

namespace SoapySDR
{
    class Device;
}

namespace Model
{
    struct SdrRoundRobinConfig;
}

namespace Sdr
{
    class RtlSdrV4 : public SdrBase
    {
    public:
        inline static const double BANDWIDTH_HZ = 2.4e6;
        inline static const double SAMPLE_RATE_HZ = 3.2e6;

        RtlSdrV4();

        void processThread() override;

        void setFrequencies(const std::vector<double> &frequencies);

        void configure(double frequency,
                       double bandwidth,
                       double gain = GAIN_DBI,
                       double sampleRate = -9999) override;

    private:
        Ds::CircularLinkedList<Model::SdrRoundRobinConfig> m_configList;
    };
}