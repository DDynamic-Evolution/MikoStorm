#include "llmcphttpparser.h"
#include "llmcptask.h"
#include "fsnativechatmodel.h"
#include "fsexportpolicy.h"
#include "llworkbudget.h"
#include "llstream3dnumeric.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
int checks = 0;
void check(bool value, const char* message)
{
    ++checks;
    if (!value) throw std::runtime_error(message);
}
void http()
{
    using namespace LLMCPHttp;
    Request request;
    auto parse = [&](const std::string& text) { return parseHeaders(text, request); };
    check(parse("POST /mcp HTTP/1.1\r\ncOnTeNt-LeNgTh:\t2\r\nAuthorization: Bearer secret\r\n\r\n{}") == ParseStatus::ready, "mixed-case headers");
    check(request.content_length == 2 && request.header("authorization") == "Bearer secret", "exact body and token");
    check(parse("POST /mcp HTTP/1.1\r\nContent-Length: 0\r\n\r\nextra") == ParseStatus::ready && request.content_length == 0, "explicit zero body");
    check(parse("POST /mcp HTTP/1.1\r\n") == ParseStatus::incomplete, "fragmented header");
    for (const std::string& value : {"-1", "+1", "1junk", "9999999999999999999999999999", "", "1 2"})
        check(parse("POST /mcp HTTP/1.1\r\nContent-Length: " + value + "\r\n\r\n") == ParseStatus::malformed, "invalid length must not throw");
    check(parse("POST /mcp HTTP/1.1\r\nContent-Length: 1048577\r\n\r\n") == ParseStatus::too_large, "oversized body");
    check(parse("POST /mcp HTTP/1.1\r\nContent-Length: 2\r\ncontent-length: 3\r\n\r\n") == ParseStatus::malformed, "conflicting framing");
    check(parse("POST /mcp HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n") == ParseStatus::malformed, "unsupported transfer encoding");
    check(parse("GET /mcp HTTP/1.1\r\n" + std::string(MAX_HEADERS, 'x')) == ParseStatus::too_large, "header memory cap");
    for (auto origin : {"http://localhost:13231", "https://127.0.0.1", "http://[::1]:8080"}) check(allowedOrigin(origin, false), "loopback origin");
    for (auto origin : {"https://example.com", "http://localhost.evil", "http://localhost/path", "http://localhost:99999", "http://[::1]:", "null"}) check(!allowedOrigin(origin, false), "reject nonlocal origin");
    check(allowedOrigin("null", true), "authenticated file origin");
}
void tasks()
{
    using namespace std::chrono_literals;
    auto session = std::make_shared<std::atomic<bool>>(true);
    auto task = std::make_shared<LLMCPCancellableTask<int>>(session);
    std::thread worker([&] { task->execute([] { return 42; }); });
    check(task->wait(1s) && task->get() == 42, "queued result");
    worker.join();
    auto cancelled = std::make_shared<LLMCPCancellableTask<int>>(session);
    bool invoked = false, cancelled_result = false;
    std::thread waiter([&] { cancelled_result = !cancelled->wait(5s); });
    const auto start = std::chrono::steady_clock::now();
    session->store(false);
    waiter.join();
    check(cancelled_result, "shutdown must cancel queued request");
    check(std::chrono::steady_clock::now() - start < 1s, "shutdown must not wait for mainloop");
    cancelled->execute([&] { invoked = true; return 0; });
    check(!invoked, "stale action must not run after shutdown");
    session = std::make_shared<std::atomic<bool>>(true);
    auto timed = std::make_shared<LLMCPCancellableTask<int>>(session);
    check(!timed->wait(1ms), "request deadline");
    timed->execute([&] { invoked = true; return 0; });
    check(!invoked, "timed-out queued action must not run later");
    auto requestFlag = std::make_shared<std::atomic<bool>>(true);
    auto cancelledRequest = std::make_shared<LLMCPCancellableTask<int>>(session, requestFlag);
    requestFlag->store(false);
    check(!cancelledRequest->wait(1s), "request cancellation releases waiter without stopping server");
    cancelledRequest->execute([&] { invoked = true; return 0; });
    check(!invoked && session->load(), "cancelled request cannot execute and server lifetime remains active");
    auto independentRequest = std::make_shared<LLMCPCancellableTask<int>>(session);
    independentRequest->execute([] { return 7; });
    check(independentRequest->wait(1s) && independentRequest->get() == 7, "cancellation leaves unrelated requests usable");
    auto throwing = std::make_shared<LLMCPCancellableTask<int>>(session);
    throwing->execute([]() -> int { throw std::runtime_error("handler"); });
    check(throwing->wait(1s), "handler exception is delivered");
    try { throwing->get(); check(false, "exception lost"); } catch (const std::runtime_error&) { check(true,"exception delivered"); }
}
void permissions()
{
    using namespace FSAssetExportPolicy;
    for (bool creator : {false,true}) for (bool export_flag : {false,true}) for (bool full : {false,true})
    {
        check(allowed(Grid::second_life, Policy::allowed, creator, export_flag, full) == creator, "SL creator rights");
        check(allowed(Grid::opensim, Policy::allowed, creator, export_flag, full) == export_flag, "OpenSim export bit");
        check(allowed(Grid::opensim, Policy::undefined, creator, export_flag, full) == full, "OpenSim legacy full permissions");
        check(allowed(Grid::opensim, Policy::denied, creator, export_flag, full) == creator, "OpenSim denied fallback");
        check(!allowed(Grid::unknown, Policy::allowed, creator, export_flag, full), "unknown grid fail closed");
    }
    bool right = true;
    auto live_check = [&] { return allowed(Grid::opensim, Policy::allowed, false, right, false); };
    check(live_check(), "initial export permitted");
    right = false;
    check(!live_check(), "revoked permission rejected at subsequent write check");
}
void audio()
{
    using namespace std::chrono_literals;
    float value = 0.f;
    check(LLStream3DNumeric::parseFloat("+1.25", value) && value == 1.25f, "valid audio gain");
    for (auto text : {"nan", "inf", "-inf", "1.0junk", "1e999", "", "+", "--1"}) check(!LLStream3DNumeric::parseFloat(text, value), "reject invalid audio parameters");
    LLWorkBudget exhausted(512, 0ms);
    check(!exhausted.take(), "elapsed time cap");
    std::vector<bool> visited(50000, false);
    std::size_t cursor = 0, total = 0, ticks = 0;
    while (total < visited.size())
    {
        LLWorkBudget budget(512, 1s);
        std::size_t examined = 0;
        while (total < visited.size() && budget.take())
        {
            visited[cursor] = true;
            cursor = (cursor + 1) % visited.size();
            ++total; ++examined;
        }
        check(examined <= 512, "bounded dense-region scan");
        ++ticks;
    }
    check(ticks == 98, "round-robin forward progress");
    for (bool seen : visited) if (!seen) throw std::runtime_error("starved object");
}
void nativeChat()
{
    using FSNativeChat::Transcript;
    Transcript history;
    for (int i = 0; i < 300; ++i) history.append(std::to_string(i));
    check(history.lines().size() == 200 && history.lines().front() == "100" && history.lines().back() == "299", "native chat retains newest 200 entries");
    history.clear();
    check(history.lines().empty() && history.bytes() == 0, "session reset drops transcript and byte count");
    for (int i = 0; i < 100; ++i) history.append(std::string(8000, 'x'));
    check(history.bytes() <= Transcript::MaxBytes && history.lines().size() < 200, "native transcript byte budget");
    history.clear();
    std::string unicode;
    for (int i = 0; i < 4000; ++i) unicode += "\xf0\x9f\x98\x80";
    history.append(unicode);
    const auto clipped = history.lines().back();
    check(clipped.size() <= Transcript::MaxEntryBytes && clipped.ends_with("..."), "oversized message is visibly clipped");
    check((clipped.size() - 3) % 4 == 0, "UTF-8 emoji is not split when clipping");
    history.append("last");
    check(history.bytes() == clipped.size() + 4, "bounded transcript accounts exact bytes");
}
void nativeConversations()
{
    using Book = FSNativeChat::Conversations;
    Book book;
    const auto localToken = book.local().token;
    auto* im = book.upsert("person", "Alice", Book::Kind::IM);
    const auto imToken = im->token;
    im->draft = "private draft"; im->history.append("private line");
    auto* group = book.upsert("group", "Friends", Book::Kind::Group);
    const auto groupToken = group->token;
    group->draft = "group draft"; group->history.append("group line"); group->unread = 3;
    check(book.select("person", imToken) && book.selected().draft == "private draft", "IM draft is isolated");
    check(book.select("group", groupToken) && book.selected().history.lines().front() == "group line", "group history is isolated");
    check(book.selected().unread == 0 && book.local().history.lines().empty(), "selection clears only native unread state");
    check(!book.select("person", groupToken) && book.selected().key == "group", "stale choice cannot select another conversation");
    book.rekey("group", "resolved-group");
    check(book.selected().key == "resolved-group" && book.selected().draft == "group draft", "rekey retains selection and draft");
    check(!book.matches("group", groupToken) && !book.matches("resolved-group", groupToken), "rekey invalidates queued targets");
    book.erase("person");
    im = book.upsert("person", "Alice", Book::Kind::IM);
    check(im->token != imToken && im->draft.empty() && im->history.lines().empty(), "reopened session cannot recover old draft or history");
    book.erase("resolved-group");
    check(book.selected().kind == Book::Kind::Local, "closed selected session falls back to local");
    book.clear();
    check(book.items().size() == 1 && book.local().token != localToken && book.local().draft.empty(), "account reset clears all conversations and invalidates local target");
    for (std::size_t i = 1; i < Book::MaxConversations; ++i) book.upsert(std::to_string(i), "name", Book::Kind::IM);
    check(!book.upsert("overflow", "name", Book::Kind::IM) && book.items().size() == Book::MaxConversations, "conversation memory is bounded without evicting drafts");
    book.erase("");
    check(book.local().kind == Book::Kind::Local, "local conversation cannot be removed");
}

