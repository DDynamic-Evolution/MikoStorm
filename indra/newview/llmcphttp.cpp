#include "llmcphttp.h"
#include "llmcpserver.h"
#include "llsdjson.h"
#include <chrono>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <vector>
#include <memory>
#include <limits>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#define closesocket close
#endif

namespace
{
bool interrupted()
{
#ifdef _WIN32
    const int error = WSAGetLastError();
    return error == WSAEINTR || error == WSAEWOULDBLOCK;
#else
    return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}
}
LLMCPHttpServer* LLMCPHttpServer::sInstance = nullptr;
bool LLMCPHttpServer::start(U16 port)
{
    if (sInstance) return isRunning();
    sInstance = new LLMCPHttpServer();
    sInstance->mPort = port;
    sInstance->mRunning = true;
    auto ready = sInstance->mStartup.get_future();
    auto* self = sInstance;
    try { self->mThread = std::thread([self] { self->run(); }); }
    catch (...) { delete sInstance; sInstance = nullptr; return false; }
    if (ready.wait_for(std::chrono::seconds(2)) != std::future_status::ready || !ready.get())
    {
        stop();
        return false;
    }
    return true;
}
void LLMCPHttpServer::stop()
{
    if (!sInstance) return;
    sInstance->mRunning = false;
    if (sInstance->mThread.joinable()) sInstance->mThread.join();
    delete sInstance;
    sInstance = nullptr;
}
bool LLMCPHttpServer::isRunning() { return sInstance && sInstance->mRunning; }
bool LLMCPHttpServer::waitSocket(Socket fd, bool write)
{
#ifdef _WIN32
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timeval timeout{0, 100000};
    return ::select(0, write ? nullptr : &fds, write ? &fds : nullptr, nullptr, &timeout) > 0;
#else
    pollfd pfd{fd, static_cast<short>(write ? POLLOUT : POLLIN), 0};
    return ::poll(&pfd, 1, 100) > 0;
#endif
}
void LLMCPHttpServer::run()
{
    mListenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (mListenFd == INVALID) { mRunning = false; mStartup.set_value(false); return; }
    int opt = 1;
#ifdef _WIN32
    // Do not let another Windows listener take over this loopback endpoint.
    ::setsockopt(mListenFd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    ::setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(mPort);
    if (::bind(mListenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(mListenFd, 5) != 0)
    {
        LL_WARNS("MCP") << "Could not listen on port " << mPort << LL_ENDL;
        ::closesocket(mListenFd);
        mListenFd = INVALID;
        mRunning = false;
        mStartup.set_value(false);
        return;
    }
    struct Worker { std::thread thread; std::shared_ptr<std::atomic<bool>> done; };
    std::vector<Worker> workers;
    try { workers.reserve(8); }
    catch (...) { ::closesocket(mListenFd); mListenFd = INVALID; mRunning = false; mStartup.set_value(false); return; }
    mStartup.set_value(true);
    LL_INFOS("MCP") << "Listening on http://127.0.0.1:" << mPort << "/mcp" << LL_ENDL;
    while (mRunning)
    {
        for (auto it = workers.begin(); it != workers.end(); )
        {
            if (it->done->load()) { it->thread.join(); it = workers.erase(it); }
            else ++it;
        }
        if (!waitSocket(mListenFd, false)) continue;
        Socket client = ::accept(mListenFd, nullptr, nullptr);
        if (client == INVALID) continue;
#ifdef _WIN32
        u_long nonblocking = 1;
        bool configured = ::ioctlsocket(client, FIONBIO, &nonblocking) == 0;
#else
        int flags = ::fcntl(client, F_GETFL, 0);
        bool configured = flags >= 0 && ::fcntl(client, F_SETFL, flags | O_NONBLOCK) == 0;
#ifdef SO_NOSIGPIPE
        ::setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif
#endif
        if (!configured || workers.size() >= 8)
        {
            if (configured) sendHttpResponse(client, 503, "Service Unavailable", R"({"error":"Too many concurrent requests"})");
            ::closesocket(client);
            continue;
        }
        // Reserve before constructing a thread: moving a joinable thread must never throw.
        try
        {
            auto done = std::make_shared<std::atomic<bool>>(false);
            workers.push_back({std::thread([this, client, done]() {
                try { handleClient(client); }
                catch (...) { LL_WARNS("MCP") << "HTTP request failed" << LL_ENDL; }
                ::closesocket(client);
                done->store(true);
            }), done});
        }
        catch (...) { ::closesocket(client); }
    }
    for (auto& worker : workers) if (worker.thread.joinable()) worker.thread.join();
    ::closesocket(mListenFd);
    mListenFd = INVALID;
}
std::string LLMCPHttpServer::readHttpRequest(Socket fd, LLMCPHttp::Request& request, int& error)
{
    std::string data;
    char buffer[4096];
    bool headers_ready = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    error = 0;
    while (mRunning && std::chrono::steady_clock::now() < deadline)
    {
        if (!waitSocket(fd, false)) continue;
        const auto count = ::recv(fd, buffer, sizeof(buffer), 0);
        if (count < 0) { if (interrupted()) continue; error = 400; return {}; }
        if (count == 0) { error = 400; return {}; }
        data.append(buffer, static_cast<std::size_t>(count));
        if (!headers_ready)
        {
            auto status = LLMCPHttp::parseHeaders(data, request);
            if (status == LLMCPHttp::ParseStatus::too_large) { error = 413; return {}; }
            if (status == LLMCPHttp::ParseStatus::malformed) { error = 400; return {}; }
            headers_ready = status == LLMCPHttp::ParseStatus::ready;
        }
        if (headers_ready && data.size() >= request.header_size + request.content_length)
        {
            // Ignore any pipelined bytes; one request is processed per connection.
            data.resize(request.header_size + request.content_length);
            return data;
        }
    }
    error = 408;
    return {};
}
namespace
{
// LLSD integers are 32-bit. Never truncate large JSON numbers before validation;
// JSON-RPC request IDs are preserved separately as raw boost::json values.
LLSD checkedLLSD(const boost::json::value& value)
{
    if (value.is_object())
    {
        LLSD result = LLSD::emptyMap();
        for (const auto& item : value.as_object()) result[std::string(item.key().data(), item.key().size())] = checkedLLSD(item.value());
        return result;
    }
    if (value.is_array())
    {
        LLSD result = LLSD::emptyArray();
        for (const auto& item : value.as_array()) result.append(checkedLLSD(item));
        return result;
    }
    if (value.is_int64() && (value.as_int64() < INT32_MIN || value.as_int64() > INT32_MAX))
        return LLSD(static_cast<F64>(value.as_int64()));
    if (value.is_uint64() && value.as_uint64() > INT32_MAX)
        return LLSD(static_cast<F64>(value.as_uint64()));
    return LlsdFromJson(value);
}
}
void LLMCPHttpServer::handleClient(Socket fd)
{
    LLMCPHttp::Request request;
    int error = 0;
    std::string data = readHttpRequest(fd, request, error);
    if (!mRunning) return;
    if (error)
    {
        sendHttpResponse(fd, error, error == 413 ? "Payload Too Large" : error == 408 ? "Request Timeout" : "Bad Request", R"({"error":"Invalid or incomplete HTTP request"})");
        return;
    }
    auto& server = LLMCPServer::instance();
    const std::string origin = request.header("origin"), authorization = request.header("authorization");
    LLMCPServer::Context context;
    const bool supplied = !authorization.empty();
    const bool bearer = authorization.compare(0, 7, "Bearer ") == 0;
    const bool authenticated = (!supplied || bearer) && server.authenticate(bearer ? authorization.substr(7) : "", supplied, context.client);
    const bool configuredAuth = server.getStatus()["authentication"].asBoolean();
    if ((!origin.empty() && !LLMCPHttp::allowedOrigin(origin, (supplied && authenticated) || (request.method == "OPTIONS" && configuredAuth))) ||
        !LLMCPHttp::loopbackAuthority(request.header("host")))
    { sendHttpResponse(fd, 403, "Forbidden", R"({"error":"Non-local origin or host"})"); return; }
    if (request.path != "/mcp" && request.path != "/health")
    { sendHttpResponse(fd, 404, "Not Found", R"({"error":"Not found"})"); return; }
    if (request.method == "OPTIONS")
    {
        sendHttpResponse(fd, 204, "No Content", "", "application/json", origin,
            "Access-Control-Allow-Methods: POST, DELETE, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version\r\n");
        return;
    }
    if (!authenticated) { sendHttpResponse(fd, 401, "Unauthorized", R"({"error":"Unauthorized"})", "application/json", origin); return; }
    if (request.path == "/health")
    {
        if (request.method == "GET") sendHttpResponse(fd, 200, "OK", R"({"server":"MikoStorm","status":"running"})", "application/json", origin);
        else sendHttpResponse(fd, 405, "Method Not Allowed", "", "application/json", origin, "Allow: GET\r\n");
        return;
    }
    if (request.method != "POST" && request.method != "DELETE")
    { sendHttpResponse(fd, 405, "Method Not Allowed", "", "application/json", origin, "Allow: POST, DELETE, OPTIONS\r\n"); return; }
    context.session = request.header("mcp-session-id");
    const auto protocol = request.header("mcp-protocol-version");
    if ((!protocol.empty() && !LLMCPServer::supportsProtocol(protocol)) || protocol.size() > 32)
    { sendHttpResponse(fd, 400, "Bad Request", R"({"error":"Unsupported MCP protocol version"})", "application/json", origin); return; }
    if (!context.session.empty() && !server.validSession(context.session, context.client))
    { sendHttpResponse(fd, 404, "Not Found", R"({"error":"Unknown or expired MCP session"})", "application/json", origin); return; }
    if (!context.session.empty() && !protocol.empty() && protocol != server.protocolForSession(context.session))
    { sendHttpResponse(fd, 400, "Bad Request", R"({"error":"Protocol version differs from initialization"})", "application/json", origin); return; }
    if (request.method == "DELETE")
    {
        if (context.session.empty()) { sendHttpResponse(fd, 400, "Bad Request", R"({"error":"MCP-Session-Id required"})", "application/json", origin); return; }
        server.closeSession(context.session, context.client);
        sendHttpResponse(fd, 204, "No Content", "", "application/json", origin);
        return;
    }
    const auto accept = LLMCPHttp::lower(request.header("accept"));
    if (!accept.empty() && accept.find("application/json") == std::string::npos && accept.find("*/*") == std::string::npos)
    { sendHttpResponse(fd, 406, "Not Acceptable", R"({"error":"This endpoint returns application/json"})", "application/json", origin); return; }
    auto contentType = LLMCPHttp::lower(request.header("content-type"));
    if (contentType.substr(0, contentType.find(';')) != "application/json")
    { sendHttpResponse(fd, 415, "Unsupported Media Type", R"({"error":"Expected application/json"})", "application/json", origin); return; }
    boost::system::error_code parse_error;
    auto json = boost::json::parse(data.substr(request.header_size), parse_error);
    if (parse_error)
    {
        sendHttpResponse(fd, 400, "Bad Request", R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}})", "application/json", origin);
        return;
    }
    if (!json.is_object())
    { sendHttpResponse(fd, 400, "Bad Request", R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Expected a JSON-RPC object"}})", "application/json", origin); return; }
    const auto& object = json.as_object();
    const auto* id = object.if_contains("id");
    if (id && (!(id->is_string() || id->is_int64() || id->is_uint64()) || (id->is_string() && id->as_string().size() > 128)))
    { sendHttpResponse(fd, 400, "Bad Request", R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid request id"}})", "application/json", origin); return; }
    const LLSD rpc = checkedLLSD(json);
    if (context.session.empty() && rpc["method"].asString() != "initialize")
    { sendHttpResponse(fd, 400, "Bad Request", R"({"error":"Initialize first; MCP-Session-Id required"})", "application/json", origin); return; }
    context.active = std::make_shared<std::atomic<bool>>(true);
    if (id) context.requestKey = boost::json::serialize(*id);
    if (auto params = object.if_contains("params"); params && params->is_object())
        if (auto target = params->as_object().if_contains("requestId")) context.cancelKey = boost::json::serialize(*target);
    std::string created;
    const LLSD response = server.handleRequest(rpc, context, &created);
    if (response.isUndefined()) sendHttpResponse(fd, 202, "Accepted", "", "application/json", origin);
    else
    {
        auto result = LlsdToJson(response);
        if (result.is_object()) result.as_object()["id"] = id ? *id : boost::json::value(nullptr);
        sendHttpResponse(fd, 200, "OK", boost::json::serialize(result), "application/json", origin,
            created.empty() ? "" : "MCP-Session-Id: " + created + "\r\n");
    }
}
bool LLMCPHttpServer::sendAll(Socket fd, const std::string& response)
{
    std::size_t offset = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (mRunning && offset < response.size() && std::chrono::steady_clock::now() < deadline)
    {
        if (!waitSocket(fd, true)) continue;
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto count = ::send(fd, response.data() + offset, static_cast<int>(response.size() - offset), flags);
        if (count < 0) { if (interrupted()) continue; return false; }
        if (count == 0) return false;
        offset += static_cast<std::size_t>(count);
    }
    return offset == response.size();
}
void LLMCPHttpServer::sendHttpResponse(Socket fd, int status, const std::string& status_text,
    const std::string& body, const std::string& content_type, const std::string& origin, const std::string& extra_headers)
{
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << status_text << "\r\nContent-Type: " << content_type
             << "\r\nContent-Length: " << body.size() << "\r\n";
    if (!origin.empty()) response << "Access-Control-Allow-Origin: " << origin << "\r\nAccess-Control-Expose-Headers: MCP-Session-Id\r\n";
    response << extra_headers << "Cache-Control: no-store\r\nConnection: close\r\n\r\n" << body;
    sendAll(fd, response.str());
}
