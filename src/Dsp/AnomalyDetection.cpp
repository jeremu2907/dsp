#include <math.h>
#include <fstream>
#include <iostream>

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
    double p = 1.0 - cdf(sample, m_x0, m_sigma, m_lambda);
    if (p < alpha)
    {
        m_consecutiveHighPower = std::min(CONSECUTIVE_COUNT, m_consecutiveHighPower + 1);
        m_consecutiveLowPower = 0;
    }
    else
    {
        m_consecutiveHighPower = 0;
        m_consecutiveLowPower = std::min(CONSECUTIVE_COUNT, m_consecutiveLowPower + 1);
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
        return;

    std::vector<double> distribution(m_samples.begin(), m_samples.end());
    std::sort(distribution.begin(), distribution.end());

    size_t n = distribution.size() - 1;
    if (n % 2 == 0)
    {
        m_x0 = (distribution[n / 2 - 1] + distribution[n / 2]) / 2.0;
    }
    else
    {
        m_x0 = distribution[n / 2];
    }
    size_t q1 = static_cast<size_t>(n * 0.25);
    size_t q3 = static_cast<size_t>(n * 0.75);
    m_sigma = (distribution[q3] - distribution[q1]) / 2.0;

    m_lambda = mle(distribution, m_x0, m_sigma);

    toFile("cauchy_dist.txt");
}

double AnomalyDetection::mle(const std::vector<double> &samples, double x_0, double sigma)
{
    double bestLambda = D_THETA;
    double bestNll = std::numeric_limits<double>::infinity();

    for (double lambda = -1; lambda <= 1.0 - D_THETA; lambda += D_THETA)
    {
        double currNll = nll(samples, x_0, sigma, lambda);

        if (currNll < bestNll)
        {
            bestNll = currNll;
            bestLambda = lambda;
        }
    }

    return bestLambda;
}

double AnomalyDetection::nll(const std::vector<double> &samples,
                             double x_0, double sigma, double lambda)
{
    double nll = 0.0;
    for (const auto &sample : samples)
    {
        double p = pdf(sample, x_0, sigma, lambda);
        if (p <= 0.0 || !std::isfinite(p))
        {
            return std::numeric_limits<double>::infinity();
        }
        nll += -log(p);
    }
    return nll;
}

double AnomalyDetection::pdf(double x, double x_0, double sigma, double lambda)
{
    double factor = 1.0 / (M_PI * sigma);
    double residual = x - x_0;
    double beta = sigma * (1.0 + lambda * sgn(residual));
    double denominator = 1.0 + ((residual * residual) / (beta * beta));
    return factor * (1.0 / denominator);
}

double AnomalyDetection::cdf(double x, double x_0, double sigma, double lambda)
{
    double residual = x - x_0;
    double sign = static_cast<double>(sgn(residual));
    double constant = (1.0 - lambda) / 2.0;
    double coefficient = (1.0 + sign*lambda) / M_PI;
    double beta = atan(residual / (sigma * (1.0 + sign*lambda)));
    return constant + coefficient * beta;
}

int AnomalyDetection::sgn(double x)
{
    const double epsilon = 1e-18;
    if (x > epsilon)
    {
        return 1;
    }
    else if (x < epsilon)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

void AnomalyDetection::toFile(const char *fileName)
{
    std::string temp_file = std::string(fileName) + ".tmp";
    std::ofstream os(temp_file, std::ios::trunc);

    if (os.is_open())
    {

        os << m_x0 << '\n'
           << m_sigma << '\n'
           << m_lambda << '\n';

        os.flush();
        os.close();

        std::rename(temp_file.c_str(), fileName);
    }
}