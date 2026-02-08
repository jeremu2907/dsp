#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include "pch.hpp"
#include "Sdr/LimeSdrMini2.hpp"
#include "Dsp/PowerSpectralDensity.hpp"

using namespace Sdr;

LimeSdrMini2::LimeSdrMini2() : SdrBase("lime") {}

void LimeSdrMini2::processThread()
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

        // LOG(SOAPY_SDR_INFO, "ret = %d, flags = %d, time_ns = %lld\n", ret, flags, time_ns);
    }
    LOG(SOAPY_SDR_INFO, "Stopping LimeSdrMini2.0 run thread");
    m_device->deactivateStream(rx_stream, 0, 0);
    m_device->closeStream(rx_stream);
    LOG(SOAPY_SDR_INFO, "Deactivated and closed LimeSdrMini2.0 RX stream successfully");
}
