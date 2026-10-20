#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace fc {

class GpuManager {
public:
    GpuManager();
    ~GpuManager() = default;

    void scanGpu();
    void updateLiveMetrics();

    const GpuInfo& getGpuInfo() const { return m_info; }

    bool setPerformanceLevel(const std::string& level);
    bool setSclkState(int stateIndex);
    bool setMclkState(int stateIndex);
    bool setPowerProfileMode(int modeIndex);

private:
    GpuInfo m_info;
    std::string m_hwmonPath;
    bool m_isAmd = false;
    bool m_isNvidia = false;
    bool m_isIntel = false;

    void scanAmdGpu(const std::string& cardPath);
    void scanNvidiaGpu();
    void scanIntelGpu(const std::string& cardPath);
};

} // namespace fc
