#include <thread>
#include <chrono>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include "pch.hpp"
#include "Sdr/LimeSdrMini2.hpp"

#include "Dsp/PowerSpectralDensity.hpp"
#include "Dsp/AnomalyDetection.hpp"

using namespace Sdr;

LimeSdrMini2::LimeSdrMini2() : SdrBase("lime"),
                               m_psd(std::make_unique<Dsp::PowerSpectralDensity>()),
                               m_anomDet(std::make_unique<Dsp::AnomalyDetection>()) {}

void LimeSdrMini2::processThread()
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
                    m_anomDet->processDistribution();
                    m_currentTimeS = std::chrono::system_clock::now();
                    m_lastSampleCollectedS = std::chrono::system_clock::now();
                    m_lastDistributionProcessedS = std::chrono::system_clock::now();
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
                    LOG(SOAPY_SDR_INFO, "🔴 Anomaly Ended on LimeSdr @ %f", m_frequency);
                }

                if (isTimeToCollectSample())
                {
                    m_anomDet->pushSample(avgPower);
                }
                if (isTimeToProcessSampleDistribution())
                {
                    m_anomDet->processDistribution();
                }
            }
            else
            {
                if (high == false)
                {
                    high = true;
                    LOG(SOAPY_SDR_INFO, "🔵 Anomaly Detected on LimeSdr @ %f", m_frequency);
                }
            }

            float avgPowerList[] = {avgPower};
            m_psd->toFile("avg_power_output.txt", m_frequency, m_bandwidth, avgPowerList, 1);
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

void LimeSdrMini2::configure(double frequency,
                             double bandwidth,
                             double gain,
                             double sampleRate)
{
    SdrBase::configure(frequency, bandwidth, gain, sampleRate);
    m_psd->setFftSize(bandwidth);
}
