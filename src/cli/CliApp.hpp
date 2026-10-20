#pragma once

#include "../dbus/DBusClient.hpp"
#include <string>
#include <vector>
#include <memory>

namespace fc {

class CliApp {
public:
    CliApp();
    ~CliApp();

    int run(int argc, char** argv);

private:
    bool m_useColor = true;
    std::shared_ptr<DBusClient> m_client;

    void printHelp(const char* progName);
    void printSummary(const std::string& json);
    void printCpu(const std::string& json);
    void printGpu(const std::string& json);
    void printFans(const std::string& json);

    std::string colorize(const std::string& text, const char* ansiCode);
    std::string bold(const std::string& text);
    std::string cyan(const std::string& text);
    std::string green(const std::string& text);
    std::string yellow(const std::string& text);
    std::string red(const std::string& text);
    std::string magenta(const std::string& text);
};

} // namespace fc
