#include "config/config.hpp"
#include <fstream>
#include <regex>
#include <stdexcept>

namespace tsto {
namespace {
std::string string_value(const std::string& text, const char* key, const std::string& fallback) {
    std::regex expression(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    return std::regex_search(text, match, expression) ? match[1].str() : fallback;
}
int number_value(const std::string& text, const char* key, int fallback) {
    std::regex expression(std::string("\\\"") + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    return std::regex_search(text, match, expression) ? std::stoi(match[1].str()) : fallback;
}
}
Config load_config(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open config: " + path.string());
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    Config config;
    config.listen_address = string_value(text, "listen_address", config.listen_address);
    config.listen_port = number_value(text, "listen_port", config.listen_port);
    config.database_path = string_value(text, "database_path", config.database_path.string());
    config.town_directory = string_value(text, "town_directory", config.town_directory.string());
    config.dlc_directory = string_value(text, "dlc_directory", config.dlc_directory.string());
    config.session_ttl_seconds = number_value(text, "session_ttl_seconds", config.session_ttl_seconds);
    config.max_request_bytes = static_cast<std::size_t>(number_value(text, "max_request_bytes", static_cast<int>(config.max_request_bytes)));
    return config;
}
}
