#pragma once
#include "database/database.hpp"
#include "config/config.hpp"
#include <string>
namespace tsto { struct AuthResult{Account account;Session session;std::string access_token;}; class AuthService{public:AuthService(Database&,const Config&);AuthResult register_account(std::string,std::string,std::string,std::string);AuthResult login(std::string,std::string,std::string);void register_device(const Session&,std::string,std::string,std::string);Session authenticate(const std::string&)const;private:Database&db_;Config config_;}; }
