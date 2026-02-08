#include <map>
#include <thread>
#include <string>
#include <vector>
#include <complex>

#include <SoapySDR/Types.hpp>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include "pch.hpp"
#include "Sdr/LimeSdrMini2.hpp"
#include "Dsp/PowerSpectralDensity.hpp"

using namespace Sdr;

LimeSdrMini2::LimeSdrMini2() : m_psd(std::make_unique<Dsp::PowerSpectralDensity>())
{
    SoapySDR::KwargsList kwargsList = SoapySDR::Device::enumerate();
    if (kwargsList.size() == 0)
    {
        throw std::runtime_error("No sdr found");
    }

    for (auto &kwargs : kwargsList)
    {
        auto it = kwargs.begin();
        while (it != kwargs.end())
        {
            std::string key = it->first;
            std::string val = it->second;
            if (key == "driver" && val == "lime")
            {
                SoapySDR::Device *dev = SoapySDR::Device::make(kwargs);
                m_device = std::unique_ptr<SoapySDR::Device>(dev);

                if (m_device == NULL)
                {
                    throw std::runtime_error("Failed to create Device");
                }
            }
            it++;
        }
    }

    m_running.store(false);
}

LimeSdrMini2::~LimeSdrMini2()
{
    m_running.store(false);

    SoapySDR::Device::unmake(m_device.release());
}

void LimeSdrMini2::configure(double frequency,
                             double bandwidth,
                             double gain)
{
    m_device->setGain(SOAPY_SDR_RX, 0, gain);
    m_device->setFrequency(SOAPY_SDR_RX, 0, frequency);
    m_device->setBandwidth(SOAPY_SDR_RX, 0, bandwidth);
    m_device->setSampleRate(SOAPY_SDR_RX, 0, bandwidth);
    m_device->setAntenna(SOAPY_SDR_RX, 0, "LNAW");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    m_gain = m_device->getGain(SOAPY_SDR_RX, 0);
    m_frequency = m_device->getFrequency(SOAPY_SDR_RX, 0);
    m_bandwidth = m_device->getBandwidth(SOAPY_SDR_RX, 0);
    m_sampleRate = m_device->getBandwidth(SOAPY_SDR_RX, 0);
}

void LimeSdrMini2::run()
{
    m_running.store(true);

    std::thread t(
        [this]()
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

                m_psd->toFile(psdReal);

                // LOG(SOAPY_SDR_INFO, "ret = %d, flags = %d, time_ns = %lld\n", ret, flags, time_ns);
            }
            LOG(SOAPY_SDR_INFO, "Stopping LimeSdrMini2.0 run thread");
            m_device->deactivateStream(rx_stream, 0, 0);
            m_device->closeStream(rx_stream);
            LOG(SOAPY_SDR_INFO, "Deactivated and closed LimeSdrMini2.0 RX stream successfully");
        });

    t.detach();
}

void LimeSdrMini2::stop()
{
    m_running.store(false);
}