#pragma once

#include <string>
#include <mutex>
#include <memory>

namespace fc {

class PrivilegeManager {
public:
    static PrivilegeManager& instance();

    bool isRoot() const;
    bool isAuthorized() const;
    bool requestAuthorization();
    bool writePrivileged(const std::string& path, const std::string& content);
    void shutdown();

    // The helper process entrypoint (when invoked with --helper)
    static void runHelper(const std::string& socketPath = "");

private:
    PrivilegeManager() = default;
    ~PrivilegeManager();

    PrivilegeManager(const PrivilegeManager&) = delete;
    PrivilegeManager& operator=(const PrivilegeManager&) = delete;

    mutable std::mutex m_mutex;
    int m_socketFd = -1;
    pid_t m_helperPid = -1;
    bool m_helperActive = false;
    std::string m_activeSocketPath;

    bool startHelper();
    std::string readLineFromSocket();
    bool writeLineToSocket(const std::string& line);
};

} // namespace fc
