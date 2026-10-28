// HTTP framing shared by the MCP listener and its standalone regression tests.
#ifndef LL_MCP_HTTP_PARSER_H
#define LL_MCP_HTTP_PARSER_H
#include <charconv>
#include <map>
#include <string>
#include <string_view>

namespace LLMCPHttp
{
constexpr std::size_t MAX_HEADERS = 16 * 1024;
constexpr std::size_t MAX_BODY = 1024 * 1024;
enum class ParseStatus { incomplete, ready, malformed, too_large };
struct Request
{
    std::string method, path;
    std::map<std::string, std::string> headers;
    std::size_t header_size = 0, content_length = 0;
    std::string header(const std::string& key) const
    {
        auto it = headers.find(key);
        return it == headers.end() ? std::string() : it->second;
    }
};
inline std::string lower(std::string text)
{
    for (char& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}
inline std::string_view trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == text.npos) return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}
inline bool decimal(std::string_view text, std::size_t& value)
{
    if (text.empty()) return false;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size();
}
inline ParseStatus parseHeaders(std::string_view data, Request& output)
{
    const auto end = data.find("\r\n\r\n");
    if (end == data.npos) return data.size() > MAX_HEADERS ? ParseStatus::too_large : ParseStatus::incomplete;
    if (end + 4 > MAX_HEADERS) return ParseStatus::too_large;
    Request request;
    request.header_size = end + 4;
    auto line_end = data.find("\r\n");
    auto line = data.substr(0, line_end);
    const auto first = line.find(' '), last = line.rfind(' ');
    if (first == line.npos || first == last || line.substr(last + 1) != "HTTP/1.1" && line.substr(last + 1) != "HTTP/1.0")
        return ParseStatus::malformed;
    request.method = std::string(line.substr(0, first));
    request.path = std::string(line.substr(first + 1, last - first - 1));
    if (request.path.empty() || request.path[0] != '/' || request.path.find(' ') != std::string::npos)
        return ParseStatus::malformed;
    std::size_t cursor = line_end + 2;
    while (cursor < end)
    {
        line_end = data.find("\r\n", cursor);
        line = data.substr(cursor, line_end - cursor);
        const auto colon = line.find(':');
        if (colon == line.npos || colon == 0) return ParseStatus::malformed;
        std::string name(line.substr(0, colon));
        for (char c : name)
        {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-')) return ParseStatus::malformed;
        }
        auto value = trim(line.substr(colon + 1));
        for (unsigned char c : value) if ((c < 32 && c != '\t') || c == 127) return ParseStatus::malformed;
        if (!request.headers.emplace(lower(name), std::string(value)).second) return ParseStatus::malformed;
        cursor = line_end + 2;
    }
    // Chunked framing is deliberately unsupported; never interpret it as a zero-length body.
    if (request.headers.count("transfer-encoding")) return ParseStatus::malformed;
    if (request.headers.count("content-length"))
    {
        if (!decimal(request.header("content-length"), request.content_length)) return ParseStatus::malformed;
        if (request.content_length > MAX_BODY) return ParseStatus::too_large;
    }
    output = std::move(request);
    return ParseStatus::ready;
}
inline bool loopbackAuthority(std::string_view authority)
{
    std::string_view host, port;
    if (!authority.empty() && authority.front() == '[')
    {
        auto end = authority.find(']');
        if (end == authority.npos) return false;
        host = authority.substr(1, end - 1);
        auto rest = authority.substr(end + 1);
        if (!rest.empty())
        {
            if (rest.front() != ':') return false;
            port = rest.substr(1);
            if (port.empty()) return false;
        }
    }
    else
    {
        auto colon = authority.find(':');
        host = authority.substr(0, colon);
        if (colon != authority.npos)
        {
            port = authority.substr(colon + 1);
            if (port.empty()) return false;
        }
    }
    auto name = lower(std::string(host));
    if (name != "localhost" && name != "127.0.0.1" && name != "::1") return false;
    std::size_t number = 0;
    return port.empty() || (decimal(port, number) && number > 0 && number <= 65535);
}
inline bool allowedOrigin(std::string_view origin, bool authenticated)
{
    // Sandboxed/file origins are only accepted with explicit authentication.
    if (origin == "null") return authenticated;
    const auto scheme = origin.find("://");
    if (scheme == origin.npos || (origin.substr(0, scheme) != "http" && origin.substr(0, scheme) != "https")) return false;
    return loopbackAuthority(origin.substr(scheme + 3));
}
}
#endif
