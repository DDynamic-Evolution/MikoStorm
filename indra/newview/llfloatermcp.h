#ifndef LL_FLOATER_MCP_H
#define LL_FLOATER_MCP_H
#include "llfloater.h"
#include "lltimer.h"
class LLFloaterMCP : public LLFloater
{
    friend class LLFloaterReg;
public:
    explicit LLFloaterMCP(const LLSD& key) : LLFloater(key) {}
    bool postBuild() override;
    void draw() override;
    void onOpen(const LLSD&) override;
    void onClose(bool app_quitting) override;
private:
    void refresh();
    void selectClient();
    void savePermissions();
    static void copy(const std::string& text);
    LLTimer mRefresh;
    LLSD mProfiles;
    std::string mGeneratedToken;
};
#endif
