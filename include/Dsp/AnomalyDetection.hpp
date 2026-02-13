#pragma once

#include <deque>

namespace Dsp
{
    class AnomalyDetection
    {
    public:
        inline static const size_t MAX_SIZE = 256;
        inline static const size_t CONSECUTIVE_COUNT = 10;

        bool isReady() const;
        void processDistribution();
        void pushSample(double sample);
        bool isAnomaly(double sample, double alpha = 0.05);

    private:
        inline static const double D_THETA = 0.0001;

        static int sgn(double x);
        static double cdf(double x, double x_0, double sigma, double lambda);
        static double pdf(double x, double x_0, double sigma, double lambda);
        static double mle(const std::vector<double> &samples, double x_0, double sigma);
        static double nll(const std::vector<double> &samples, double x_0, double sigma, double lambda);

        void toFile(const char *fileName);

        std::deque<double> m_samples;
        double m_x0 = 0;
        double m_sigma = 0;
        double m_lambda = 0;
        bool m_ready = false;
        bool m_anomaly = false;
        size_t m_consecutiveHighPower = 0;
        size_t m_consecutiveLowPower = 0;
    };
}