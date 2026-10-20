#include "PrivilegeManager.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <array>
#include <cstring>
#include <poll.h>

namespace fc {

PrivilegeManager& PrivilegeManager::instance() {
    static PrivilegeManager s_instance;
    return s_instance;
}

PrivilegeManager::~PrivilegeManager() {
    shutdown();
}

bool PrivilegeManager::isRoot() const {
    return geteuid() == 0;
}

bool PrivilegeManager::isAuthorized() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return isRoot() || m_helperActive;
}

bool PrivilegeManager::startHelper() {
    if (m_helperActive) return true;

    // 1. Create a dedicated UNIX domain listening socket
    int listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd < 0) return false;

    std::string sockPath = "/tmp/kaizen_priv_" + std::to_string(getpid()) + ".sock";
    unlink(sockPath.c_str());

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(listenFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listenFd);
        unlink(sockPath.c_str());
        return false;
    }

    if (listen(listenFd, 1) < 0) {
        close(listenFd);
        unlink(sockPath.c_str());
        return false;
    }

    // 2. Discover our own executable path
    std::array<char, 1024> exeBuf;
    ssize_t len = readlink("/proc/self/exe", exeBuf.data(), exeBuf.size() - 1);
    if (len <= 0) {
        close(listenFd);
        unlink(sockPath.c_str());
        return false;
    }
    exeBuf[len] = '\0';
    std::string exePath(exeBuf.data());

    // 3. Fork and run pkexec WITHOUT redirecting STDIN/STDOUT
    // Keeping STDIN/STDOUT intact is essential so polkit can prompt the user for password!
    pid_t pid = fork();
    if (pid < 0) {
        close(listenFd);
        unlink(sockPath.c_str());
        return false;
    }

    if (pid == 0) {
        close(listenFd);
        execlp("pkexec", "pkexec", exePath.c_str(), "--helper", sockPath.c_str(), nullptr);
        _exit(127);
    }

    m_helperPid = pid;
    m_activeSocketPath = sockPath;

    // 4. Wait up to 60 seconds for pkexec to authenticate and the child helper to connect
    struct pollfd pfd;
    pfd.fd = listenFd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, 60000);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        int clientFd = accept(listenFd, nullptr, nullptr);
        close(listenFd);
        unlink(sockPath.c_str());

        if (clientFd >= 0) {
            m_socketFd = clientFd;
            // Handshake
            if (writeLineToSocket("PING")) {
                std::string reply = readLineFromSocket();
                if (reply == "PONG") {
                    m_helperActive = true;
                    return true;
                }
            }
            close(m_socketFd);
            m_socketFd = -1;
        }
    } else {
        close(listenFd);
        unlink(sockPath.c_str());
    }

    // Authentication failed, cancelled, or timed out
    m_helperActive = false;
    if (m_socketFd >= 0) {
        close(m_socketFd);
        m_socketFd = -1;
    }
    if (m_helperPid > 0) {
        kill(m_helperPid, SIGTERM);
        waitpid(m_helperPid, nullptr, 0);
        m_helperPid = -1;
    }
    return false;
}

bool PrivilegeManager::requestAuthorization() {
    if (isRoot()) return true;
    std::lock_guard<std::mutex> lock(m_mutex);
    return startHelper();
}

std::string PrivilegeManager::readLineFromSocket() {
    std::string line;
    char ch;
    while (read(m_socketFd, &ch, 1) == 1) {
        if (ch == '\n') break;
        line += ch;
    }
    return line;
}

bool PrivilegeManager::writeLineToSocket(const std::string& line) {
    std::string data = line + "\n";
    ssize_t written = write(m_socketFd, data.c_str(), data.size());
    return written == static_cast<ssize_t>(data.size());
}

bool PrivilegeManager::writePrivileged(const std::string& path, const std::string& content) {
    if (isRoot()) {
        std::ofstream ofs(path);
        if (ofs.is_open()) {
            ofs << content;
            return ofs.good();
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // CRITICAL: NEVER automatically spawn pkexec from writePrivileged!
    // If authorization wasn't explicitly requested by the user, return false.
    if (!m_helperActive || m_socketFd < 0) {
        return false;
    }

    // Send WRITE command: WRITE <path> <content>
    std::string cmd = "WRITE " + path + " " + content;
    if (!writeLineToSocket(cmd)) {
        m_helperActive = false;
        return false;
    }

    std::string reply = readLineFromSocket();
    return (reply == "OK");
}

void PrivilegeManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_helperActive && m_socketFd >= 0) {
        writeLineToSocket("QUIT");
        close(m_socketFd);
        m_socketFd = -1;
        m_helperActive = false;
    }
    if (m_helperPid > 0) {
        kill(m_helperPid, SIGTERM);
        waitpid(m_helperPid, nullptr, WNOHANG);
        m_helperPid = -1;
    }
    if (!m_activeSocketPath.empty()) {
        unlink(m_activeSocketPath.c_str());
        m_activeSocketPath.clear();
    }
}

void PrivilegeManager::runHelper(const std::string& socketPath) {
    // Running as root under pkexec
    if (socketPath.empty()) {
        // Fallback standard stdin/stdout mode
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            if (line == "PING") {
                std::cout << "PONG\n" << std::flush;
            } else if (line == "QUIT") {
                break;
            } else if (line.find("WRITE ") == 0) {
                std::string rest = line.substr(6);
                auto spacePos = rest.find(' ');
                if (spacePos != std::string::npos) {
                    std::string path = rest.substr(0, spacePos);
                    std::string val = rest.substr(spacePos + 1);
                    std::ofstream ofs(path);
                    if (ofs.is_open()) {
                        ofs << val;
                        std::cout << (ofs.good() ? "OK\n" : "ERR\n") << std::flush;
                    } else {
                        std::cout << "ERR\n" << std::flush;
                    }
                } else {
                    std::cout << "ERR\n" << std::flush;
                }
            }
        }
        return;
    }

    // Connect to the parent's dedicated UNIX domain socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return;
    }

    std::string line;
    char ch;
    while (read(fd, &ch, 1) == 1) {
        if (ch != '\n') {
            line += ch;
            continue;
        }

        if (line == "PING") {
            const char pong[] = "PONG\n";
            ssize_t w = write(fd, pong, sizeof(pong) - 1);
            (void)w;
        } else if (line == "QUIT") {
            break;
        } else if (line.find("WRITE ") == 0) {
            std::string rest = line.substr(6);
            auto spacePos = rest.find(' ');
            if (spacePos != std::string::npos) {
                std::string path = rest.substr(0, spacePos);
                std::string val = rest.substr(spacePos + 1);
                std::ofstream ofs(path);
                if (ofs.is_open()) {
                    ofs << val;
                    if (ofs.good()) {
                        const char ok[] = "OK\n";
                        ssize_t w = write(fd, ok, sizeof(ok) - 1);
                        (void)w;
                    } else {
                        const char err[] = "ERR\n";
                        ssize_t w = write(fd, err, sizeof(err) - 1);
                        (void)w;
                    }
                } else {
                    const char err[] = "ERR\n";
                    ssize_t w = write(fd, err, sizeof(err) - 1);
                    (void)w;
                }
            } else {
                const char err[] = "ERR\n";
                ssize_t w = write(fd, err, sizeof(err) - 1);
                (void)w;
            }
        }
        line.clear();
    }

    close(fd);
}

} // namespace fc
