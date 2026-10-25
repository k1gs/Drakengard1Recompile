#include "pad_control_file.h"
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
void check(bool value,const std::source_location where=std::source_location::current()) {
    if (!value) throw std::runtime_error("pad command file regression failed at line "+std::to_string(where.line()));
}
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("unreadable or oversized command was accepted");
}
struct Files {
    std::filesystem::path directory;
    Files() {
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        const auto base=std::filesystem::temp_directory_path();
        for (unsigned i=0;i<100;++i) {
            auto candidate=base/("drakengard-pad-file-"+std::to_string(stamp)+"-"+std::to_string(i));
            if (std::filesystem::create_directory(candidate)) { directory=std::move(candidate); return; }
        }
        throw std::runtime_error("could not isolate pad file regression");
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(directory,ignored);
    }
};
void write(const std::filesystem::path& path,const std::string& text) {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    stream.write(text.data(),static_cast<std::streamsize>(text.size()));
    stream.close();
    check(bool(stream));
}
std::string_view read(const std::filesystem::path& path,drakengard::PadControlFileBuffer& buffer) {
    const auto length=drakengard::readPadControlFile(path.string().c_str(),buffer);
    return {buffer.data(),length};
}
#ifdef _WIN32
struct RenameOwner {
    HANDLE handle;
    explicit RenameOwner(const std::filesystem::path& path)
        : handle(CreateFileW(path.c_str(),DELETE,
              FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
              nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)) {
        check(handle!=INVALID_HANDLE_VALUE);
    }
    ~RenameOwner() { CloseHandle(handle); }
    RenameOwner(const RenameOwner&)=delete;
    RenameOwner& operator=(const RenameOwner&)=delete;
};
#endif

void validCommandsSurviveRenameOwnershipAndReplacement() {
    Files files;
    const auto path=files.directory/"pad.txt",next=files.directory/"next.txt";
    write(path,"D1PAD1 1 BFFF 1000\n");
    drakengard::PadControlFileBuffer buffer;
    drakengard::PadControl pad;
    {
#ifdef _WIN32
        // Simulate the renamer's DELETE handle, not a second concurrent rename.
        // The old CRT reader fails here; the new reader must consume the press.
        RenameOwner owner(path);
#endif
        check(pad.sample(read(path,buffer),10)==0xbfff);
    }
    write(next,"D1PAD1 2 FFFF 0\n");
#ifdef _WIN32
    check(MoveFileExW(next.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)!=0);
#else
    std::filesystem::rename(next,path);
#endif
    check(pad.sample(read(path,buffer),11)==0xffff);
}

void fullBoundaryAndInvalidFilesRemainStrict() {
    Files files;
    const auto path=files.directory/"pad.txt";
    const std::string tail="3 BFFF 1\n";
    const std::string boundary="D1PAD1 "+std::string(128-7-tail.size(),'0')+tail;
    write(path,boundary);
    drakengard::PadControlFileBuffer buffer;
    drakengard::PadControl pad;
    check(pad.sample(read(path,buffer),42)==0xbfff);
    // Do not accept a valid-looking prefix by clipping the physical file.
    write(path,"D1PAD1 4 FFFF 0\n"+std::string(129,'x'));
    refuses([&] { read(path,buffer); });
    refuses([&] { read(files.directory/"missing.txt",buffer); });
    write(path,"INVALID HOST PAD COMMAND\n");
    refuses([&] { pad.sample(read(path,buffer),43); });
}
}

int main() {
    try {
        validCommandsSurviveRenameOwnershipAndReplacement();
        fullBoundaryAndInvalidFilesRemainStrict();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
