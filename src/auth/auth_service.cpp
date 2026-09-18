#include "auth/auth_service.hpp"
#include <stdexcept>
namespace tsto {
AuthService::AuthService(Database& d,const Config& c):db_(d),config_(c){}
AuthResult AuthService::register_account(std::string,std::string,std::string,std::string){ throw std::runtime_error("register implementation pending"); }
AuthResult AuthService::login(std::string,std::string,std::string){ throw std::runtime_error("login implementation pending"); }
Session AuthService::authenticate(const std::string&) const { throw std::runtime_error("authentication implementation pending"); }
}
