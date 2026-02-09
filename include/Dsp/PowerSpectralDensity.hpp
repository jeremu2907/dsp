#pragma once

#include <vector>
#include <complex>

#include <fftw3.h>

namespace Dsp
{
    class PowerSpectralDensity
    {
    public:
        PowerSpectralDensity();
        ~PowerSpectralDensity();

        static void toFile(const char* fileName, double cf, double bw, float* arr, size_t size);

        void computeRealPsd(const std::complex<float>* fft, float* real, float sampleRate);

        double computeAvgPower(const std::complex<float>* iqSamples);

        void execute(std::complex<float>* in, std::complex<float>* out);

        size_t getFftSize() const;

        void setFftSize(double bandwidthHz);

    private:
        static void rotate(float* arr, size_t size);
        static void swap(float& a, float& b);
        void hanningWindow(std::complex<float>* in);

        fftwf_plan m_plan;
        size_t m_fftSize;
    };
}