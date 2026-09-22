#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>

#include <boost/math/distributions/students_t.hpp>

#include "Dsp/AnomalyDetection.hpp"

using namespace Dsp;


void AnomalyDetection::pushSample(double sample)
{
    m_samples.push_back(sample);

    if (m_samples.size() > MAX_SIZE)
    {
        m_samples.pop_front();
        m_ready = true;
    }
}

bool AnomalyDetection::isReady() const
{
    return m_ready;
}

bool AnomalyDetection::isAnomaly(double sample, double alpha)
{
    m_prevFilteredSample = (1.0 - Dsp::AnomalyDetection::EMWA_ALPHA) * m_prevFilteredSample + Dsp::AnomalyDetection::EMWA_ALPHA * sample;

    const double p = 1.0 - cdf(m_prevFilteredSample,
                               m_mean,
                               m_sigma,
                               m_nu);

    if (p < alpha)
    {
        m_consecutiveHighPower = std::min(CONSECUTIVE_COUNT,
                                          m_consecutiveHighPower + 1);

        m_consecutiveLowPower = 0;
    }
    else
    {
        m_consecutiveHighPower = 0;

        m_consecutiveLowPower = std::min(CONSECUTIVE_COUNT,
                                         m_consecutiveLowPower + 1);
    }

    if (m_consecutiveHighPower >= CONSECUTIVE_COUNT)
    {
        m_anomaly = true;
        return m_anomaly;
    }

    if (m_consecutiveLowPower >= CONSECUTIVE_COUNT)
    {
        m_anomaly = false;
        return m_anomaly;
    }

    return m_anomaly;
}

void AnomalyDetection::processDistribution()
{
    if (m_samples.size() < 2)
    {
        return;
    }

    std::vector<double> distribution(m_samples.begin(),
                                     m_samples.end());

    const size_t n = distribution.size();

    m_mean = std::accumulate(distribution.begin(),
                             distribution.end(),
                             0.0) / static_cast<double>(n);

    double variance = 0.0;

    for (const double x : distribution)
    {
        const double d = x - m_mean;
        variance += d * d;
    }

    variance /= static_cast<double>(n - 1);

    double sigma = std::sqrt(variance);

    if (sigma <= 0.0 ||
        !std::isfinite(sigma))
    {
        sigma = std::numeric_limits<double>::epsilon();
    }

    m_sigma = sigma;

    m_nu = n - 1;

    toFile("student_t_dist.txt");
}

double AnomalyDetection::pdf(double x,
                             double x_0,
                             double sigma,
                             double nu)
{
    if (sigma <= 0.0 || nu <= 0.0)
    {    
        return 0.0;
    }

    const double z = (x - x_0) / sigma;

    boost::math::students_t_distribution<double> t(nu);

    return boost::math::pdf(t, z) / sigma;
}

double AnomalyDetection::cdf(double x,
                             double x_0,
                             double sigma,
                             double nu)
{
    if (sigma <= 0.0 || nu <= 0.0)
    {
        return 0.0;
    }

    const double z = (x - x_0) / sigma;

    boost::math::students_t_distribution<double> t(nu);

    return boost::math::cdf(t, z);
}

double AnomalyDetection::mean() const
{
    return m_mean;
}

double AnomalyDetection::minSnrDb() const
{
    return 10.0 * log10((m_mean + CRITIAL_VALUE_FROM_ALPHA_AND_N_MINUS_ONE * m_sigma) / m_mean);
}

double AnomalyDetection::prevFilteredSample() const
{
    return m_prevFilteredSample;
}

void AnomalyDetection::toFile(
    const char* fileName)
{
    std::string temp_file = std::string(fileName) + ".tmp";

    std::ofstream os(
        temp_file,
        std::ios::trunc);

    if (os.is_open())
    {
        os << m_mean << '\n'
           << m_sigma << '\n'
           << m_nu << '\n';

        os.flush();
        os.close();

        std::rename(
            temp_file.c_str(),
            fileName);
    }
}