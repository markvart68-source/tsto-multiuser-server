#pragma once
#include "auth/auth_service.hpp"
#include "services/town_service.hpp"
#include <string>
namespace tsto {
struct HttpRequest { std::string method,path,authorization,body; };
struct HttpResponse { int status{200}; std::string body; };
class ApiRouter { public: ApiRouter(AuthService&,TownService&); HttpResponse handle(const HttpRequest&); private: AuthService& auth_; TownService& towns_; };
}
