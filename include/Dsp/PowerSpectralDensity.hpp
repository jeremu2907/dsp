#pragma once

#include <vector>
#include <complex>

#include <fftw3.h>

namespace Dsp
{
    class PowerSpectralDensity
    {
    public:
        inline static const size_t FFT_SIZE = 1024;
        inline static const char PSD_OUTPUT_FILE_NAME[] = "psd_output.txt";
        PowerSpectralDensity();
        ~PowerSpectralDensity();

        static void toFile(float* arr);
        static void computeRealPsd(const std::complex<float>* fft, float* real, float sampleRate);

        void execute(std::complex<float>* in, std::complex<float>* out);

    private:
        static void rotate(float* arr);
        static void swap(float& a, float& b);
        void hanningWindow(std::complex<float>* in);

        fftwf_plan m_plan;
    };
}