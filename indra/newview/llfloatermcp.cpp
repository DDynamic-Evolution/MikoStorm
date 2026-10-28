#include "llviewerprecompiledheaders.h"
#include "llsdutil.h"
#include "llfloatermcp.h"
#include "llmcpserver.h"
#include "llviewercontrol.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "llclipboard.h"
#include "llsdjson.h"
namespace
{
const char* controls[] = {"allow_read", "allow_camera", "allow_move", "allow_chat", "allow_inventory"};
}
void LLFloaterMCP::copy(const std::string& text)
{
    const LLWString wide = utf8str_to_wstring(text);
    LLClipboard::instance().copyToClipboard(wide, 0, static_cast<S32>(wide.size()));
}
bool LLFloaterMCP::postBuild()
{
    getChild<LLButton>("start")->setCommitCallback([](LLUICtrl*, const LLSD&) {
        if (gSavedSettings.getBOOL("MCPEnabled")) LLMCPServer::configureLive();
        else gSavedSettings.setBOOL("MCPEnabled", true);
    });
    getChild<LLButton>("stop")->setCommitCallback([](LLUICtrl*, const LLSD&) { gSavedSettings.setBOOL("MCPEnabled", false); });
    getChild<LLButton>("restart")->setCommitCallback([](LLUICtrl*, const LLSD&) { LLMCPServer::configureLive(); });
    getChild<LLButton>("copy_url")->setCommitCallback([](LLUICtrl*, const LLSD&) {
        copy("http://127.0.0.1:" + std::to_string(gSavedSettings.getU32("MCPPort")) + "/mcp");
    });
    getChild<LLButton>("generate_token")->setCommitCallback([this](LLUICtrl*, const LLSD&) {
        auto token = LLMCPServer::generateToken();
        if (token.empty()) { getChild<LLTextBox>("feedback")->setText(getString("token_failed")); return; }
        gSavedSettings.setString("MCPAuthToken", token);
        mGeneratedToken = token; getChild<LLLineEditor>("new_token")->setValue(token);
        getChild<LLTextBox>("feedback")->setText(getString("global_created"));
    });
    getChild<LLButton>("copy_token")->setCommitCallback([this](LLUICtrl*, const LLSD&) { if (!mGeneratedToken.empty()) copy(mGeneratedToken); });
    getChild<LLComboBox>("client")->setCommitCallback([this](LLUICtrl*, const LLSD&) { selectClient(); });
    getChild<LLButton>("save_permissions")->setCommitCallback([this](LLUICtrl*, const LLSD&) { savePermissions(); });
    getChild<LLButton>("create_client")->setCommitCallback([this](LLUICtrl*, const LLSD&) {
        const auto name = getChild<LLLineEditor>("client_name")->getText();
        const auto before = LLMCPServer::instance().getClientProfiles();
        auto token = LLMCPServer::instance().createClient(name, LLMCPServer::Read);
        if (token.empty()) { getChild<LLTextBox>("feedback")->setText(getString("token_failed")); return; }
        mGeneratedToken = token; getChild<LLLineEditor>("new_token")->setValue(token);
        refresh();
        for (const auto& profile : llsd::inArray(mProfiles))
        {
            bool existed = false;
            for (const auto& old : llsd::inArray(before)) if (old["id"].asString() == profile["id"].asString()) existed = true;
            if (!existed) getChild<LLComboBox>("client")->setValue(profile["id"]);
        }
        selectClient(); getChild<LLTextBox>("feedback")->setText(getString("client_created"));
    });
    getChild<LLButton>("remove_client")->setCommitCallback([this](LLUICtrl*, const LLSD&) {
        LLMCPServer::instance().removeClient(getChild<LLComboBox>("client")->getValue().asString());
        refresh(); getChild<LLTextBox>("feedback")->setText(getString("client_removed"));
    });
    return true;
}
void LLFloaterMCP::onOpen(const LLSD&) { LLMCPServer::instance().refreshAccess(); refresh(); }
void LLFloaterMCP::onClose(bool quitting)
{
    mGeneratedToken.clear(); getChild<LLLineEditor>("new_token")->clear(); LLFloater::onClose(quitting);
}
void LLFloaterMCP::selectClient()
{
    const auto id = getChild<LLComboBox>("client")->getValue().asString();
    for (const auto& profile : llsd::inArray(mProfiles)) if (profile["id"].asString() == id)
        for (U32 bit = 0; bit < 5; ++bit) getChild<LLCheckBoxCtrl>(controls[bit])->setValue((profile["permissions"].asInteger() & (1U << bit)) != 0);
    getChild<LLButton>("remove_client")->setEnabled(id != "default" && id != "media-plugin" && !id.empty());
}
void LLFloaterMCP::savePermissions()
{
    U32 mask = 0;
    for (U32 bit = 0; bit < 5; ++bit) if (getChild<LLCheckBoxCtrl>(controls[bit])->getValue().asBoolean()) mask |= 1U << bit;
    LLMCPServer::instance().setClientPermissions(getChild<LLComboBox>("client")->getValue().asString(), mask);
    refresh(); getChild<LLTextBox>("feedback")->setText(getString("permissions_saved"));
}
void LLFloaterMCP::refresh()
{
    auto& server = LLMCPServer::instance();
    const LLSD status = server.getStatus();
    getChild<LLTextBox>("status")->setText(getString(status["running"].asBoolean() ? "running" : "stopped") +
        llformat(" | Port %u | %d %s | %d %s", gSavedSettings.getU32("MCPPort"), status["sessions"].asInteger(), getString("sessions").c_str(),
            status["operations"].asInteger(), getString("operations").c_str()) + "\n" + status["last_error"].asString());
    const auto profiles = server.getClientProfiles();
    if (boost::json::serialize(LlsdToJson(profiles)) != boost::json::serialize(LlsdToJson(mProfiles)))
    {
        auto* combo = getChild<LLComboBox>("client"); const auto old = combo->getValue();
        combo->clearRows(); mProfiles = profiles;
        for (const auto& profile : llsd::inArray(profiles)) combo->add(profile["name"].asString(), profile["id"]);
        combo->setValue(old);
        if (combo->getCurrentIndex() < 0) combo->setCurrentByIndex(0);
        selectClient();
    }
    const LLSD audit = server.getAudit(); std::string lines;
    const S32 auditCount = static_cast<S32>(audit.size()); // Audit is capped at 100 entries.
    for (S32 index = llmax(0, auditCount - 8); index < auditCount; ++index)
    {
        const auto timestamp = audit[index]["time"].asString();
        const auto client = audit[index]["client"].asString();
        lines += (timestamp.size() >= 19 ? timestamp.substr(11, 8) : timestamp) + " | " + client.substr(0, 12) + " | " +
            audit[index]["tool"].asString() + " | " + audit[index]["status"].asString() + "\n";
    }
    getChild<LLTextBox>("recent_calls")->setText(lines.empty() ? getString("no_calls") : lines);
    getChild<LLButton>("copy_token")->setEnabled(!mGeneratedToken.empty());
    mRefresh.reset();
}
void LLFloaterMCP::draw()
{
    if (mRefresh.getElapsedTimeF32() >= .5f) refresh();
    LLFloater::draw();
}
