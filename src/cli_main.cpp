#include "cli/CliApp.hpp"

int main(int argc, char** argv) {
    fc::CliApp app;
    return app.run(argc, argv);
}
