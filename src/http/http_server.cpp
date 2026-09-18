#include "http/api_router.hpp"
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tsto {
namespace {

std::string escape_json(const std::string& value) {
    std::string output;
    output.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch));
                    output += oss.str();
                } else {
                    output += ch;
                }
                break;
        }
    }
    return output;
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Object, Array };
    Type type{Type::Null};
    std::string string_value;
    long long number_value{0};
    bool bool_value{false};
    std::map<std::string, JsonValue> object_value;
    std::vector<JsonValue> array_value;
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    JsonValue parse() {
        skip_ws();
        JsonValue value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) {
            throw std::invalid_argument("unexpected trailing JSON data");
        }
        return value;
    }

private:
    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_)])) {
            ++pos_;
        }
    }

    char peek() const {
        if (pos_ >= text_.size()) {
            throw std::invalid_argument("unexpected end of JSON payload");
        }
        return text_[pos_];
    }

    void expect(char expected) {
        if (peek() != expected) {
            throw std::invalid_argument(std::string("JSON parse error: expected '") + expected + "'");
        }
        ++pos_;
    }

    JsonValue parse_value() {
        skip_ws();
        const char ch = peek();
        switch (ch) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return JsonValue{JsonValue::Type::String, parse_string()};
            case 't': return parse_literal("true", JsonValue::Type::Bool, true);
            case 'f': return parse_literal("false", JsonValue::Type::Bool, false);
            case 'n': return parse_literal("null", JsonValue::Type::Null, false);
            default:
                if ((ch == '-') || std::isdigit(static_cast<unsigned char>(ch))) {
                    return JsonValue{JsonValue::Type::Number, {}, parse_number()};
                }
                throw std::invalid_argument("unexpected token in JSON payload");
        }
    }

    JsonValue parse_object() {
        JsonValue object;
        object.type = JsonValue::Type::Object;
        expect('{');
        skip_ws();
        if (peek() == '}') {
            ++pos_;
            return object;
        }
        for (;;) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            JsonValue value = parse_value();
            object.object_value.emplace(std::move(key), std::move(value));
            skip_ws();
            char next = peek();
            if (next == '}') {
                ++pos_;
                break;
            }
            expect(',');
        }
        return object;
    }

    JsonValue parse_array() {
        JsonValue arr;
        arr.type = JsonValue::Type::Array;
        expect('[');
        skip_ws();
        if (peek() == ']') {
            ++pos_;
            return arr;
        }
        for (;;) {
            arr.array_value.push_back(parse_value());
            skip_ws();
            if (peek() == ']') {
                ++pos_;
                break;
            }
            expect(',');
        }
        return arr;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return out;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    throw std::invalid_argument("unterminated escape sequence in JSON string");
                }
                const char esc = text_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) {
                            throw std::invalid_argument("invalid unicode escape in JSON string");
                        }
                        std::string hex = text_.substr(pos_, 4);
                        pos_ += 4;
                        std::istringstream iss(hex);
                        unsigned int code = 0;
                        iss >> std::hex >> code;
                        if (code <= 0x7F) {
                            out.push_back(static_cast<char>(code));
                        } else if (code <= 0x7FF) {
                            out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
                            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default:
                        throw std::invalid_argument("unsupported escape sequence in JSON string");
                }
            } else {
                out.push_back(ch);
            }
        }
        throw std::invalid_argument("unterminated JSON string");
    }

    long long parse_number() {
        std::size_t start = pos_;
        if (text_[pos_] == '-') ++pos_;
        if (pos_ < text_.size() && text_[pos_] == '0') ++pos_;
        else {
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        const std::string token = text_.substr(start, pos_ - start);
        try {
            return std::stoll(token);
        } catch (...) {
            throw std::invalid_argument("invalid JSON numeric literal");
        }
    }

    JsonValue parse_literal(const std::string& literal, JsonValue::Type type, bool value) {
        if (text_.compare(pos_, literal.size(), literal) != 0) {
            throw std::invalid_argument("invalid JSON literal");
        }
        pos_ += literal.size();
        JsonValue out;
        out.type = type;
        out.bool_value = value;
        return out;
    }

    std::string text_;
    std::size_t pos_{0};
};

JsonValue parse_json(const std::string& body) {
    JsonParser parser(body);
    return parser.parse();
}

const JsonValue* member(const JsonValue& object, const std::string& key) {
    if (object.type != JsonValue::Type::Object) {
        return nullptr;
    }
    auto it = object.object_value.find(key);
    return it == object.object_value.end() ? nullptr : &it->second;
}

std::string require_string(const JsonValue& object, const std::string& key) {
    const JsonValue* value = member(object, key);
    if (!value || value->type != JsonValue::Type::String) {
        throw std::invalid_argument("field '" + key + "' must be a string");
    }
    return value->string_value;
}

