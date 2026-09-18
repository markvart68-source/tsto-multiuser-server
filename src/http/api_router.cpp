#include "http/api_router.hpp"
#include <cctype>
#include <stdexcept>

namespace tsto { namespace {
std::string json_escape(const std::string& value) { std::string out; for (char c : value) { if (c == '"') out += "\\\""; else if (c == '\\') out += "\\\\"; else if (c == '\n') out += "\\n"; else if (c == '\r') out += "\\r"; else out += c; } return out; }
std::string field(const std::string& json, const std::string& key) {
    const auto marker = "\"" + key + "\""; auto p = json.find(marker); if (p == std::string::npos) return {};
    p = json.find(':', p + marker.size()); if (p == std::string::npos) return {}; ++p; while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p]))) ++p;
    if (p < json.size() && json[p] == '"') { ++p; std::string out; bool escaped = false; for (; p < json.size(); ++p) { char c = json[p]; if (escaped) { out += c; escaped = false; } else if (c == '\\') escaped = true; else if (c == '"') break; else out += c; } return out; }
    const auto end = json.find_first_of(",}", p); return json.substr(p, end == std::string::npos ? json.size() - p : end - p);
}
HttpResponse error_response(int status, const std::string& message) { return {status, "{\"error\":\"" + json_escape(message) + "\"}"}; }
}
ApiRouter::ApiRouter(AuthService& auth, TownService& towns) : auth_(auth), towns_(towns) {}
HttpResponse ApiRouter::handle(const HttpRequest& request) {
    try {
        if (request.method == "POST" && request.path == "/v1/auth/register") { auto r = auth_.register_account(field(request.body,"email"), field(request.body,"password"), field(request.body,"display_name"), field(request.body,"device_id")); return {201, "{\"account_id\":" + std::to_string(r.account.id) + ",\"token\":\"" + r.access_token + "\"}"}; }
        if (request.method == "POST" && request.path == "/v1/auth/login") { auto r = auth_.login(field(request.body,"email"), field(request.body,"password"), field(request.body,"device_id")); return {200, "{\"account_id\":" + std::to_string(r.account.id) + ",\"token\":\"" + r.access_token + "\"}"}; }
        const auto session = auth_.authenticate(request.authorization);
        if (request.method == "POST" && request.path == "/v1/devices") { auth_.register_device(session, field(request.body,"device_id"), field(request.body,"platform"), field(request.body,"app_version")); return {201, "{\"registered\":true}"}; }
        if (request.method == "GET" && request.path == "/v1/town") { const auto town = towns_.load(session.account_id); return {200, "{\"revision\":" + std::to_string(town.revision) + ",\"payload\":\"" + json_escape(town.payload) + "\"}"}; }
        if (request.method == "PUT" && request.path == "/v1/town") { const auto revision = std::stoll(field(request.body,"expected_revision")); const auto town = towns_.save(session.account_id, revision, field(request.body,"payload")); return {200, "{\"revision\":" + std::to_string(town.revision) + "}"}; }
        return error_response(404, "not_found");
    } catch (const TownConflict&) { return error_response(409, "revision_conflict"); }
      catch (const std::invalid_argument& e) { return error_response(400, e.what()); }
      catch (const std::exception& e) { return error_response(401, e.what()); }
}
}
