// Test helper: writes a SYNTHETIC 17-map installation (not GTA 2 data) for the CLI tests.
//   gta20_write_synthetic_install <dir>
#include <iostream>

#include "synthetic_install.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: gta20_write_synthetic_install <dir>\n";
        return 2;
    }
    if (auto w = gta20::testsupport::write_synthetic_install(argv[1]); !w) {
        std::cerr << "error: " << w.error().message << "\n";
        return 1;
    }
    return 0;
}
