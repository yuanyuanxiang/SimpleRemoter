#include <Windows.h>
#include <string>
#include <thread>
#include <filesystem>
#include <fstream>

#undef min
#undef max
#include "httplib.h"
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#pragma comment(lib, "ws2_32.lib")

namespace fs = std::filesystem;

class FileDownloadServer {
public:
    FileDownloadServer(int port = 8080) : port_(port) {
        char exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, exe_path, MAX_PATH);
        root_dir_ = fs::path(exe_path).parent_path() / "Payloads";
        fs::create_directories(root_dir_);
    }

    bool Start() {
        server_.Get("/payloads/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
            std::string filename = req.matches[1];

            if (filename.empty() || filename.find("..") != std::string::npos) {
                res.status = 403;
                return;
            }

            fs::path filepath = root_dir_ / filename;

            if (!fs::exists(filepath) || !fs::is_regular_file(filepath)) {
                res.status = 404;
                return;
            }

            auto filesize = fs::file_size(filepath);
            std::string path_str = filepath.string();

            res.set_header("Content-Disposition",
                "attachment; filename=\"" + filename + "\"");

            res.set_content_provider(filesize, "application/octet-stream",
                [path_str](size_t offset, size_t length, httplib::DataSink& sink) {
                    std::ifstream file(path_str, std::ios::binary);
                    if (!file) return false;
                    file.seekg(offset);

                    char buffer[65536];
                    size_t remaining = length;
                    while (remaining > 0 && file) {
                        size_t to_read = (std::min)(remaining, sizeof(buffer));
                        file.read(buffer, to_read);
                        size_t n = (size_t)file.gcount();
                        if (n == 0) break;
                        sink.write(buffer, n);
                        remaining -= n;
                    }
                    return true;
                }
            );
            });

        server_thread_ = std::thread([this]() {
            server_.listen("0.0.0.0", port_);
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return server_.is_running();
    }

    void Stop() {
        server_.stop();
        if (server_thread_.joinable()) server_thread_.join();
    }

private:
    httplib::Server server_;
    fs::path root_dir_;
    int port_;
    std::thread server_thread_;
};
