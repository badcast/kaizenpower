#include "dbus/DBusServer.hpp"
#include "core/CpuManager.hpp"
#include "core/GpuManager.hpp"
#include "core/FanManager.hpp"
#include "core/ProfileManager.hpp"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    std::cout << "Starting Kaizen Power Tool System Daemon (kaizend)..." << std::endl;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--helper") {
            std::cout << "Notice: 'kaizend' is the D-Bus daemon service (ignoring --helper flag)" << std::endl;
        }
    }

    auto cpu = std::make_shared<fc::CpuManager>();
    auto gpu = std::make_shared<fc::GpuManager>();
    auto fan = std::make_shared<fc::FanManager>();
    auto prof = std::make_shared<fc::ProfileManager>(fan, cpu, gpu);

    auto server = std::make_shared<fc::DBusServer>(cpu, gpu, fan, prof);

    if (!server->init()) {
        std::cerr << "Failed to initialize Kaizen D-Bus service" << std::endl;
        return 1;
    }

    server->run();
    std::cout << "Kaizen Daemon terminated cleanly" << std::endl;
    return 0;
}