long long require_number(const JsonValue& object, const std::string& key) {
    const JsonValue* value = member(object, key);
    if (!value || value->type != JsonValue::Type::Number) {
        throw std::invalid_argument("field '" + key + "' must be a number");
    }
    return value->number_value;
}

HttpResponse json_response(int status, const std::string& payload) {
    HttpResponse response;
    response.status = status;
    response.content_type = "application/json; charset=utf-8";
    response.body = payload;
    return response;
}

HttpResponse error_response(int status, const std::string& message) {
    std::ostringstream body;
    body << "{\"error\":\"" << escape_json(message) << "\"}";
    return json_response(status, body.str());
}

std::string account_json(const Account& account) {
    std::ostringstream out;
    out << "{\"id\":" << account.id << ",\"email\":\"" << escape_json(account.email)
        << "\",\"display_name\":\"" << escape_json(account.display_name)
        << "\",\"user_id\":\"" << escape_json(account.user_id) << "\"}";
    return out.str();
}

std::string currencies_json(const std::map<std::string, std::int64_t>& items) {
    std::ostringstream out;
    out << "{\"currencies\":{";
    bool first = true;
    for (const auto& [currency, amount] : items) {
        if (!first) out << ',';
        first = false;
        out << "\"" << escape_json(currency) << "\":" << amount;
    }
    out << "}}";
    return out.str();
}

std::string town_json(const Town& town) {
    std::ostringstream out;
    out << "{\"account_id\":" << town.account_id << ",\"revision\":" << town.revision
        << ",\"payload\":\"" << escape_json(town.payload) << "\"}";
    return out.str();
}

}

ApiRouter::ApiRouter(AuthService& auth, TownService& towns) : auth_(auth), towns_(towns) {}

HttpResponse ApiRouter::handle(const HttpRequest& request) {
    try {
        if (request.path == "/v1/auth/register") {
            const JsonValue json = parse_json(request.body);
            const auto& object = json.object_value;
            const std::string email = require_string(json, "email");
            const std::string password = require_string(json, "password");
            const std::string display_name = require_string(json, "display_name");
            const std::string device_id = require_string(json, "device_id");
            const auto result = auth_.register_account(email, password, display_name, device_id);
            std::ostringstream body;
            body << "{\"account_id\":" << result.account.id << ",\"token\":\"" << escape_json(result.access_token) << "\"}";
            return json_response(201, body.str());
        }

        if (request.path == "/v1/auth/login") {
            const JsonValue json = parse_json(request.body);
            const std::string email = require_string(json, "email");
            const std::string password = require_string(json, "password");
            const std::string device_id = require_string(json, "device_id");
            const auto result = auth_.login(email, password, device_id);
            std::ostringstream body;
            body << "{\"account_id\":" << result.account.id << ",\"token\":\"" << escape_json(result.access_token) << "\"}";
            return json_response(200, body.str());
        }

        if (request.path == "/v1/devices" && request.method == "POST") {
            const JsonValue json = parse_json(request.body);
            const Session session = auth_.authenticate(request.authorization);
            const std::string device_id = require_string(json, "device_id");
            const std::string platform = require_string(json, "platform");
            const std::string app_version = require_string(json, "app_version");
            auth_.register_device(session, device_id, platform, app_version);
            return json_response(201, "{\"registered\":true}");
        }

        if (request.method == "GET" && request.path == "/v1/me") {
            const Session session = auth_.authenticate(request.authorization);
            return json_response(200, account_json(auth_.account(session.account_id)));
        }

        if (request.method == "GET" && request.path == "/v1/currencies") {
            const Session session = auth_.authenticate(request.authorization);
            return json_response(200, currencies_json(auth_.currencies(session.account_id)));
        }

        if (request.method == "PUT" && request.path == "/v1/currencies") {
            const Session session = auth_.authenticate(request.authorization);
            const JsonValue json = parse_json(request.body);
            const std::string currency = require_string(json, "currency");
            const long long amount = require_number(json, "amount");
            auth_.set_currency(session.account_id, currency, amount);
            return json_response(200, currencies_json(auth_.currencies(session.account_id)));
        }

        if (request.method == "GET" && request.path == "/v1/town") {
            const Session session = auth_.authenticate(request.authorization);
            return json_response(200, town_json(towns_.load(session.account_id)));
        }

        if (request.method == "PUT" && request.path == "/v1/town") {
            const Session session = auth_.authenticate(request.authorization);
            const JsonValue json = parse_json(request.body);
            const std::int64_t expected_revision = require_number(json, "expected_revision");
            const std::string payload = require_string(json, "payload");
            const Town town = towns_.save(session.account_id, expected_revision, payload);
            return json_response(200, town_json(town));
        }

        return error_response(404, "not found");
    } catch (const std::invalid_argument& ex) {
        return error_response(400, ex.what());
    } catch (const std::runtime_error& ex) {
        return error_response(401, ex.what());
    } catch (...) {
        return error_response(500, "internal server error");
    }
}
}
