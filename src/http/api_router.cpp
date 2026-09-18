#pragma once
#include "auth/auth_service.hpp"
#include "services/town_service.hpp"
#include <string>

namespace tsto {
struct HttpRequest {
    std::string method;
    std::string path;
    std::string authorization;
    std::string body;
};

struct HttpResponse {
    int status{200};
    std::string body;
    std::string content_type{"application/json; charset=utf-8"};
};

class ApiRouter {
public:
    ApiRouter(AuthService&, TownService&);
    HttpResponse handle(const HttpRequest&);

private:
    AuthService& auth_;
    TownService& towns_;
};
}
