#include <map>
#include <thread>
#include <string>
#include <vector>
#include <complex>
#include <exception>

#include <SoapySDR/Types.hpp>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include "pch.hpp"
#include "Sdr/SdrBase.hpp"
#include "Dsp/PowerSpectralDensity.hpp"

using namespace Sdr;

SdrBase::SdrBase(const std::string &driver) : m_psd(std::make_unique<Dsp::PowerSpectralDensity>())
{
    bool found = false;

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

            if (key == "driver" && val == driver)
            {
                SoapySDR::Device *dev = SoapySDR::Device::make(kwargs);
                m_device = std::unique_ptr<SoapySDR::Device>(dev);

                if (m_device == NULL)
                {
                    throw std::runtime_error("Failed to create Device");
                }

                found = true;
            }
            it++;
        }
    }

    if(found == false)
    {
        std::string e = "No " + driver + " driver found";
        throw std::runtime_error(e);
    }

    m_running.store(false);
}

SdrBase::~SdrBase()
{
    stop();

    SoapySDR::Device::unmake(m_device.release());
}

void SdrBase::configure(double frequency,
                        double bandwidth,
                        double gain,
                        double sampleRate)
{
    m_device->setGain(SOAPY_SDR_RX, 0, gain);
    m_device->setFrequency(SOAPY_SDR_RX, 0, frequency);
    m_device->setBandwidth(SOAPY_SDR_RX, 0, bandwidth);
    sampleRate = sampleRate < 0 ? bandwidth : sampleRate;
    m_device->setSampleRate(SOAPY_SDR_RX, 0, sampleRate);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    m_gain = m_device->getGain(SOAPY_SDR_RX, 0);
    m_frequency = m_device->getFrequency(SOAPY_SDR_RX, 0);
    m_bandwidth = m_device->getBandwidth(SOAPY_SDR_RX, 0);
    m_sampleRate = m_device->getBandwidth(SOAPY_SDR_RX, 0);
}

void SdrBase::run()
{
    m_running.store(true);

    std::thread t(&SdrBase::processThread, this);

    t.detach();
}

void SdrBase::stop()
{
    m_running.store(false);
}

double SdrBase::getGain() const
{
    return m_gain;
}

double SdrBase::getFrequency() const
{
    return m_frequency;
}

double SdrBase::getBandwidth() const
{
    return m_bandwidth;
}

double SdrBase::getSampleRate() const
{
    return m_sampleRate;
}