#include "http/http_server.hpp"
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_type = SOCKET;
static constexpr socket_type invalid_socket = INVALID_SOCKET;
static void close_socket(socket_type socket) { closesocket(socket); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_type = int;
static constexpr socket_type invalid_socket = -1;
static void close_socket(socket_type socket) { close(socket); }
#endif

namespace tsto {
namespace {
std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    return value.substr(first);
}
std::string reason(int status) {
    switch (status) { case 200:return "OK"; case 206:return "Partial Content"; case 400:return "Bad Request"; case 403:return "Forbidden"; case 404:return "Not Found"; case 413:return "Payload Too Large"; case 500:return "Internal Server Error"; default:return "Error"; }
}
std::string content_type(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".zip") return "application/zip";
    if (ext == ".gz" || ext == ".tgz") return "application/gzip";
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".txt" || ext == ".xml") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}
void send_all(socket_type socket, const std::string& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        const auto count = ::send(socket, data.data() + offset, static_cast<int>(data.size() - offset), 0);
        if (count <= 0) return;
        offset += static_cast<std::size_t>(count);
    }
}
std::filesystem::path safe_file(const std::filesystem::path& root, const std::string& request_path, bool& unsafe) {
    unsafe = false;
    std::string relative = request_path;
    if (relative.rfind("/static/", 0) == 0) relative.erase(0, 8);
    else if (relative.rfind("/dlc/", 0) == 0) relative.erase(0, 5);
    else { unsafe = true; return {}; }
    if (relative.empty() || relative.find('\0') != std::string::npos) { unsafe = true; return {}; }
    for (char& c : relative) if (c == '\\') c = '/';
    std::filesystem::path candidate = (root / std::filesystem::path(relative)).lexically_normal();
    std::error_code error;
    const auto canonical_root = std::filesystem::weakly_canonical(root, error);
    if (error) { unsafe = true; return {}; }
    const auto canonical_file = std::filesystem::weakly_canonical(candidate, error);
    if (error) return candidate;
    auto root_text = canonical_root.generic_string();
    auto file_text = canonical_file.generic_string();
    if (file_text != root_text && file_text.rfind(root_text + "/", 0) != 0) unsafe = true;
    return canonical_file;
}
}

HttpServer::HttpServer(const Config& config, ApiRouter& router, std::filesystem::path web_root)
    : config_(config), router_(router), web_root_(std::move(web_root)), dlc_root_(config.dlc_directory) {
    std::error_code error;
    std::filesystem::create_directories(dlc_root_, error);
    if (error) throw std::runtime_error("cannot create DLC directory: " + error.message());
}

void HttpServer::run() {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
#endif
    socket_type server = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == invalid_socket) throw std::runtime_error("failed to create listening socket");
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(config_.listen_port));
    if (config_.listen_address == "0.0.0.0") address.sin_addr.s_addr = htonl(INADDR_ANY);
    else if (inet_pton(AF_INET, config_.listen_address.c_str(), &address.sin_addr) != 1) { close_socket(server); throw std::runtime_error("listen_address must be IPv4"); }
    if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 || ::listen(server, 64) < 0) { close_socket(server); throw std::runtime_error("failed to bind/listen"); }
    for (;;) {
        sockaddr_in client_address{};
#ifdef _WIN32
        int length = sizeof(client_address);
#else
        socklen_t length = sizeof(client_address);
#endif
        socket_type client = ::accept(server, reinterpret_cast<sockaddr*>(&client_address), &length);
        if (client != invalid_socket) { handle_client(static_cast<int>(client)); close_socket(client); }
    }
}

void HttpServer::handle_client(int raw_socket) {
    const socket_type socket = static_cast<socket_type>(raw_socket);
    std::string request;
    char buffer[8192]{};
    std::size_t header_end = std::string::npos;
    while (request.size() < config_.max_request_bytes && header_end == std::string::npos) {
        const auto count = ::recv(socket, buffer, sizeof(buffer), 0);
        if (count <= 0) return;
        request.append(buffer, static_cast<std::size_t>(count));
        header_end = request.find("\r\n\r\n");
    }
    if (header_end == std::string::npos) { send_all(socket, "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"); return; }
    std::istringstream headers(request.substr(0, header_end));
    std::string request_line;
    std::getline(headers, request_line);
    if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();
    HttpRequest http_request;
    std::istringstream first(request_line);
    first >> http_request.method >> http_request.path;
    if (http_request.method.empty() || http_request.path.empty()) { send_all(socket, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"); return; }
    std::size_t content_length = 0;
    std::string line;
    while (std::getline(headers, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto colon = line.find(':'); if (colon == std::string::npos) continue;
        const auto name = line.substr(0, colon); const auto value = trim(line.substr(colon + 1));
        if (name == "Authorization" || name == "authorization") http_request.authorization = value;
        if (name == "Content-Length" || name == "content-length") { try { content_length = std::stoull(value); } catch (...) { send_all(socket, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"); return; } }
    }
    if (content_length > config_.max_request_bytes) { send_all(socket, "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"); return; }
    const std::size_t body_start = header_end + 4;
    while (request.size() < body_start + content_length) { const auto count = ::recv(socket, buffer, sizeof(buffer), 0); if (count <= 0) return; request.append(buffer, static_cast<std::size_t>(count)); }
    http_request.body = request.substr(body_start, content_length);
    if (const auto query = http_request.path.find('?'); query != std::string::npos) http_request.path.resize(query);

    int status = 200;
    std::string body;
    std::string type = "application/json; charset=utf-8";
    if (http_request.path == "/" || http_request.path == "/webpanel" || http_request.path == "/webpanel/") http_request.path = "/webpanel/index.html";

    if (http_request.method == "GET" && (http_request.path.rfind("/static/", 0) == 0 || http_request.path.rfind("/dlc/", 0) == 0)) {
        bool unsafe = false;
        const auto file = safe_file(dlc_root_, http_request.path, unsafe);
        if (unsafe) { status = 403; body = "forbidden"; type = "text/plain; charset=utf-8"; }
        else if (!std::filesystem::is_regular_file(file)) { status = 404; body = "not found"; type = "text/plain; charset=utf-8"; }
        else {
            std::ifstream input(file, std::ios::binary);
            body.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            type = content_type(file);
        }
    } else if (http_request.method == "GET" && http_request.path.rfind("/webpanel/", 0) == 0) {
        const auto file = (web_root_ / http_request.path.substr(10)).lexically_normal();
        const auto root = web_root_.lexically_normal();
        const auto file_text = file.generic_string(); const auto root_text = root.generic_string();
        if (file_text.rfind(root_text, 0) != 0 || !std::filesystem::is_regular_file(file)) { status = 404; body = "not found"; type = "text/plain; charset=utf-8"; }
        else { std::ifstream input(file, std::ios::binary); body.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()); type = content_type(file); }
    } else if (http_request.path.rfind("/v1/", 0) == 0) {
        const auto response = router_.handle(http_request); status = response.status; body = response.body; type = "application/json; charset=utf-8";
    } else { status = 404; body = "not found"; type = "text/plain; charset=utf-8"; }
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason(status) << "\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size() << "\r\nCache-Control: public, max-age=3600\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n" << body;
    send_all(socket, response.str());
}
}
