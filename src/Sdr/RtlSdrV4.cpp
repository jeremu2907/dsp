#include <thread>
#include <chrono>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include "pch.hpp"
#include "Sdr/RtlSdrV4.hpp"
#include "DataStructure/Node.hpp"
#include "Model/SdrRoundRobinConfig.hpp"
#include "DataStructure/CircularLinkedList.hpp"

using namespace Sdr;

RtlSdrV4::RtlSdrV4() : SdrBase("rtlsdr") {}

void RtlSdrV4::processThread()
{
    if (m_configList.empty())
    {
        LOG(SOAPY_SDR_INFO, "RTL-SDR v4 process thread empty. Exiting...");
        return;
    }

    SoapySDR::Stream *rx_stream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    if (rx_stream == NULL)
    {
        throw std::runtime_error("Failed to set up stream");
    }
    m_device->activateStream(rx_stream, 0, 0, 0);

    auto it = m_configList.current();
    auto *config = &it->value;
    configure(config->frequency, BANDWIDTH_HZ, GAIN_DBI);

    auto *psd = &config->psd;
    auto *anomDet = &config->anomDet;
    auto *anom = &config->anomaly;

    size_t numElements = psd->getFftSize();
    float *psdReal = new float[numElements];
    std::complex<float> *out = new std::complex<float>[numElements];
    std::complex<float> *buff = new std::complex<float>[numElements];

    try
    {
        while (m_running.load() == true)
        {
            if (anomDet->isReady() == false)
            {
                LOG(SOAPY_SDR_INFO, "Calibrating initial distribution for %f Hz", config->frequency);
                while (anomDet->isReady() == false)
                {
                    void *buffs[] = {buff};
                    int flags;
                    long long time_ns;
                    m_device->readStream(rx_stream, buffs, numElements, flags, time_ns, 1e5);

                    psd->execute(buff, out);

                    float avgPower = static_cast<float>(psd->computeAvgPower(out));
                    anomDet->pushSample(avgPower);
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                anomDet->processDistribution();
                m_currentTimeS = std::chrono::system_clock::now();
                m_lastSampleCollectedS = std::chrono::system_clock::now();
                m_lastDistributionProcessedS = std::chrono::system_clock::now();
                LOG(SOAPY_SDR_INFO, "Calibrating initial distribution completed for %f Hz", config->frequency);
            }
            else
            {
                for (size_t rep = 0; rep <= Dsp::AnomalyDetection::CONSECUTIVE_COUNT; rep++)
                {
                    void *buffs[] = {buff};
                    int flags;
                    long long time_ns;
                    m_device->readStream(rx_stream, buffs, numElements, flags, time_ns, 1e5);

                    psd->execute(buff, out);

                    float avgPower = static_cast<float>(psd->computeAvgPower(out));

                    bool isAnom = anomDet->isAnomaly(avgPower);

                    if (isAnom == false)
                    {
                        if (*anom == true)
                        {
                            *anom = false;
                            LOG(SOAPY_SDR_INFO, "🔴 Anomaly Ended @ %f Hz", config->frequency);
                        }

                        if (isTimeToCollectSample())
                        {
                            anomDet->pushSample(avgPower);
                        }
                        if (isTimeToProcessSampleDistribution())
                        {
                            anomDet->processDistribution();
                        }
                    }
                    else
                    {
                        if (*anom == false)
                        {
                            *anom = true;
                            LOG(SOAPY_SDR_INFO, "🔵 Anomaly Detected @ %f Hz", config->frequency);
                        }
                    }

                    float avgPowerList[] = {avgPower};
                    psd->toFile("avg_power_output.txt", m_frequency, m_bandwidth, avgPowerList, 1);
                    psd->computeRealPsd(out, psdReal, m_sampleRate);
                    psd->toFile("psd_output.txt", m_frequency, m_bandwidth, psdReal, numElements);
                }
            }

            if (m_configList.size() == 1)
            {
                continue;
            }

            auto *tConfig = &m_configList.next()->value;

            if (config == tConfig)
            {
                continue;
            }

            config = tConfig;
            configure(config->frequency, BANDWIDTH_HZ);
            psd = &config->psd;
            anom = &config->anomaly;
            anomDet = &config->anomDet;

            size_t newNumElements = psd->getFftSize();
            if (newNumElements != numElements)
            {
                delete[] out;
                delete[] buff;
                delete[] psdReal;

                numElements = newNumElements;
                psdReal = new float[numElements];
                out = new std::complex<float>[numElements];
                buff = new std::complex<float>[numElements];
            }
        }
    }
    catch (...)
    {
        LOG(SOAPY_SDR_ERROR, "Stopping %s run thread due to ERROR", m_driver.c_str());
        delete[] out;
        delete[] buff;
        delete[] psdReal;
        m_device->deactivateStream(rx_stream, 0, 0);
        m_device->closeStream(rx_stream);
        throw;
    }

    delete[] out;
    delete[] buff;
    delete[] psdReal;

    m_device->deactivateStream(rx_stream, 0, 0);
    m_device->closeStream(rx_stream);

    LOG(SOAPY_SDR_INFO, "Deactivated and closed %s RX stream successfully", m_driver.c_str());
}

void RtlSdrV4::setFrequencies(const std::vector<double> &frequencies)
{
    for (auto &f : frequencies)
    {
        Model::SdrRoundRobinConfig *config = new Model::SdrRoundRobinConfig();
        config->anomaly = false;
        config->bandwidth = BANDWIDTH_HZ;
        config->frequency = f;
        m_configList.emplace(*config);
        delete config;
    }
}

void RtlSdrV4::configure(double frequency,
                         double bandwidth,
                         double gain,
                         double sampleRate)
{
    SdrBase::configure(frequency, bandwidth, gain, sampleRate);
    m_configList.current()->value.psd.setFftSize(bandwidth);
}