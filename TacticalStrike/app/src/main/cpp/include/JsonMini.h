#pragma once

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ts {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Array = std::vector<JsonValue>;
    using Object = std::unordered_map<std::string, JsonValue>;

    Type type = Type::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    Array arrVal;
    Object objVal;

    JsonValue() = default;
    explicit JsonValue(bool b) : type(Type::Bool), boolVal(b) {}
    explicit JsonValue(double n) : type(Type::Number), numVal(n) {}
    explicit JsonValue(int n) : type(Type::Number), numVal(static_cast<double>(n)) {}
    explicit JsonValue(std::string s) : type(Type::String), strVal(std::move(s)) {}
    explicit JsonValue(Array a) : type(Type::Array), arrVal(std::move(a)) {}
    explicit JsonValue(Object o) : type(Type::Object), objVal(std::move(o)) {}

    bool isNull() const { return type == Type::Null; }
    bool isBool() const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    float asFloat(float def = 0.0f) const {
        return isNumber() ? static_cast<float>(numVal) : def;
    }
    int asInt(int def = 0) const {
        return isNumber() ? static_cast<int>(numVal) : def;
    }
    bool asBool(bool def = false) const {
        return isBool() ? boolVal : def;
    }
    const std::string& asString() const { return strVal; }

    bool has(const std::string& key) const {
        return isObject() && objVal.find(key) != objVal.end();
    }

    const JsonValue& get(const std::string& key) const {
        static const JsonValue kNull;
        if (!isObject()) return kNull;
        auto it = objVal.find(key);
        return it == objVal.end() ? kNull : it->second;
    }

    static bool parse(const std::string& text, JsonValue& out) {
        size_t pos = 0;
        if (!parseValue(text, pos, out)) return false;
        return true;
    }

    std::string dump() const {
        std::ostringstream os;
        dumpInternal(os);
        return os.str();
    }

private:
    static void skipWs(const std::string& s, size_t& p) {
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
    }

    static bool parseValue(const std::string& s, size_t& p, JsonValue& out) {
        skipWs(s, p);
        if (p >= s.size()) return false;
        char c = s[p];
        if (c == '{') return parseObject(s, p, out);
        if (c == '[') return parseArray(s, p, out);
        if (c == '"') {
            std::string str;
            if (!parseString(s, p, str)) return false;
            out = JsonValue(std::move(str));
            return true;
        }
        if (c == 't' && s.compare(p, 4, "true") == 0) {
            p += 4;
            out = JsonValue(true);
            return true;
        }
        if (c == 'f' && s.compare(p, 5, "false") == 0) {
            p += 5;
            out = JsonValue(false);
            return true;
        }
        if (c == 'n' && s.compare(p, 4, "null") == 0) {
            p += 4;
            out = JsonValue();
            return true;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parseNumber(s, p, out);
        }
        return false;
    }

    static bool parseString(const std::string& s, size_t& p, std::string& out) {
        if (p >= s.size() || s[p] != '"') return false;
        ++p;
        out.clear();
        while (p < s.size()) {
            char c = s[p++];
            if (c == '"') return true;
            if (c == '\\' && p < s.size()) {
                char esc = s[p++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default: out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return false;
    }

    static bool parseNumber(const std::string& s, size_t& p, JsonValue& out) {
        const char* start = s.c_str() + p;
        char* end = nullptr;
        double val = std::strtod(start, &end);
        if (end == start) return false;
        p = static_cast<size_t>(end - s.c_str());
        out = JsonValue(val);
        return true;
    }

    static bool parseArray(const std::string& s, size_t& p, JsonValue& out) {
        ++p; // '['
        Array arr;
        skipWs(s, p);
        if (p < s.size() && s[p] == ']') {
            ++p;
            out = JsonValue(std::move(arr));
            return true;
        }
        while (p < s.size()) {
            JsonValue elem;
            if (!parseValue(s, p, elem)) return false;
            arr.push_back(std::move(elem));
            skipWs(s, p);
            if (p < s.size() && s[p] == ',') {
                ++p;
                continue;
            }
            if (p < s.size() && s[p] == ']') {
                ++p;
                out = JsonValue(std::move(arr));
                return true;
            }
            return false;
        }
        return false;
    }

    static bool parseObject(const std::string& s, size_t& p, JsonValue& out) {
        ++p; // '{'
        Object obj;
        skipWs(s, p);
        if (p < s.size() && s[p] == '}') {
            ++p;
            out = JsonValue(std::move(obj));
            return true;
        }
        while (p < s.size()) {
            skipWs(s, p);
            std::string key;
            if (!parseString(s, p, key)) return false;
            skipWs(s, p);
            if (p >= s.size() || s[p] != ':') return false;
            ++p;
            JsonValue val;
            if (!parseValue(s, p, val)) return false;
            obj.emplace(std::move(key), std::move(val));
            skipWs(s, p);
            if (p < s.size() && s[p] == ',') {
                ++p;
                continue;
            }
            if (p < s.size() && s[p] == '}') {
                ++p;
                out = JsonValue(std::move(obj));
                return true;
            }
            return false;
        }
        return false;
    }

    void dumpInternal(std::ostringstream& os) const {
        switch (type) {
            case Type::Null: os << "null"; break;
            case Type::Bool: os << (boolVal ? "true" : "false"); break;
            case Type::Number: os << numVal; break;
            case Type::String: {
                os << '"';
                for (char c : strVal) {
                    if (c == '"' || c == '\\') os << '\\';
                    os << c;
                }
                os << '"';
                break;
            }
            case Type::Array: {
                os << '[';
                for (size_t i = 0; i < arrVal.size(); ++i) {
                    if (i > 0) os << ',';
                    arrVal[i].dumpInternal(os);
                }
                os << ']';
                break;
            }
            case Type::Object: {
                os << '{';
                bool first = true;
                for (const auto& kv : objVal) {
                    if (!first) os << ',';
                    first = false;
                    os << '"' << kv.first << "\":";
                    kv.second.dumpInternal(os);
                }
                os << '}';
                break;
            }
        }
    }
};

} // namespace ts
