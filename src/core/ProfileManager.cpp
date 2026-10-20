#include "ProfileManager.hpp"
#include "I18n.hpp"
#include <iostream>

namespace fc {

ProfileManager::ProfileManager(std::shared_ptr<FanManager> fanMgr,
                               std::shared_ptr<CpuManager> cpuMgr,
                               std::shared_ptr<GpuManager> gpuMgr)
    : m_fanMgr(std::move(fanMgr))
    , m_cpuMgr(std::move(cpuMgr))
    , m_gpuMgr(std::move(gpuMgr)) {}

std::string ProfileManager::getProfileName(SystemProfile profile) const {
    switch (profile) {
        case SystemProfile::Silent: return _("Quiet (Silent)");
        case SystemProfile::Balanced: return _("Balanced");
        case SystemProfile::Performance: return _("Performance");
        case SystemProfile::MaxCooling: return _("Max Cooling");
    }
    return _("Unknown");
}

std::string ProfileManager::getProfileNameEnglish(SystemProfile profile) const {
    switch (profile) {
        case SystemProfile::Silent: return "Silent";
        case SystemProfile::Balanced: return "Balanced";
        case SystemProfile::Performance: return "Performance";
        case SystemProfile::MaxCooling: return "Max Cooling (100% Fans)";
    }
    return "Unknown";
}

std::string ProfileManager::getProfileDescription(SystemProfile profile) const {
    switch (profile) {
        case SystemProfile::Silent:
            return _("Minimum noise: Powersave mode for CPU, fan speeds at 30%, conservative GPU clocks.");
        case SystemProfile::Balanced:
            return _("Optimal balance: Automatic fan curve, adaptive frequencies for processor and graphics card.");
        case SystemProfile::Performance:
            return _("Maximum power: Performance governor for CPU, Turbo Boost enabled, GPU High, fans at 80%.");
        case SystemProfile::MaxCooling:
            return _("Extreme cooling: All fans (CPU, GPU, chassis) spin at 100% for rapid heat dissipation.");
    }
    return "";
}

bool ProfileManager::applyProfile(SystemProfile profile) {
    m_activeProfile = profile;
    m_isAuto = false;
    std::cout << "[KAIZEN] Profile changed to: " << getProfileNameEnglish(profile) << std::endl;

    switch (profile) {
        case SystemProfile::Silent:
            std::cout << "[KAIZEN] Applying Silent profile (CPU: powersave, Fans: 30%, GPU: low)" << std::endl;
            if (m_cpuMgr) {
                m_cpuMgr->setGovernor("powersave");
                m_cpuMgr->setEnergyPreference("power");
            }
            if (m_fanMgr) {
                m_fanMgr->setAllFansPercent(30);
            }
            if (m_gpuMgr && m_gpuMgr->getGpuInfo().present) {
                m_gpuMgr->setPerformanceLevel("low");
            }
            return true;

        case SystemProfile::Balanced:
            std::cout << "[KAIZEN] Applying Balanced profile (CPU: powersave/balance_performance, GPU: auto, Fans: auto)" << std::endl;
            if (m_cpuMgr) {
                m_cpuMgr->setGovernor("powersave");
                m_cpuMgr->setEnergyPreference("balance_performance");
            }
            if (m_fanMgr) {
                m_fanMgr->setAllFansAuto();
            }
            if (m_gpuMgr && m_gpuMgr->getGpuInfo().present) {
                m_gpuMgr->setPerformanceLevel("auto");
            }
            return true;

        case SystemProfile::Performance:
            std::cout << "[KAIZEN] Applying Performance profile (CPU: performance, Turbo Boost ON, Fans: 80%, GPU: high)" << std::endl;
            if (m_cpuMgr) {
                m_cpuMgr->setGovernor("performance");
                m_cpuMgr->setEnergyPreference("performance");
                m_cpuMgr->setTurboBoost(true);
            }
            if (m_fanMgr) {
                m_fanMgr->setAllFansPercent(80);
            }
            if (m_gpuMgr && m_gpuMgr->getGpuInfo().present) {
                m_gpuMgr->setPerformanceLevel("high");
            }
            return true;

        case SystemProfile::MaxCooling:
            std::cout << "[KAIZEN] Applying Max Cooling profile (Extreme dissipation: all fans at 100%)" << std::endl;
            if (m_fanMgr) {
                m_fanMgr->setAllFansPercent(100);
            }
            return true;
    }
    return false;
}

bool ProfileManager::restoreAuto() {
    m_isAuto = true;
    std::cout << "[KAIZEN] Profile changed to: Factory Auto (Restoring default hardware control)" << std::endl;
    if (m_cpuMgr) {
        m_cpuMgr->setGovernor("schedutil");
        m_cpuMgr->setTurboBoost(true);
        m_cpuMgr->setEnergyPreference("balance_performance");
    }
    if (m_gpuMgr && m_gpuMgr->getGpuInfo().present) {
        m_gpuMgr->setPerformanceLevel("auto");
    }
    if (m_fanMgr) {
        m_fanMgr->setAllFansAuto();
    }
    return true;
}

} // namespace fc
