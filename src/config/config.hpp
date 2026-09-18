#pragma once
#include <filesystem>
#include <string>

namespace tsto {
struct Config { std::string listen_address{"127.0.0.1"}; int listen_port{8080}; std::filesystem::path database_path{"data/tsto.db"}; std::filesystem::path town_directory{"data/towns"}; int session_ttl_seconds{2592000}; std::size_t max_request_bytes{10 * 1024 * 1024}; };
Config load_config(const std::filesystem::path& path);
}
