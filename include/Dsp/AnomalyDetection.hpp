#pragma once

#include <deque>

namespace Dsp
{
    class AnomalyDetection
    {
    public:
        inline static const size_t MAX_SIZE = 256;
        inline static const size_t CONSECUTIVE_COUNT = 10;
        inline static const double ALPHA = 0.001;
        inline static const double EMWA_ALPHA = .05;
        inline static const double CRITIAL_VALUE_FROM_ALPHA_AND_N_MINUS_ONE = 3.1225;

        double mean() const;
        double minSnrDb() const;
        double prevFilteredSample() const;
        bool isReady() const;

        void processDistribution();
        void pushSample(double sample);
        bool isAnomaly(double sample, double alpha = ALPHA);

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
        double m_prevFilteredSample = 0.0;
        double m_mean = 0.0;
        double m_sigma = 1.0;
        double m_nu = 0.0; 
        bool m_ready = false;
        bool m_anomaly = false;
        size_t m_consecutiveHighPower = 0;
        size_t m_consecutiveLowPower = 0;
    };
}