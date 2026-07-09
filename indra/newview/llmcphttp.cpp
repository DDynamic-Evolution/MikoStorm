#include "llmcphttp.h"
#include "llmcpserver.h"
#include "llsdjson.h"
#include <cstring>
#include <cerrno>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
typedef int ssize_t;
#define SHUT_RDWR SD_BOTH
#define close closesocket
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#endif

LLMCPHttpServer* LLMCPHttpServer::sInstance = nullptr;

void LLMCPHttpServer::start(U16 port, const std::string& auth_token)
{
    if (sInstance)
    {
        LL_WARNS("MCP") << "HTTP server already running" << LL_ENDL;
        return;
    }
    sInstance = new LLMCPHttpServer();
    sInstance->mPort = port;
    sInstance->mAuthToken = auth_token;
    sInstance->mRunning = true;
    sInstance->mThread = std::thread(&LLMCPHttpServer::serverThread, sInstance);
    LL_INFOS("MCP") << "HTTP server thread started on port " << port << LL_ENDL;
}

void LLMCPHttpServer::stop()
{
    if (!sInstance) return;
    sInstance->mRunning = false;
    if (sInstance->mListenFd >= 0)
    {
        ::shutdown(sInstance->mListenFd, SHUT_RDWR);
        ::closesocket(sInstance->mListenFd);
        sInstance->mListenFd = -1;
    }
    if (sInstance->mThread.joinable())
        sInstance->mThread.join();
    delete sInstance;
    sInstance = nullptr;
    LL_INFOS("MCP") << "HTTP server stopped" << LL_ENDL;
}

bool LLMCPHttpServer::isRunning()
{
    return sInstance && sInstance->mRunning;
}

void LLMCPHttpServer::serverThread(LLMCPHttpServer* self)
{
    self->run();
}

void LLMCPHttpServer::run()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        LL_WARNS("MCP") << "WSAStartup failed" << LL_ENDL;
        mRunning = false;
        return;
    }
#endif

    mListenFd = (int)::socket(AF_INET, SOCK_STREAM, 0);
    if (mListenFd < 0)
    {
        LL_WARNS("MCP") << "Failed to create socket" << LL_ENDL;
        mRunning = false;
        return;
    }

    int opt = 1;
