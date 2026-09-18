#include "http/api_router.hpp"
namespace tsto {
ApiRouter::ApiRouter(AuthService&a,TownService&t):auth_(a),towns_(t){}
HttpResponse ApiRouter::handle(const HttpRequest& r){
    if(r.method=="POST"&&r.path=="/v1/auth/register") return {501,"register route wired; JSON adapter required"};
    if(r.method=="POST"&&r.path=="/v1/auth/login") return {501,"login route wired; JSON adapter required"};
    if(r.method=="POST"&&r.path=="/v1/devices") return {501,"device route wired; JSON adapter required"};
    if(r.method=="GET"&&r.path=="/v1/town") return {501,"town load route wired; JSON adapter required"};
    if(r.method=="PUT"&&r.path=="/v1/town") return {501,"town save route wired; JSON adapter required"};
    return {404,"not found"};
}
}
