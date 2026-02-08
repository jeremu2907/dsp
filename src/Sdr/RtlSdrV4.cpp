#include <thread>
#include <chrono>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include "pch.hpp"
#include "Sdr/RtlSdrV4.hpp"
#include "Dsp/PowerSpectralDensity.hpp"

#define DWELL_PER_FREQUENCY_US (1.0e6 / m_frequencies.size())

using namespace Sdr;

RtlSdrV4::RtlSdrV4() : SdrBase("rtlsdr") {}

void RtlSdrV4::processThread()
{
    SoapySDR::Stream *rx_stream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    if (rx_stream == NULL)
    {
        throw std::runtime_error("Failed to set up stream");
    }
    m_device->activateStream(rx_stream, 0, 0, 0);
    const size_t numElements = Dsp::PowerSpectralDensity::FFT_SIZE;
    std::complex<float> buff[numElements];

    while (m_running.load() == true)
    {
        void *buffs[] = {buff};
        int flags;
        long long time_ns;
        m_device->readStream(rx_stream, buffs, numElements, flags, time_ns, 1e5);

        std::complex<float> out[numElements];
        m_psd->execute(buff, out);

        float psdReal[Dsp::PowerSpectralDensity::FFT_SIZE];
        m_psd->computeRealPsd(out, psdReal, m_sampleRate);

        m_psd->toFile(psdReal, m_frequency, m_bandwidth);
    }
    LOG(SOAPY_SDR_INFO, "Stopping RTL-SDR v4 run thread");
    m_device->deactivateStream(rx_stream, 0, 0);
    m_device->closeStream(rx_stream);
    LOG(SOAPY_SDR_INFO, "Deactivated and closed RTL-SDR v4 RX stream successfully");
}

void RtlSdrV4::processThreadWithRoundRobin()
{
    const size_t numElements = Dsp::PowerSpectralDensity::FFT_SIZE;

    std::complex<float> buff[numElements];
    std::complex<float> out[numElements];
    float psdReal[numElements];

    SoapySDR::Stream *rx_stream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    if (rx_stream == nullptr)
    {
        throw std::runtime_error("Failed to set up stream");
    }

    void *buffs[] = {buff};
    int flags = 0;
    long long time_ns = 0;

    while (m_running.load())
    {
        for (const auto &frequency : m_frequencies)
        {
            if (!m_running.load())
                break;

            configure(frequency, BANDWIDTH_MHZ, GAIN_MHZ, SAMPLE_RATE_MHZ);

            m_device->activateStream(rx_stream, 0, 0, 0);
            m_device->readStream(rx_stream, buffs, numElements, flags, time_ns, 1e5);

            auto start = std::chrono::steady_clock::now();
            const long long dwellTime = static_cast<long long>(DWELL_PER_FREQUENCY_US);

            while (std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count() < dwellTime)
            {
                int ret = m_device->readStream(rx_stream, buffs, numElements, flags, time_ns, 1e5);

                if (ret < 0)
                {
                    LOG(SOAPY_SDR_WARNING, "readStream error: %d", ret);
                    continue;
                }

                m_psd->execute(buff, out);
                m_psd->computeRealPsd(out, psdReal, m_sampleRate);
                m_psd->toFile(psdReal, frequency, BANDWIDTH_MHZ);
            }

            m_device->deactivateStream(rx_stream, 0, 0);

            LOG(SOAPY_SDR_INFO, "Changed Rtl-Sdr V4 cf to %.2f MHz", frequency / 1e6);
        }
    }

    m_device->closeStream(rx_stream);
    LOG(SOAPY_SDR_INFO, "Stopping Rtl-Sdr V4 run thread");
}

void RtlSdrV4::run()
{
    m_running.store(true);

    std::thread t;
    if (m_frequencies.size() > 0)
    {
        t = std::thread(&RtlSdrV4::processThreadWithRoundRobin, this);
    }
    else
    {
        t = std::thread(&RtlSdrV4::processThread, this);
    }
    t.detach();
}

void RtlSdrV4::setFrequencies(const std::vector<double> &frequencies)
{
    m_frequencies = frequencies;
}