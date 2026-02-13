#include <math.h>
#include <fstream>

#include "Dsp/PowerSpectralDensity.hpp"

using namespace Dsp;

static_assert(sizeof(std::complex<float>) == sizeof(fftwf_complex),
              "std::complex<float> must have same layout as fftwf_complex");

PowerSpectralDensity::PowerSpectralDensity() {}

PowerSpectralDensity::~PowerSpectralDensity()
{
    fftwf_destroy_plan(m_plan);
}

size_t PowerSpectralDensity::getFftSize() const
{
    return m_fftSize;
}

void PowerSpectralDensity::execute(std::complex<float> *in, std::complex<float> *out)
{
    hanningWindow(in);
    fftwf_execute_dft(m_plan,
                      reinterpret_cast<fftwf_complex *>(in),
                      reinterpret_cast<fftwf_complex *>(out));
}

void PowerSpectralDensity::hanningWindow(std::complex<float> *in)
{
    for (size_t i = 0; i < m_fftSize; i++)
    {
        float window = sinf((M_PI * i) / m_fftSize);
        in[i] *= (window * window);
    }
}

void PowerSpectralDensity::computeRealPsd(const std::complex<float> *fft, float *real, float sampleRate)
{
    for (size_t i = 0; i < m_fftSize; i++)
    {
        real[i] = std::abs(fft[i]);
        real[i] = real[i] * real[i];
        real[i] = real[i] / (static_cast<float>(m_fftSize) * sampleRate);
        real[i] = 10.0f * log10f(real[i]);
    }

    rotate(real, m_fftSize);
}

void PowerSpectralDensity::setFftSize(double bandwidthHz)
{
    double bandwidthMhz = bandwidthHz / 1e6;
    size_t size = pow(2, 6 + floor(log2(bandwidthMhz)));
    m_fftSize = size;

    m_plan = fftwf_plan_dft_1d(m_fftSize, NULL, NULL, FFTW_FORWARD, FFTW_ESTIMATE);
}

double PowerSpectralDensity::computeAvgPower(const std::complex<float> *iqSamples)
{
    double magnitudeSquared = 0.0f;
    for (size_t i = 0; i < m_fftSize; i++)
    {
        double magnitude = std::abs(iqSamples[i]);
        magnitudeSquared += magnitude * magnitude;
    }

    return magnitudeSquared / static_cast<double>(m_fftSize);
}

void PowerSpectralDensity::toFile(const char *fileName, double cf, double bw, float *arr, size_t size)
{
    std::string temp_file = std::string(fileName) + ".tmp";
    std::ofstream os(temp_file, std::ios::trunc);

    os << cf << '\n';
    os << bw << '\n';
    os << size << '\n';

    if (os.is_open())
    {

        int r = 0;
        for (size_t i = 0; i < size; i++)
        {
            r++;
            os << arr[i] << ",";
        }
        os << '\n';

        os.flush();
        os.close();

        std::rename(temp_file.c_str(), fileName);
    }
}

void PowerSpectralDensity::rotate(float *arr, size_t size)
{
    size_t mid = size / 2;
    for (size_t i = 0; i < mid; i++)
    {
        swap(arr[i], arr[mid + i]);
    }
}

void PowerSpectralDensity::swap(float &a, float &b)
{
    float c = b;
    b = a;
    a = c;
}