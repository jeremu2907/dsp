#include <thread>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include <random>

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
    if (m_mode == SOAPY_SDR_RX)
    {
        rx();
    }
    else if (m_mode == SOAPY_SDR_TX)
    {
        std::thread inputT(&LimeSdrMini2::inputThread, this);
        tx();
        inputT.join();
    }
}

void LimeSdrMini2::setMode(int mode)
{
    m_mode = mode;
}

void LimeSdrMini2::rx()
{
    SoapySDR::Stream *rxStream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    if (rxStream == NULL)
    {
        throw std::runtime_error("Failed to set up stream");
    }
    m_device->activateStream(rxStream, 0, 0, 0);
    const size_t numElements = m_psd->getFftSize();
    std::complex<float> *buff = new std::complex<float>[numElements];
    std::complex<float> *out = new std::complex<float>[numElements];
    float *psdReal = new float[numElements];

    try
    {
        bool high = false;
        while (m_running.load() == true)
        {
            if (m_anomDet->isReady() == false)
            {
                LOG(SOAPY_SDR_DEBUG, "Calibrating initial distribution...");
                while (m_anomDet->isReady() == false)
                {
                    void *buffs[] = {buff};
                    int flags;
                    long long time_ns;
                    m_device->readStream(rxStream, buffs, numElements, flags, time_ns, 1e5);

                    m_psd->execute(buff, out);

                    float avgPower = static_cast<float>(m_psd->computeAvgPower(out));
                    m_anomDet->pushSample(avgPower);
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }

                m_anomDet->processDistribution();
                m_currentTimeS = std::chrono::system_clock::now();
                m_lastSampleCollectedS = std::chrono::system_clock::now();
                m_lastDistributionProcessedS = std::chrono::system_clock::now();
                LOG(SOAPY_SDR_INFO, "Calibrating initial distribution completed");
            }
            else
            {
                void *buffs[] = {buff};
                int flags;
                long long time_ns;
                m_device->readStream(rxStream, buffs, numElements, flags, time_ns, 1e5);

                m_psd->execute(buff, out);

                float avgPower = static_cast<float>(m_psd->computeAvgPower(out));
                bool isAnom = m_anomDet->isAnomaly(avgPower);

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
    }
    catch (...)
    {
        LOG(SOAPY_SDR_ERROR, "Stopping %s run thread due to ERROR", m_driver.c_str());
        delete[] buff;
        delete[] out;
        delete[] psdReal;

        m_device->deactivateStream(rxStream, 0, 0);
        m_device->closeStream(rxStream);
        throw;
    }
    LOG(SOAPY_SDR_INFO, "Stopping %s run thread", m_driver.c_str());
    delete[] buff;
    delete[] out;
    delete[] psdReal;
    m_device->deactivateStream(rxStream, 0, 0);
    m_device->closeStream(rxStream);
    LOG(SOAPY_SDR_INFO, "Deactivated and closed %s RX stream successfully", m_driver.c_str());
}

void LimeSdrMini2::tx()
{
    configureTx(0, 10e6, 64, 30e6);
    SoapySDR::Stream *txStream = m_device->setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32);
    if (txStream == NULL)
    {
        throw std::runtime_error("Failed to set up stream");
    }
    m_device->activateStream(txStream, 0, 0, 0);
    LOG(SOAPY_SDR_INFO, "TX stream activated, waiting for trigger...");

    const size_t BUFF_SIZE = 2048;
    std::complex<float> *buff = new std::complex<float>[BUFF_SIZE];
    void *buffs[] = {buff};
    int flags = 0;

    const double sampleRate = m_device->getSampleRate(SOAPY_SDR_TX, 0);
    const int DWELL_MS = 20;

    std::mt19937 rng(std::random_device{}());

    auto getNextFreqIndex = [&](size_t current) -> size_t
    {
        if (m_txFrequencies.size() == 1) return 0;
        std::uniform_int_distribution<size_t> dist(0, m_txFrequencies.size() - 2);
        size_t next = dist(rng);
        if (next >= current) next++;
        return next;
    };

    auto generateTone = [&](double cwHz)
    {
        double phase = 0.0;
        const double phaseStep = 2.0 * M_PI * cwHz / sampleRate;
        for (size_t i = 0; i < BUFF_SIZE; i++)
        {
            buff[i] = std::complex<float>(std::cos(phase), std::sin(phase));
            phase += phaseStep;
            if (phase > 2.0 * M_PI)
                phase -= 2.0 * M_PI;
        }
    };

    size_t freqIndex = 0;
    generateTone(m_txFrequencies[freqIndex]);

    while (m_running.load() == true)
    {
        if (m_triggerTx.load() == false)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto dwellStart = std::chrono::steady_clock::now();

        while (m_running.load() && m_triggerTx.load())
        {
            int ret = m_device->writeStream(txStream, buffs, BUFF_SIZE, flags, 1e5);
            if (ret < 0)
            {
                LOG(SOAPY_SDR_ERROR, "TX write error: %d", ret);
                goto cleanup;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - dwellStart)
                               .count();

            if (elapsed >= DWELL_MS)
                break;
        }

        freqIndex = getNextFreqIndex(freqIndex);
        m_device->setFrequency(SOAPY_SDR_TX, 0, m_txFrequencies[freqIndex]);
        generateTone(m_txFrequencies[freqIndex]);
        LOG(SOAPY_SDR_INFO, "Hopped to %f Hz", m_txFrequencies[freqIndex]);
    }

cleanup:
    delete[] buff;
    m_device->deactivateStream(txStream, 0, 0);
    m_device->closeStream(txStream);
}

void LimeSdrMini2::inputThread()
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    LOG(SOAPY_SDR_INFO, "Input thread started, press SPACE to toggle TX, q to quit");

    while (m_running.load() == true)
    {
        char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);

        if (n < 0)
        {
            LOG(SOAPY_SDR_ERROR, "read() error: %d", errno);
            break;
        }
        if (n == 0)
            continue; // timeout, no key

        LOG(SOAPY_SDR_INFO, "Key received: %d", (int)ch); // temp debug

        if (ch == ' ' || ch == '\n' || ch == '\r')
        {
            bool current = m_triggerTx.load();
            m_triggerTx.store(!current);
            LOG(SOAPY_SDR_INFO, "TX %s", !current ? "ON" : "OFF");
        }
        else if (ch == 'q')
        {
            m_running.store(false);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

void LimeSdrMini2::setTxFrequencies(std::vector<double> frequencies)
{
    m_txFrequencies = frequencies;
}

void LimeSdrMini2::configureRx(double frequency,
                               double bandwidth,
                               double gain,
                               double sampleRate)
{
    SdrBase::configureRx(frequency, bandwidth, gain, sampleRate);
    m_psd->setFftSize(bandwidth);
}
