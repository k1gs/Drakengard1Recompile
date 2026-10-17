#include "iso_media.h"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if ((argc != 3 && argc != 4) || std::string(argv[1]) != "--input-complete") {
        std::cerr << "Pass --input-complete image.iso [file-path] before inspecting media\n";
        return 2;
    }
    try {
        drakengard::IsoMedia media(argv[2]);
        std::cout << "sectors=" << media.sectors() << " files=" << media.fileCount();
        if (argc == 4) {
            auto file = media.find(argv[3]);
            if (!file) { std::cerr << "File not found\n"; return 3; }
            std::cout << " lsn=" << file->lsn << " size=" << file->size;
        }
        std::cout << '\n';
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
