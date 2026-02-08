#include <math.h>
#include <fstream>

#include "Dsp/PowerSpectralDensity.hpp"

using namespace Dsp;

static_assert(sizeof(std::complex<float>) == sizeof(fftwf_complex), 
              "std::complex<float> must have same layout as fftwf_complex");

PowerSpectralDensity::PowerSpectralDensity()
{
    m_plan = fftwf_plan_dft_1d(FFT_SIZE, NULL, NULL, FFTW_FORWARD, FFTW_ESTIMATE);
}

PowerSpectralDensity::~PowerSpectralDensity()
{
    fftwf_destroy_plan(m_plan);
}

void PowerSpectralDensity::execute(std::complex<float> *in, std::complex<float> *out)
{
    hanningWindow(in);
    fftwf_execute_dft(m_plan,
                      reinterpret_cast<fftwf_complex *>(in),
                      reinterpret_cast<fftwf_complex *>(out));
}

void PowerSpectralDensity::hanningWindow(std::complex<float>* in)
{
    for(size_t i = 0; i < FFT_SIZE; i++)
    {
        float window = sinf((M_PI * i)/FFT_SIZE);
        in[i] *= (window * window);
    }
}

void PowerSpectralDensity::computeRealPsd(const std::complex<float>* fft, float* real, float sampleRate)
{
    for(size_t i = 0; i < FFT_SIZE; i++)
    {
        real[i] = std::abs(fft[i]);
        real[i] = real[i] * real[i];
        real[i] = real[i] / (static_cast<float>(FFT_SIZE) * sampleRate);
        real[i] = 10.0f * log10f(real[i]);
    }

    rotate(real);
}

void PowerSpectralDensity::toFile(float* arr, double cf, double bw)
{
    std::string temp_file = std::string(PSD_OUTPUT_FILE_NAME) + ".tmp";
    std::ofstream os(temp_file, std::ios::trunc);

    os << cf << '\n';
    os << bw << '\n';
    
    if (os.is_open())
    {
        for(size_t i = 0; i < FFT_SIZE; i++)
        {
            os << arr[i] << ",";
        }
        os.close();
        
        // Atomic rename (overwrites the target file)
        std::rename(temp_file.c_str(), PSD_OUTPUT_FILE_NAME);
    }
}

void PowerSpectralDensity::rotate(float* arr)
{
    size_t mid = FFT_SIZE / 2;
    for(size_t i = 0; i < mid; i++)
    {
        swap(arr[i], arr[mid + i]);
    }
}

void PowerSpectralDensity::swap(float& a, float& b)
{
    float c = b;
    b = a;
    a = c;
}