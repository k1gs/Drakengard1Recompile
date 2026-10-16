#include "ps2_runtime.h"
#include <exception>
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Pass the locally verified boot ELF; use scripts/run_native.py\n";
        return 2;
    }
    try {
        auto runtime = std::make_unique<PS2Runtime>();
        if (!runtime->initialize("Drakengard Europe - diagnostic runtime")) return 3;
        if (!runtime->loadELF(argv[1])) return 4;
        runtime->run();
        std::cerr << "Diagnostic run ended; this is not a playable-port acceptance result\n";
        return 5;
    } catch (const std::exception& error) {
        std::cerr << "Native blocker: " << error.what() << '\n';
        return 6;
    }
}
