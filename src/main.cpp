#include "http/http_server.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_type = SOCKET;
static constexpr socket_type invalid_socket = INVALID_SOCKET;
static void close_socket(socket_type s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_type = int;
static constexpr socket_type invalid_socket = -1;
static void close_socket(socket_type s) { close(s); }
#endif

namespace tsto {
namespace {
std::string trim_left(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    return value;
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return trim_left(std::move(value));
}

std::string reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "Service Unavailable";
    }
}

std::string content_type(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

void send_all(socket_type socket, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const auto count = ::send(socket, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
        if (count <= 0) {
            return;
        }
        sent += static_cast<std::size_t>(count);
    }
}
}

HttpServer::HttpServer(const Config& config, ApiRouter& router, std::filesystem::path web_root)
    : config_(config), router_(router), web_root_(std::move(web_root)) {}

void HttpServer::run() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    const socket_type server = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == invalid_socket) {
        throw std::runtime_error("failed to create listening socket");
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(config_.listen_port));
    if (config_.listen_address == "0.0.0.0") {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, config_.listen_address.c_str(), &address.sin_addr) != 1) {
        close_socket(server);
        throw std::runtime_error("listen_address must be a valid IPv4 address");
    }

    if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 || ::listen(server, 64) < 0) {
        close_socket(server);
        throw std::runtime_error("failed to bind/listen to configured address");
    }

    for (;;) {
        sockaddr_in client_address{};
#ifdef _WIN32
        int length = sizeof(client_address);
#else
        socklen_t length = sizeof(client_address);
#endif
        const socket_type client = ::accept(server, reinterpret_cast<sockaddr*>(&client_address), &length);
        if (client == invalid_socket) {
            continue;
        }
        handle_client(static_cast<int>(client));
        close_socket(client);
    }
}

void HttpServer::handle_client(int raw_socket) {
    const socket_type socket = static_cast<socket_type>(raw_socket);
    std::string request;
    char buffer[8192] = {};
    std::size_t header_end = std::string::npos;

    while (request.size() < config_.max_request_bytes && header_end == std::string::npos) {
        const auto n = ::recv(socket, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            return;
        }
        request.append(buffer, static_cast<std::size_t>(n));
        header_end = request.find("\r\n\r\n");
    }
    if (header_end == std::string::npos) {
        send_all(socket, "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        return;
    }

    const std::string header_text = request.substr(0, header_end);
    std::istringstream lines(header_text);
    std::string request_line;
    std::getline(lines, request_line);
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream parts(request_line);
    std::string method, path;
    parts >> method >> path;
    if (method.empty() || path.empty()) {
        send_all(socket, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        return;
    }

    HttpRequest http_request;
    http_request.method = method;
    http_request.path = path;

    std::size_t content_length = 0;
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string name = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (name == "Authorization" || name == "authorization") {
            http_request.authorization = value;
        }
        if (name == "Content-Length" || name == "content-length") {
            try {
                content_length = std::stoull(value);
            } catch (...) {
                content_length = 0;
            }
        }
    }

    if (content_length > config_.max_request_bytes) {
        send_all(socket, "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        return;
    }

    const std::size_t body_offset = header_end + 4;
    while (request.size() < body_offset + content_length) {
        const auto n = ::recv(socket, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            return;
        }
        request.append(buffer, static_cast<std::size_t>(n));
    }

    http_request.body = request.substr(body_offset, content_length);
    const std::size_t query_index = http_request.path.find('?');
    if (query_index != std::string::npos) {
        http_request.path = http_request.path.substr(0, query_index);
    }

    std::string payload;
    int status = 200;
    std::string content_type_value = "application/json; charset=utf-8";

    if (http_request.path == "/" || http_request.path == "/webpanel" || http_request.path == "/webpanel/") {
        http_request.path = "/webpanel/index.html";
    }

    if (http_request.path.rfind("/webpanel/", 0) == 0 && http_request.method == "GET") {
        std::filesystem::path file = web_root_ / http_request.path.substr(std::string("/webpanel/").size());
        file = file.lexically_normal();
        const auto root = web_root_.lexically_normal();
        const auto file_native = file.generic_string();
        const auto root_native = root.generic_string();
        if (file_native.rfind(root_native, 0) != 0 || !std::filesystem::is_regular_file(file)) {
            status = 404;
            payload = "not found";
            content_type_value = "text/plain; charset=utf-8";
        } else {
            std::ifstream input(file, std::ios::binary);
            payload.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            content_type_value = content_type(file);
        }
    } else if (http_request.path.rfind("/v1/", 0) == 0) {
        const auto response = router_.handle(http_request);
        status = response.status;
        payload = response.body;
        content_type_value = response.content_type;
    } else {
        status = 404;
        payload = "not found";
        content_type_value = "text/plain; charset=utf-8";
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason(status) << "\r\n"
             << "Content-Type: " << content_type_value << "\r\n"
             << "Content-Length: " << payload.size() << "\r\n"
             << "Connection: close\r\n"
             << "X-Content-Type-Options: nosniff\r\n\r\n"
             << payload;
    send_all(socket, response.str());
}
}
