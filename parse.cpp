#include <string>
#include <map>
#include <iostream>
#include <ranges>
#include <expected>
#include <utility>
#include <cstdint>

using namespace std::string_literals;
using namespace std::ranges;

constexpr uint32_t METHOD_MAX_LEN = 32;
constexpr uint32_t URL_MAX_LEN = 1024;
constexpr uint32_t VERSION_MAX_LEN = 32;

enum class ParseError {
    method_too_long,
    url_too_long,
    version_too_long,
    invalid_crlf
};

enum class ParseState {
    method,
    url,
    version,
    headers,
    body
};

struct Request {
    std::string method;
    std::string url;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

const char* get_parse_error_str(ParseError e) noexcept {
    switch(e) {
        case ParseError::method_too_long:
            return "method too long";
        case ParseError::url_too_long:
            return "url too long";
        case ParseError::version_too_long:
            return "version too long";
        case ParseError::invalid_crlf:
            return "invalid crlf";
        default:
            return "error unknown";
    }
}

/**
 * if parse success, return a uint8_t value,
 * else return an error message.
*/
std::expected<uint8_t, std::string> hex_to_decimal(char c) noexcept {
    if (c >= '0' && c <= '9'){
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f'){
        return c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F'){
        return c - 'A' + 10;
    }
    else {
        return std::unexpected("param should be 0 ~ 9, a ~ f or A ~ F");
    }
}

/**
 * if parse success, return the decoded url string,
 * else return an error message.
*/
std::expected<std::string, std::string> percent_encoding_url_decode(std::string const& url) {
    std::string ret;
    auto urlLen = url.length();

    for (auto i = 0zu; i < urlLen;) {
        if (url[i] == '%') {
            if (i + 2 >= urlLen) {
                return std::unexpected("invalid percent encoding url");
            }

            auto p1 = hex_to_decimal(url[i + 1]);
            auto p2 = hex_to_decimal(url[i + 2]);

            if (p1 && p2) {
                ret += static_cast<char>(16 * p1.value() + p2.value());
                i += 3;
            }
            else {
                return std::unexpected("invalid percent encoding url");
            }
        }
        else {
            ret += url[i];
            ++i;
        }
    }

    return ret;
}

std::map<std::string, std::string> parse_http_headers(std::string const& headers) {
    std::map<std::string, std::string> m;

    for (const auto line : views::split(headers, "\r\n"s)) {
        auto str = std::ranges::to<std::string>(line);
        
        auto pos = str.find(": ");
        auto key = str.substr(0, pos);
        auto value = str.substr(pos + 2);

        m.emplace(std::move(key), std::move(value));
    }

    std::cout << m.size() << "\n";
    return m;
}

std::expected<Request, ParseError> parse_http_request(std::string const& data) {
    Request req;
    std::string headers;
    ParseState state = ParseState::method;
    const char* ptr = data.c_str();

    while(*ptr != '\0') {
        switch(state) {
            case ParseState::method:
                if (*ptr == ' ') {
                    state = ParseState::url;
                }
                else {
                    if (req.method.length() == METHOD_MAX_LEN) {
                        return std::unexpected(ParseError::method_too_long);
                    }

                    req.method += *ptr;
                }

                ++ptr;
                break;
            case ParseState::url:
                if (*ptr == ' ') {
                    state = ParseState::version;
                }
                else {
                    if (req.url.length() == URL_MAX_LEN) {
                        return std::unexpected(ParseError::url_too_long);
                    }

                    req.url += *ptr;
                }

                ++ptr;
                break;
            case ParseState::version:
                if (*ptr == '\r') {
                    if (ptr[1] != '\n') {
                        return std::unexpected(ParseError::invalid_crlf);
                    }
                    else {
                        state = ParseState::headers;
                        ptr += 2;
                    }
                }
                else {
                    if (req.version.length() == VERSION_MAX_LEN) {
                        return std::unexpected(ParseError::version_too_long);
                    }

                    req.version += *ptr;
                    ++ptr;
                }

                break;
            case ParseState::headers:
                if (*ptr == '\r') {
                    if (ptr[1] != '\n') {
                        return std::unexpected(ParseError::invalid_crlf);
                    }

                    if (ptr[2] == '\r' && ptr[3] == '\n') {
                        state = ParseState::body;
                        ptr += 4;
                    }
                    else {
                        headers += "\r\n";
                        ptr += 2;
                    }
                }
                else {
                    headers += *ptr;
                    ++ptr;
                }

                break;
            case ParseState::body:
                req.body += *ptr;
                ++ptr;
                break;
            default:
                std::unreachable();
        }
    }

    req.headers = std::move(parse_http_headers(headers));
    return req;
}

int main() {
    std::string url = "GET http://www.hatsunemiku.com/ HTTP/1.1\r\n"
                            "Host: www.example.com\r\n"
                            "Content-Length: 10\r\n"
                            "Accept-Encoding: utf-8\r\n"
                            "\r\n"
                            "Hello World";

    auto decodeUrl = percent_encoding_url_decode(url);
    if (decodeUrl) {
        auto req = parse_http_request(decodeUrl.value());
        if (req) {
            const auto& v = req.value();
            std::cout << v.method << "\n";
            std::cout << v.url << "\n";
            std::cout << v.version << "\n";

            for (const auto& [k, v] : v.headers) {
                std::cout << k << " => " << v << "\n";
            }

            std::cout << v.body << "\n";
        }
        else {
            std::cerr << get_parse_error_str(req.error()) << "\n";
        }
    }
    else {
        std::cerr << decodeUrl.error() << "\n";
    }
}
