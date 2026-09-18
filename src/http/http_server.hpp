#pragma once
#include "http/api_router.hpp"
#include "config/config.hpp"
#include <filesystem>
#include <string>

namespace tsto {
class HttpServer {
public:
    HttpServer(const Config& config, ApiRouter& router, std::filesystem::path web_root);
    void run();
private:
    Config config_;
    ApiRouter& router_;
    std::filesystem::path web_root_;
    void handle_client(int socket);
};
}
