#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace fc {

class CpuManager {
public:
    CpuManager();
    ~CpuManager() = default;

    void scanCpu();
    void updateLiveMetrics();

    const CpuInfo& getCpuInfo() const { return m_info; }

    bool setGovernor(const std::string& governor);
    bool setEnergyPreference(const std::string& preference);
    bool setFreqLimits(double minGhz, double maxGhz);
    bool setTurboBoost(bool enable);

private:
    CpuInfo m_info;
    std::string m_tempSensorPath;
    std::string m_turboPath;
    bool m_isNoTurboFile = false; // true if file is intel_pstate/no_turbo (where 0=on, 1=off)

    void findTempSensor();
    void findTurboControl();
};

} // namespace fc
