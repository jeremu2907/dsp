#pragma once

#include <deque>

namespace Dsp
{
    class AnomalyDetection
    {
    public:
        inline static const size_t MAX_SIZE = 256;

        bool isReady() const;

        void pushSample(double sample);

        bool isAnomaly(double sample, double alpha = 0.00000001);

    private:
        double cdf(double x);
        void processDistribution();

        std::deque<double> m_samples;
        double m_mean = 0.0f;
        double m_var = 0.0f;
        bool m_ready = false;
    };
}