#ifndef LL_LLMCPHTTP_H
#define LL_LLMCPHTTP_H
#include "llsd.h"
#include "llmcphttpparser.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <future>

class LLMCPHttpServer
{
public:
    static bool start(U16 port);
    static void stop();
    static bool isRunning();
private:
#ifdef _WIN32
    using Socket = std::uintptr_t;
#else
    using Socket = int;
#endif
    static constexpr Socket INVALID = static_cast<Socket>(-1);
    void run();
    void handleClient(Socket fd);
    std::string readHttpRequest(Socket fd, LLMCPHttp::Request& request, int& error);
    bool waitSocket(Socket fd, bool write);
    bool sendAll(Socket fd, const std::string& response);
    void sendHttpResponse(Socket fd, int status, const std::string& status_text,
                          const std::string& body, const std::string& content_type = "application/json",
                          const std::string& origin = "", const std::string& extra_headers = "");
    static LLMCPHttpServer* sInstance;
    Socket mListenFd{INVALID};
    U16 mPort{0};
    std::atomic<bool> mRunning{false};
    std::thread mThread;
    std::promise<bool> mStartup;
};
#endif
