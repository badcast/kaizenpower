#pragma once

#include "Types.hpp"
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace fc {

class FanManager {
public:
    FanManager();
    ~FanManager() = default;

    void scanFans();
    void updateLiveMetrics(double cpuTemp = 0.0, double gpuTemp = 0.0, bool applyCurves = false);

    const std::vector<FanInfo>& getFans() const { return m_fans; }
    FanInfo* getFanById(const std::string& id);

    bool setFanSpeedPercent(const std::string& fanId, int percent);
    bool setFanAutoMode(const std::string& fanId, bool autoMode);
    bool setFanControlMode(const std::string& fanId, FanControlMode mode);
    bool setFanCurvePoints(const std::string& fanId, const std::vector<CurvePoint>& points);
    bool setUserFanType(const std::string& fanId, FanType type);

    // Set all fans to a percentage or auto mode
    void setAllFansPercent(int percent);
    void setAllFansAuto();

private:
    std::vector<FanInfo> m_fans;

    FanType detectFanType(const std::string& hwmonPath,
                          const std::string& hwmonName,
                          int fanIndex,
                          const std::string& label,
                          std::string& outReason);

    int getAutoModeCode(const std::string& hwmonName) const;
};

} // namespace fc
