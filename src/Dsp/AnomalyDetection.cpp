#include <math.h>
#include <iostream>

#include "Dsp/AnomalyDetection.hpp"

using namespace Dsp;

void AnomalyDetection::pushSample(double sample)
{
    m_samples.push_back(sample);

    if (m_samples.size() > MAX_SIZE)
    {
        m_samples.pop_front();
        processDistribution();
    }
}

bool AnomalyDetection::isReady() const
{
    return m_ready;
}

bool AnomalyDetection::isAnomaly(double sample, double alpha)
{ 
    double p = 1.0f - cdf(sample);
    if(p < alpha)
    {
        m_consecutiveHighPower = std::min(CONSECUTIVE_COUNT, m_consecutiveHighPower + 1);
        m_consecutiveLowPower = 0;
    }
    else
    {
        m_consecutiveHighPower = 0;
        m_consecutiveLowPower = std::min(CONSECUTIVE_COUNT, m_consecutiveLowPower + 1);
    }

    if(m_consecutiveHighPower >= CONSECUTIVE_COUNT)
    {
        m_anomaly = true;
        return m_anomaly;
    }

    if(m_consecutiveLowPower >= CONSECUTIVE_COUNT)
    {
        m_anomaly = false;
        return m_anomaly;
    }

    return m_anomaly;
}

void AnomalyDetection::processDistribution()
{
    double n = m_samples.size();

    double sum = 0.0f;
    for (auto &s : m_samples)
    {
        sum += s;
    }
    double mean = sum / n;

    double sumResidualSquared = 0.0f;
    for (auto &s : m_samples)
    {
        double residual = s - mean;
        sumResidualSquared += residual * residual;
    }
    double var = sumResidualSquared / (n - 1);

    m_mean = mean;
    m_var = var;

    m_ready = true;
}

double AnomalyDetection::cdf(double x)
{
    return 0.5f * (1.0f + erf((x - m_mean) / (sqrt(2.0f * m_var))));
}