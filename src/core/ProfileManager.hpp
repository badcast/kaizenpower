#pragma once

#include "Types.hpp"
#include "FanManager.hpp"
#include "CpuManager.hpp"
#include "GpuManager.hpp"
#include <memory>
#include <string>

namespace fc {

class ProfileManager {
public:
    ProfileManager(std::shared_ptr<FanManager> fanMgr,
                   std::shared_ptr<CpuManager> cpuMgr,
                   std::shared_ptr<GpuManager> gpuMgr);
    ~ProfileManager() = default;

    bool applyProfile(SystemProfile profile);
    bool restoreAuto();
    bool isAuto() const { return m_isAuto; }
    void setAuto(bool a) { m_isAuto = a; }
    void setActiveProfile(SystemProfile profile) {
        m_activeProfile = profile;
        m_isAuto = false;
    }
    SystemProfile getCurrentProfile() const { return m_activeProfile; }
    std::string getProfileName(SystemProfile profile) const;
    std::string getProfileNameEnglish(SystemProfile profile) const;
    std::string getProfileDescription(SystemProfile profile) const;

private:
    std::shared_ptr<FanManager> m_fanMgr;
    std::shared_ptr<CpuManager> m_cpuMgr;
    std::shared_ptr<GpuManager> m_gpuMgr;
    SystemProfile m_activeProfile = SystemProfile::Balanced;
    bool m_isAuto = true;
};

} // namespace fc
