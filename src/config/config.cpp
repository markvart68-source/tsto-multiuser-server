#include "config/config.hpp"
#include <fstream>
#include <stdexcept>
#include <regex>

namespace tsto {
static std::string value(const std::string& s, const char* key, const std::string& fallback) {
    std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\""); std::smatch m;
    return std::regex_search(s, m, r) ? m[1].str() : fallback;
}
static int number(const std::string& s, const char* key, int fallback) {
    std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*([0-9]+)"); std::smatch m;
    return std::regex_search(s, m, r) ? std::stoi(m[1].str()) : fallback;
}
Config load_config(const std::filesystem::path& path) {
    std::ifstream in(path); if (!in) throw std::runtime_error("Cannot open config: " + path.string());
    std::string s((std::istreambuf_iterator<char>(in)), {}); Config c;
    c.listen_address=value(s,"listen_address",c.listen_address); c.listen_port=number(s,"listen_port",c.listen_port);
    c.database_path=value(s,"database_path",c.database_path.string()); c.town_directory=value(s,"town_directory",c.town_directory.string());
    c.session_ttl_seconds=number(s,"session_ttl_seconds",c.session_ttl_seconds); c.max_request_bytes=static_cast<std::size_t>(number(s,"max_request_bytes",static_cast<int>(c.max_request_bytes))); return c;
}
}
