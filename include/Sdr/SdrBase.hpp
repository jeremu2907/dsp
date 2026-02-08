#pragma once

#include <atomic>
#include <memory>

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
    class SdrBase
    {
    public:
        SdrBase(const std::string &driver);
        virtual ~SdrBase();

        virtual void run();

        void stop();

        double getGain() const;
        double getFrequency() const;
        double getBandwidth() const;
        double getSampleRate() const;

        void configure(double frequency,
                       double bandwidth,
                       double gain,
                       double sampleRate = -9999);

        virtual void processThread() = 0;

    protected:
        std::atomic<bool> m_running;
        std::unique_ptr<SoapySDR::Device> m_device;
        std::unique_ptr<Dsp::PowerSpectralDensity> m_psd;

        double m_gain = 0.0f;
        double m_frequency = 0.0f;
        double m_bandwidth = 0.0f;
        double m_sampleRate = 0.0f;
    };
}