void nativeThemeAndFriends()
{
    using namespace FSNativeChat;
    check(chooseTheme(true, true, false) == ThemeMode::HighContrast, "high contrast overrides dark preference");
    check(chooseTheme(true, false, true) == ThemeMode::HighContrast, "high contrast works without app mode");
    check(chooseTheme(false, true, false) == ThemeMode::Dark, "Windows dark application mode");
    check(chooseTheme(false, true, true) == ThemeMode::Light, "Windows light application mode");
    check(chooseTheme(false, false, false) == ThemeMode::System, "unavailable app mode falls back to system colors");
    Friends friends;
    friends.update({{"alice", "Alice", true, 0}, {"bob", "Bob", false, 0}});
    const auto alice = friends.entries()[0].token, bob = friends.entries()[1].token;
    check(alice != bob && friends.matches("alice", alice) && !friends.matches("bob", alice), "friend tokens bind the requested contact");
    friends.update({{"bob", "Bob New Name", true, 0}, {"alice", "Alice", false, 0}});
    check(friends.matches("alice", alice) && friends.matches("bob", bob), "name online and order changes retain membership identity");
    friends.update({{"bob", "Bob", true, 0}});
    check(!friends.matches("alice", alice), "removed friends cannot be opened from old snapshots");
    friends.update({{"alice", "Alice", true, 0}});
    check(!friends.matches("alice", alice) && friends.entries()[0].token != alice, "readded friend invalidates old queued action");
    const auto beforeReset = friends.entries()[0].token;
    friends.clear(); friends.update({{"alice", "Alice", true, 0}});
    check(!friends.matches("alice", beforeReset), "account reset invalidates friend lifetime");
    std::vector<FriendEntry> many;
    for (std::size_t i = 0; i <= Friends::MaxEntries; ++i) many.push_back({std::to_string(i), "friend", false, 0});
    friends.update(std::move(many));
    check(friends.entries().size() == Friends::MaxEntries && !friends.matches(std::to_string(Friends::MaxEntries), beforeReset), "friend snapshot is bounded");
    friends.clear(); check(friends.entries().empty(), "logout clears copied friend data");
}

int main()
{
    try { http(); tasks(); permissions(); audio(); nativeChat(); nativeConversations(); nativeThemeAndFriends(); std::cout << checks << " checks passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
