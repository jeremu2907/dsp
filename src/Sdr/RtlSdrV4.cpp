#include <thread>
#include <chrono>
#include <iostream>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include "pch.hpp"
#include "Sdr/RtlSdrV4.hpp"

using namespace Sdr;

RtlSdrV4::RtlSdrV4() : SdrBase("rtlsdr"),
                       m_frequencies(),
                       m_anomDetMap(),
                       m_psdMap(),
                       m_anomMap() {}

void RtlSdrV4::processThread()
{
    if (m_frequencies.empty())
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

    auto it = m_frequencies.begin();
    double frequency = *(it);
    configure(frequency, BANDWIDTH_HZ, GAIN_HZ);

    auto *psd = &m_psdMap[frequency];
    auto *anomDet = &m_anomDetMap[frequency];
    auto *anom = &m_anomMap[frequency];

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
                LOG(SOAPY_SDR_INFO, "Calibrating initial distribution for %f Hz", frequency);
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
                LOG(SOAPY_SDR_INFO, "Calibrating initial distribution completed for %f Hz", frequency);
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
                            LOG(SOAPY_SDR_INFO, "🔴 Anomaly Ended @ %f Hz", frequency);
                        }
                        // float avgPowerList[] = {avgPower};
                        // psd->toFile("avg_power_output.txt", m_frequency, m_bandwidth, avgPowerList, 1);
                    }
                    else
                    {
                        if (*anom == false)
                        {
                            *anom = true;
                            LOG(SOAPY_SDR_INFO, "🔵 Anomaly Detected @ %f Hz", frequency);
                        }
                    }

                    // psd->computeRealPsd(out, psdReal, m_sampleRate);

                    // psd->toFile("psd_output.txt", m_frequency, m_bandwidth, psdReal, numElements);
                }
            }

            if (m_frequencies.size() == 1)
            {
                continue;
            }

            if (it + 1 != m_frequencies.end())
            {
                frequency = *(++it);
            }
            else
            {
                it = m_frequencies.begin();
                frequency = *(it);
            }
            configure(frequency, BANDWIDTH_HZ, GAIN_HZ);
            psd = &m_psdMap[frequency];
            anom = &m_anomMap[frequency];
            anomDet = &m_anomDetMap[frequency];

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
    m_frequencies = frequencies;

    for (auto &f : m_frequencies)
    {
        auto [psdIt, inserted] = m_psdMap.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(f),
            std::forward_as_tuple());

        psdIt->second.setFftSize(static_cast<size_t>(BANDWIDTH_HZ));

        m_anomDetMap.emplace(f, Dsp::AnomalyDetection());
        m_anomMap.emplace(f, false);
    }
}