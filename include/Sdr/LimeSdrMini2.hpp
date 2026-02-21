#pragma once

#include "SdrBase.hpp"

namespace Dsp
{
    class PowerSpectralDensity;
}

namespace SoapySDR
{
    class Device;
}

namespace Sdr
{
    class LimeSdrMini2 : public SdrBase
    {
    public:
        LimeSdrMini2();

        void tx();
        
        void rx();

        void setTxFrequencies(std::vector<double> frequencies);

        void setMode(int mode);

        void processThread() override;

        void configureRx(double frequency,
                       double bandwidth,
                       double gain = GAIN_DBI,
                       double sampleRate = -9999) override;

    private:
        void inputThread();

        std::vector<double> m_txFrequencies;
        int m_mode = SOAPY_SDR_RX;
        std::atomic<bool> m_triggerTx{false};
        std::unique_ptr<Dsp::PowerSpectralDensity> m_psd;
        std::unique_ptr<Dsp::AnomalyDetection> m_anomDet;
    };
}