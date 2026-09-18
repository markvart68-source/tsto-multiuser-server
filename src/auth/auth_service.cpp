#include "auth/auth_service.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace tsto { namespace {
std::string hex(const unsigned char* data,size_t length){std::ostringstream out;for(size_t i=0;i<length;++i)out<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(data[i]);return out.str();}
std::string random_hex(size_t bytes){std::vector<unsigned char> value(bytes);if(RAND_bytes(value.data(),static_cast<int>(value.size()))!=1)throw std::runtime_error("secure random generation failed");return hex(value.data(),value.size());}
std::string sha256(const std::string& value){unsigned char digest[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(value.data()),value.size(),digest);return hex(digest,sizeof digest);}
std::string password_hash(const std::string& password){const auto salt=random_hex(16);unsigned char digest[32];if(PKCS5_PBKDF2_HMAC(password.c_str(),static_cast<int>(password.size()),reinterpret_cast<const unsigned char*>(salt.data()),static_cast<int>(salt.size()),120000,EVP_sha256(),sizeof digest,digest)!=1)throw std::runtime_error("password hashing failed");return "pbkdf2$120000$"+salt+"$"+hex(digest,sizeof digest);}
bool password_matches(const std::string& password,const std::string& stored){const auto p1=stored.find('$'),p2=stored.find('$',p1+1),p3=stored.find('$',p2+1);if(p1==std::string::npos||p2==std::string::npos||p3==std::string::npos)return false;const int rounds=std::stoi(stored.substr(p1+1,p2-p1-1));const auto salt=stored.substr(p2+1,p3-p2-1);unsigned char digest[32];if(PKCS5_PBKDF2_HMAC(password.c_str(),static_cast<int>(password.size()),reinterpret_cast<const unsigned char*>(salt.data()),static_cast<int>(salt.size()),rounds,EVP_sha256(),sizeof digest,digest)!=1)return false;return hex(digest,sizeof digest)==stored.substr(p3+1);}
std::string epoch(){return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
}
AuthService::AuthService(Database& db,const Config& config):db_(db),config_(config){}
AuthResult AuthService::register_account(std::string email,std::string password,std::string display_name,std::string device_id){if(email.empty()||password.size()<8||device_id.empty())throw std::invalid_argument("email, device_id, and an 8-character password are required");if(db_.find_account_by_email(email))throw std::runtime_error("account already exists");Account draft{0,email,password_hash(password),display_name,random_hex(16),static_cast<std::int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()&0x7fffffffffffffff)};auto account=db_.create_account(draft);if(!account)throw std::runtime_error("account creation failed");if(!db_.add_device(Device{device_id,"unknown","",account->id}))throw std::runtime_error("device registration failed");return login(email,password,device_id);}
AuthResult AuthService::login(std::string email,std::string password,std::string device_id){auto account=db_.find_account_by_email(email);if(!account||!password_matches(password,account->password_hash))throw std::runtime_error("invalid credentials");if(!db_.device_belongs_to(account->id,device_id)&&!db_.add_device(Device{device_id,"unknown","",account->id}))throw std::runtime_error("device registration failed");const auto token=random_hex(32);const auto expiry=std::stoll(epoch())+config_.session_ttl_seconds;Session session{sha256(token),device_id,account->id,std::to_string(expiry)};if(!db_.create_session(session))throw std::runtime_error("session creation failed");return {*account,session,token};}
Session AuthService::authenticate(const std::string& authorization) const {if(authorization.rfind("Bearer ",0)!=0)throw std::runtime_error("bearer token required");auto token=authorization.substr(7);if(token.empty())throw std::runtime_error("bearer token required");auto session=db_.find_session(sha256(token));if(!session)throw std::runtime_error("invalid or expired session");if(!db_.device_belongs_to(session->account_id,session->device_id))throw std::runtime_error("session device is not registered");return *session;}
}
