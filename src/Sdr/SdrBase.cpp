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
#include "Dsp/AnomalyDetection.hpp"

using namespace Sdr;

SdrBase::SdrBase(const std::string &driver) : m_psd(std::make_unique<Dsp::PowerSpectralDensity>()),
                                              m_anomDet(std::make_unique<Dsp::AnomalyDetection>()),
                                              m_driver(driver)
{
    bool found = false;

    SoapySDR::KwargsList kwargsList = SoapySDR::Device::enumerate();
    if (kwargsList.size() == 0)
    {
        throw std::runtime_error("No SDR found");
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

    if (found == false)
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
    bool configSuccess = false;
    do
    {
        try
        {
            m_device->setGain(SOAPY_SDR_RX, 0, gain);
            m_device->setFrequency(SOAPY_SDR_RX, 0, frequency);
            m_device->setBandwidth(SOAPY_SDR_RX, 0, bandwidth);
            sampleRate = sampleRate < 0 ? bandwidth : sampleRate;
            m_device->setSampleRate(SOAPY_SDR_RX, 0, sampleRate);

            m_psd->setFftSize(bandwidth);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            m_gain = m_device->getGain(SOAPY_SDR_RX, 0);
            m_frequency = m_device->getFrequency(SOAPY_SDR_RX, 0);
            m_bandwidth = m_device->getBandwidth(SOAPY_SDR_RX, 0);
            m_sampleRate = m_device->getBandwidth(SOAPY_SDR_RX, 0);
        }
        catch (...)
        {
            configSuccess = false;
        }

        configSuccess = true;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (configSuccess == false);
}

void SdrBase::run()
{
    m_running.store(true);

    std::thread t(&SdrBase::processThread, this);

    t.detach();
}

void SdrBase::processThread()
{
    SoapySDR::Stream *rx_stream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    if (rx_stream == NULL)
    {
        throw std::runtime_error("Failed to set up stream");
    }
    m_device->activateStream(rx_stream, 0, 0, 0);
    const size_t numElements = m_psd->getFftSize();
    std::complex<float> *buff = new std::complex<float>[numElements];
    std::complex<float> *out = new std::complex<float>[numElements];
    float *psdReal = new float[numElements];

    try
    {
        bool init = true;
        bool high = false;
        bool previousIsAnomDetReady = m_anomDet->isReady();
        while (m_running.load() == true)
        {
            void *buffs[] = {buff};
            int flags;
            long long time_ns;
            m_device->readStream(rx_stream, buffs, numElements, flags, time_ns, 1e5);

            m_psd->execute(buff, out);

            float avgPower = static_cast<float>(m_psd->computeAvgPower(out));

            if (init == true)
            {
                init = false;
                continue;
            }

            bool isAnom = false;
            if (previousIsAnomDetReady == false)
            {
                m_anomDet->pushSample(avgPower);
                previousIsAnomDetReady = m_anomDet->isReady();
                if (previousIsAnomDetReady == true)
                {
                    LOG(SOAPY_SDR_INFO, "Calibrating initial distribution completed");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                LOG(SOAPY_SDR_DEBUG, "Calibrating initial distribution...");
            }
            else
            {
                isAnom = m_anomDet->isAnomaly(avgPower);
            }

            if (isAnom == false)
            {
                if (high == true)
                {
                    high = false;
                    LOG(SOAPY_SDR_INFO, "🔴 Anomaly Ended");
                }
                float avgPowerList[] = {avgPower};
                m_psd->toFile("avg_power_output.txt", m_frequency, m_bandwidth, avgPowerList, 1);
            }
            else
            {
                if (high == false)
                {
                    high = true;
                    LOG(SOAPY_SDR_INFO, "🔵 Anomaly Detected");
                }
            }

            m_psd->computeRealPsd(out, psdReal, m_sampleRate);

            m_psd->toFile("psd_output.txt", m_frequency, m_bandwidth, psdReal, numElements);
        }
    }
    catch (...)
    {
        LOG(SOAPY_SDR_ERROR, "Stopping %s run thread due to ERROR", m_driver.c_str());
        delete[] buff;
        delete[] out;
        delete[] psdReal;
        throw;
    }
    LOG(SOAPY_SDR_INFO, "Stopping %s run thread", m_driver.c_str());
    delete[] buff;
    delete[] out;
    delete[] psdReal;
    m_device->deactivateStream(rx_stream, 0, 0);
    m_device->closeStream(rx_stream);
    LOG(SOAPY_SDR_INFO, "Deactivated and closed %s RX stream successfully", m_driver.c_str());
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