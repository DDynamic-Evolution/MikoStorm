#include "llviewerprecompiledheaders.h"
#include "llsdutil.h"
#include "llmcpserver.h"
#include "llagentcamera.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llpresetsmanager.h"
#include "llfloaterreg.h"
#include "llfloatercameratimeline.h"
#include "llviewerstats.h"
#include "lltracerecording.h"
#include "llsdjson.h"
#ifdef USE_3D_STREAM
#include "llpositionalstreammgr.h"
#endif
#include <algorithm>
#include <cmath>
namespace
{
LLSD result(const LLSD& data)
{
    return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", boost::json::serialize(LlsdToJson(data)))));
}
LLSD failure(const std::string& text)
{
    LLSD value = result(LLSDMap("error", text)); value["isError"] = true; return value;
}
LLSD emptySchema() { return LLSDMap("type", "object")("properties", LLSD::emptyMap()); }
}
void LLMCPServer::registerViewerTools()
{
    const auto operation = LLSDMap("type", "object")("properties", LLSDMap("operation_id", LLSDMap("type", "string")("maxLength", 64)))
        ("required", llsd::array("operation_id"));
    registerTool("operation_status", "Get status/progress of an operation owned by this client session", operation,
        [this](const LLSD& p) { const auto state = operationStatus(p["operation_id"].asString()); return state.has("error") ? failure(state["error"].asString()) : result(state); });
    registerTool("operation_cancel", "Request cancellation; simulator actions already submitted may still complete", operation,
        [this](const LLSD& p) { const auto state = cancelOperation(p["operation_id"].asString()); return state.has("error") ? failure(state["error"].asString()) : result(state); });
    registerTool("camera_get_state", "Read local camera position, field of view and bone follow ID", emptySchema(),
        [](const LLSD&) {
            const auto& position = gAgentCamera.getCameraPositionAgent();
            return result(LLSDMap("x", position.mV[VX])("y", position.mV[VY])("z", position.mV[VZ])
                ("fov_radians", LLViewerCamera::getInstance()->getView())("joint_id", gSavedSettings.getS32("CameraFollowJoint")));
        });
    registerTool("camera_list_joints", "List available bone camera joints with numeric IDs", emptySchema(),
        [](const LLSD&) { LLSD joints = LLSD::emptyArray();
            for (const auto& [name, id] : getCameraJointList()) joints.append(LLSDMap("name", name)("id", id));
            return result(LLSDMap("joints", joints)); });
    registerTool("camera_set_joint", "Select bone camera joint; -1 disables bone follow",
        LLSDMap("type", "object")("properties", LLSDMap("joint_id", LLSDMap("type", "integer")("minimum", -1)("maximum", 65535)))
            ("required", llsd::array("joint_id")), [](const LLSD& p) {
                const S32 id = p["joint_id"].asInteger(); bool found = id == -1;
                for (const auto& joint : getCameraJointList()) if (joint.second == id) found = true;
                if (!found) return failure("Joint is not available on the current avatar");
                gSavedSettings.setS32("CameraFollowJoint", id); return result(LLSDMap("joint_id", id));
            });
    registerTool("camera_list_presets", "List existing local camera presets", emptySchema(), [](const LLSD&) {
        LLPresetsManager::preset_name_list_t names;
        LLPresetsManager::instance().loadPresetNamesFromDir(PRESETS_CAMERA, names, DEFAULT_SHOW);
        LLSD presets = LLSD::emptyArray(); for (const auto& name : names) { presets.append(name); if (presets.size() >= 200) break; }
        return result(LLSDMap("presets", presets)("truncated", names.size() > 200));
    });
    registerTool("camera_load_preset", "Load a named existing camera preset",
        LLSDMap("type", "object")("properties", LLSDMap("name", LLSDMap("type", "string")("minLength", 1)("maxLength", 128)))
            ("required", llsd::array("name")), [](const LLSD& p) {
                const auto name = p["name"].asString();
                if (name.find_first_of("/\\") != std::string::npos || name == "." || name == "..") return failure("Invalid preset name");
                LLPresetsManager::preset_name_list_t names;
                LLPresetsManager::instance().loadPresetNamesFromDir(PRESETS_CAMERA, names, DEFAULT_SHOW);
                if (std::find(names.begin(), names.end(), name) == names.end()) return failure("Camera preset does not exist");
                LLPresetsManager::instance().loadPreset(PRESETS_CAMERA, name);
                return result(LLSDMap("preset", name)("load_requested", true));
            });
    registerTool("timeline_get_state", "Read the current camera timeline without opening a window", emptySchema(), [](const LLSD&) {
        auto* floater = LLFloaterReg::findTypedInstance<LLFloaterCameraTimeline>("camera_timeline");
        return result(floater ? floater->getMCPState() : LLSDMap("playing", false)("keyframes", 0));
    });
    for (const std::string action : {"start", "stop", "capture"})
        registerTool("timeline_" + action, "Camera timeline: " + action, emptySchema(), [action](const LLSD&) {
            auto* floater = LLFloaterReg::showTypedInstance<LLFloaterCameraTimeline>("camera_timeline");
            if (!floater) return failure("Camera timeline is not available");
            const bool ok = action == "capture" ? floater->captureMCPKeyframe() : floater->setMCPPlayback(action == "start");
            if (!ok) return failure("Playback needs two keyframes; capture needs stopped playback and fewer than 256 keyframes");
            return result(floater->getMCPState());
        });
    registerTool("performance_get", "Read measured recent viewer frame rate and draw distance", emptySchema(), [](const LLSD&) {
        const F64 fps = LLTrace::get_frame_recording().getPeriodMedianPerSec(LLStatViewer::FPS, 10);
        const bool valid = std::isfinite(fps) && fps > 0.0;
        return result(LLSDMap("fps", valid ? LLSD(fps) : LLSD())("frame_ms", valid ? LLSD(1000.0 / fps) : LLSD())
            ("draw_distance_m", gSavedSettings.getF32("RenderFarClip"))("sample", "median of 10 recording periods"));
    });
#ifdef USE_3D_STREAM
    registerTool("audio_list_sources", "List active 3D sound sources; URLs and credentials are omitted",
        LLSDMap("type", "object")("properties", LLSDMap("offset", LLSDMap("type", "integer"))
            ("max_results", LLSDMap("type", "integer"))), [](const LLSD& p) {
                return result(LLPositionalStreamMgr::instance().getMCPStreams(p.has("offset") ? p["offset"].asInteger() : 0,
                    p.has("max_results") ? p["max_results"].asInteger() : 50));
            });
    registerTool("audio_mute_source", "Mute or unmute an active 3D source locally; parcel restrictions remain in force",
        LLSDMap("type", "object")("properties", LLSDMap("source_id", LLSDMap("type", "string")("maxLength", 36))
            ("muted", LLSDMap("type", "boolean")))("required", llsd::array("source_id", "muted")), [](const LLSD& p) {
                const bool muted = p["muted"].asBoolean();
                if (!LLPositionalStreamMgr::instance().setSourceMuted(LLUUID(p["source_id"].asString()), muted))
                    return failure("Unknown active source or local mute limit reached");
                return result(LLSDMap("source_id", p["source_id"])("muted", muted));
            });
#endif
}
