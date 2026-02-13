#pragma once

#include <atomic>
#include <memory>
#include <chrono>

namespace Dsp
{
    class PowerSpectralDensity;
    class AnomalyDetection;
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
        inline static const double GAIN_DBI = 0;

        SdrBase(const std::string &driver);
        virtual ~SdrBase();

        virtual void run();

        void stop();

        double getGain() const;
        double getFrequency() const;
        double getBandwidth() const;
        double getSampleRate() const;

        virtual void configure(double frequency,
                               double bandwidth,
                               double gain = GAIN_DBI,
                               double sampleRate = -9999);

        virtual void processThread() = 0;

    protected:
        inline static const long long TIME_BETWEEN_ROLLING_SAMPLE_COLLECT_MS = 10;
        inline static const long long TIME_BETWEEN_ROLLING_SAMPLE_DIST_PROCESS_MS = 10000;

        bool isTimeToCollectSample();
        bool isTimeToProcessSampleDistribution();

        std::atomic<bool> m_running;
        std::unique_ptr<SoapySDR::Device> m_device;

        double m_gain = 0.0f;
        double m_frequency = 0.0f;
        double m_bandwidth = 0.0f;
        double m_sampleRate = 0.0f;

        std::string m_driver;

        std::chrono::time_point<std::chrono::system_clock> m_currentTimeS;
        std::chrono::time_point<std::chrono::system_clock> m_lastSampleCollectedS;
        std::chrono::time_point<std::chrono::system_clock> m_lastDistributionProcessedS;
    };
}