#ifdef _WIN32
    ::setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    ::setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(mPort);

    if (::bind(mListenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        LL_WARNS("MCP") << "Failed to bind to port " << mPort << LL_ENDL;
        ::closesocket(mListenFd);
        mListenFd = -1;
        mRunning = false;
        return;
    }

    if (::listen(mListenFd, 5) < 0)
    {
        LL_WARNS("MCP") << "Failed to listen on port " << mPort << LL_ENDL;
        ::closesocket(mListenFd);
        mListenFd = -1;
        mRunning = false;
        return;
    }

    LL_INFOS("MCP") << "Listening on http://127.0.0.1:" << mPort << "/mcp" << LL_ENDL;

    while (mRunning)
    {
#ifdef _WIN32
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(mListenFd, &readfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 500000; // 500ms
        int ret = ::select(0, &readfds, nullptr, nullptr, &tv);
#else
        struct pollfd pfd;
        pfd.fd = mListenFd;
        pfd.events = POLLIN;
        int ret = ::poll(&pfd, 1, 500);
#endif

        if (ret < 0)
        {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            break;
        }

        if (ret == 0) continue;

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = (int)::accept(mListenFd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;

        handleClient(client_fd);
        ::closesocket(client_fd);
    }

    ::closesocket(mListenFd);
    mListenFd = -1;

#ifdef _WIN32
    WSACleanup();
#endif

    LL_INFOS("MCP") << "HTTP server thread exiting" << LL_ENDL;
}

void LLMCPHttpServer::handleClient(int client_fd)
{
    std::string request = readHttpRequest(client_fd);
    if (request.empty()) return;

    std::string method, path, body;
    size_t pos = request.find("\r\n");
    if (pos == std::string::npos) return;

    std::string request_line = request.substr(0, pos);
    size_t m1 = request_line.find(' ');
    if (m1 == std::string::npos) return;
    size_t m2 = request_line.find(' ', m1 + 1);
    if (m2 == std::string::npos) return;

    method = request_line.substr(0, m1);
    path = request_line.substr(m1 + 1, m2 - m1 - 1);

    // Parse headers
    bool auth_ok = mAuthToken.empty();
    size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) return;

    std::string headers = request.substr(0, header_end);
    std::string header_part = request.substr(pos + 2, header_end - pos - 2);

    // Check auth
    if (!mAuthToken.empty())
    {
        size_t auth_pos = header_part.find("Authorization: Bearer ");
        if (auth_pos != std::string::npos)
        {
            size_t auth_start = auth_pos + 21;
            size_t auth_end = header_part.find("\r\n", auth_start);
            std::string token = header_part.substr(auth_start, auth_end - auth_start);
            auth_ok = (token == mAuthToken);
        }
    }

    if (!auth_ok)
    {
        sendHttpResponse(client_fd, 401, "Unauthorized", R"({"jsonrpc":"2.0","id":null,"error":{"code":-32001,"message":"Unauthorized"}})", "application/json");
        return;
    }

    // Get body
    body = request.substr(header_end + 4);

    if (method == "POST" && path == "/mcp")
    {
        if (body.empty())
        {
            sendHttpResponse(client_fd, 400, "Bad Request", R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}})", "application/json");
            return;
        }

        // Parse JSON-RPC request using Boost.JSON via llsdjson
        try
        {
            LLSD request_llsd = LlsdFromJson(boost::json::parse(body));
            LLSD response_llsd = LLMCPServer::instance().handleRequest(request_llsd);
            std::string response_body;
            if (response_llsd.isDefined() && !response_llsd.isUndefined())
            {
                boost::json::value json = LlsdToJson(response_llsd);
                response_body = boost::json::serialize(json);
            }
            else
            {
                // Notification - no response, but send empty 202
                sendHttpResponse(client_fd, 202, "Accepted", "", "application/json");
                return;
            }
            sendHttpResponse(client_fd, 200, "OK", response_body, "application/json");
        }
        catch (const std::exception& e)
        {
            LL_WARNS("MCP") << "JSON parse error: " << e.what() << LL_ENDL;
            sendHttpResponse(client_fd, 400, "Bad Request",
                R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error","data":")" + std::string(e.what()) + "\"}}", "application/json");
        }
    }
    else if (method == "GET" && path == "/mcp")
    {
        // GET for health check / SSE
        sendHttpResponse(client_fd, 200, "OK",
            R"({"jsonrpc":"2.0","result":{"server":"MikoStorm","status":"running"}})",
            "application/json");
    }
    else if (method == "OPTIONS")
    {
        // CORS preflight
        std::string cors_headers =
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization, MCP-Session-Id\r\n"
            "Access-Control-Max-Age: 3600\r\n"
            "Content-Length: 0\r\n";
        std::string response = "HTTP/1.1 204 No Content\r\n" + cors_headers + "\r\n";
        ssize_t written = ::send(client_fd, response.c_str(), (int)response.size(), 0);
        (void)written;
    }
    else
    {
        sendHttpResponse(client_fd, 404, "Not Found", R"({"error":"Not found"})", "application/json");
    }
}

std::string LLMCPHttpServer::readHttpRequest(int fd)
{
    std::string result;
    char buf[4096];
    bool headers_done = false;
    size_t content_length = 0;
    bool has_content_length = false;

    while (true)
    {
#ifdef _WIN32
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        int ret = ::select(0, &readfds, nullptr, nullptr, &tv);
#else
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        int ret = ::poll(&pfd, 1, 5000);
#endif
        if (ret <= 0) break;

        ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        buf[n] = '\0';
        result.append(buf, n);

        if (!headers_done)
        {
            size_t header_end = result.find("\r\n\r\n");
            if (header_end != std::string::npos)
            {
                headers_done = true;
                // Parse Content-Length
                size_t cl_pos = result.find("Content-Length: ");
                if (cl_pos == std::string::npos)
                    cl_pos = result.find("content-length: ");
                if (cl_pos != std::string::npos)
                {
                    size_t val_start = result.find_first_of("0123456789", cl_pos + 16);
                    size_t val_end = result.find_first_not_of("0123456789", val_start);
                    if (val_start != std::string::npos)
                    {
                        content_length = std::stoul(result.substr(val_start, val_end - val_start));
                        has_content_length = true;
                    }
                }

                size_t body_received = result.size() - header_end - 4;
                if (has_content_length && body_received >= content_length)
                    break;
                if (!has_content_length)
                    break;
            }
        }
        else if (has_content_length)
        {
            size_t header_end = result.find("\r\n\r\n");
            size_t body_received = result.size() - header_end - 4;
            if (body_received >= content_length)
                break;
        }
    }

    return result;
}

void LLMCPHttpServer::sendHttpResponse(int fd, int status, const std::string& status_text,
                                       const std::string& body, const std::string& content_type)
{
    std::ostringstream response;
    response << "HTTP/1.1 " << status << " " << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Expose-Headers: MCP-Session-Id\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    std::string resp_str = response.str();
    ssize_t written = ::send(fd, resp_str.c_str(), (int)resp_str.size(), 0);
    (void)written;
}
