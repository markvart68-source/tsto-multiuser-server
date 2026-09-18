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
std::string hex(const unsigned char*p,size_t n){std::ostringstream o;for(size_t i=0;i<n;i++)o<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)p[i];return o.str();}
std::string random_hex(size_t n){std::vector<unsigned char>b(n);if(RAND_bytes(b.data(),static_cast<int>(b.size()))!=1)throw std::runtime_error("secure random generator failed");return hex(b.data(),b.size());}
std::string hash_token(const std::string&t){unsigned char h[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(t.data()),t.size(),h);return hex(h,sizeof h);}
std::string password_hash(const std::string&p){auto salt=random_hex(16);unsigned char out[32];if(PKCS5_PBKDF2_HMAC(p.c_str(),static_cast<int>(p.size()),reinterpret_cast<const unsigned char*>(salt.data()),static_cast<int>(salt.size()),120000,EVP_sha256(),sizeof out,out)!=1)throw std::runtime_error("password hashing failed");return "pbkdf2$120000$"+salt+"$"+hex(out,sizeof out);}
bool verify(const std::string&p,const std::string&stored){auto a=stored.find('$'),b=stored.find('$',a+1),c=stored.find('$',b+1);if(a==std::string::npos||b==std::string::npos||c==std::string::npos)return false;auto salt=stored.substr(b+1,c-b-1);auto expected=stored.substr(c+1);unsigned char out[32];PKCS5_PBKDF2_HMAC(p.c_str(),static_cast<int>(p.size()),reinterpret_cast<const unsigned char*>(salt.data()),static_cast<int>(salt.size()),std::stoi(stored.substr(a+1,b-a-1)),EVP_sha256(),sizeof out,out);return hex(out,sizeof out)==expected;}
std::string numeric(){return std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+random_hex(8);}
}
AuthService::AuthService(Database&d,const Config&c):db_(d),config_(c){}
AuthResult AuthService::register_account(std::string email,std::string password,std::string display,std::string device){if(email.empty()||password.size()<8||device.empty())throw std::invalid_argument("email, device and password of at least 8 characters are required");if(db_.find_account_by_email(email))throw std::runtime_error("account already exists");Account draft{0,email,password_hash(password),display,numeric(),static_cast<std::int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()&0x7fffffffffffffff)};auto a=db_.create_account(draft);if(!a)throw std::runtime_error("account creation failed");db_.add_device(Device{device,"unknown","",a->id});return login(email,password,device);}
AuthResult AuthService::login(std::string email,std::string password,std::string device){auto a=db_.find_account_by_email(email);if(!a||!verify(password,a->password_hash))throw std::runtime_error("invalid credentials");if(!db_.device_belongs_to(a->id,device)){db_.add_device(Device{device,"unknown","",a->id});}auto raw=random_hex(32);auto exp=std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()+config_.session_ttl_seconds);Session s{hash_token(raw),device,a->id,exp};if(!db_.create_session(s))throw std::runtime_error("session creation failed");return {*a,s,raw};}
Session AuthService::authenticate(const std::string&bearer)const{if(bearer.rfind("Bearer ",0)==0)return db_.find_session(hash_token(bearer.substr(7))).value_or throw std::runtime_error("invalid or expired session");throw std::runtime_error("bearer token required");}
}
