#pragma once

#include <deque>

namespace Dsp
{
    class AnomalyDetection
    {
    public:
        inline static const size_t MAX_SIZE = 256;
        inline static const size_t CONSECUTIVE_COUNT = 10;

        double mean() const;
        bool isReady() const;

        void processDistribution();
        void pushSample(double sample);
        bool isAnomaly(double sample, double alpha = 0.01);

    private:
        double pdf(double x,
                   double x_0,
                   double sigma,
                   double nu);

        double cdf(double x,
                   double x_0,
                   double sigma,
                   double nu);

        void toFile(const char *fileName);

        std::deque<double> m_samples;
        double m_mean = 0.0;
        double m_sigma = 1.0;
        double m_nu = 0.0; 
        bool m_ready = false;
        bool m_anomaly = false;
        size_t m_consecutiveHighPower = 0;
        size_t m_consecutiveLowPower = 0;
    };
}