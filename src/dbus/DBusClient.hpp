#pragma once

#include <gio/gio.h>
#include <string>
#include <memory>
#include <vector>

namespace fc {

class DBusClient {
public:
    DBusClient();
    ~DBusClient();

    bool connect();
    bool isConnected() const;

    bool getSummary(std::string& outJson);
    bool getCpuInfo(std::string& outJson);
    bool getGpuInfo(std::string& outJson);
    bool getFans(std::string& outJson);

    bool setCpuGovernor(const std::string& gov, std::string& outMsg);
    bool setCpuLimits(double minGhz, double maxGhz, std::string& outMsg);
    bool setCpuTurbo(bool enable, std::string& outMsg);
    bool setCpuEnergyPreference(const std::string& pref, std::string& outMsg);

    bool setGpuPerfLevel(const std::string& level, std::string& outMsg);
    bool setGpuClock(int sclkIdx, int mclkIdx, std::string& outMsg);
    bool setGpuPowerProfile(int profileIdx, std::string& outMsg);

    bool setFanSpeed(const std::string& fanId, int percent, std::string& outMsg);
    bool setFanMode(const std::string& fanId, const std::string& mode, std::string& outMsg);
    bool setFanCurve(const std::string& fanId, const std::string& curveJson, std::string& outMsg);

    bool setAuto(const std::string& component, std::string& outMsg);
    bool applyProfile(const std::string& profileName, std::string& outMsg);

private:
    GDBusProxy* m_proxy = nullptr;
    bool callBoolStringMethod(const char* methodName, GVariant* parameters, std::string& outMsg);
};

} // namespace fc
