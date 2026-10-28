#include "llviewerprecompiledheaders.h"
#include "llpositionalstreammgr.h"
#include "llworkbudget.h"
#include "llagent.h"
#include "llfasttimer.h"
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
static LLTrace::BlockTimerStatHandle FTM_STREAM3D_DISCOVERY("3D Stream Discovery");

void LLPositionalStreamMgr::detectLinksetStructureChanges()
{
    LLWorkBudget work(128, std::chrono::milliseconds(1));
    // Carry pending roots across ticks; evaluating a root can change mPrimToRoot.
    while (!mStructureChanges.empty() && work.take())
    {
        const LLUUID root = *mStructureChanges.begin();
        mStructureChanges.erase(mStructureChanges.begin());
        evaluateLinkset(root);
    }
    if (mPrimToRoot.empty()) return;
    auto current = mPrimToRoot.upper_bound(mStructureCursor);
    std::size_t remaining = mPrimToRoot.size();
    while (remaining-- && work.take())
    {
        if (current == mPrimToRoot.end()) current = mPrimToRoot.begin();
        const auto [prim_id, registered_root] = *current++;
        mStructureCursor = prim_id;
        LLViewerObject* object = gObjectList.findObject(prim_id);
        if (!object || object->isDead()) { mStructureChanges.insert(registered_root); continue; }
        LLViewerObject* root = object->getRootEdit();
        const LLUUID new_root = root ? root->getID() : prim_id;
        if (new_root != registered_root)
        {
            mStructureChanges.insert(registered_root);
            mStructureChanges.insert(new_root);
        }
    }
}

void LLPositionalStreamMgr::pollObjectPropertiesFamily(F64 now)
{
    // M8: Stream3DDescriptionScan also gates the active poll. (Enabled is
    // checked one level up in update(); reaching here implies Enabled=true.)
    if (!gSavedSettings.getBOOL("Stream3DDescriptionScan"))
    {
        return;
    }

    LL_RECORD_BLOCK_TIME(FTM_STREAM3D_DISCOVERY);
    // Run the scan at ~2 Hz; with kBudgetPerScan = 5 that caps outgoing
    // requests at ~10 req/s sustained, regardless of frame rate.
    // 10 req/s × 30s poll_interval = ~300 prims per cycle. In denser regions
    // a given prim's effective re-poll interval will exceed Stream3DPollInterval.
    static constexpr F64 kScanInterval  = 0.5;
    static constexpr int kBudgetPerScan = 5;
    // r8 F8: priority queue retry cadence. Far below Stream3DPollInterval so
    // a freshly discovered linkset finishes binding within seconds even when
    // the first Family request was dropped, but capped to kPriorityRetryCap
    // attempts so prims the sim refuses to answer for don't burn the whole
    // priority budget every tick. After the cap, round-robin (poll_interval
    // cadence) takes over silently.
    static constexpr F64 kPriorityRetryWait = 3.0;
    static constexpr S32 kPriorityRetryCap  = 5;

    if (now - mLastPollScanTime < kScanInterval)
    {
        return;
    }
    mLastPollScanTime = now;

    const F32 poll_interval = gSavedSettings.getF32("Stream3DPollInterval");
    const F32 max_dist      = gSavedSettings.getF32("Stream3DMaxDistance");

    // Per-scan breakdown for support / tuning. Off by default; enable with
    //   --logdebug Stream3D
    LL_DEBUGS("Stream3D") << "poll diag: interval=" << poll_interval
                           << " max_dist=" << max_dist
                           << " num_objects=" << gObjectList.getNumObjects()
                           << LL_ENDL;

    if (poll_interval <= 0.f)
    {
        return;
    }

    if (max_dist <= 0.f)
    {
        return;
    }
    const F32 max_dist_sq = max_dist * max_dist;
    const LLVector3d agent_pos = gAgent.getPositionGlobal();

    // r8 F2-c: spot link/unlink/death once per scan tick. This is cheap and
    // independent of the request budget — it only reads gObjectList and may
    // call evaluateLinkset (which itself may enqueue priority polls picked
    // up by the drain below).
    detectLinksetStructureChanges();

    int budget = kBudgetPerScan;
    LLWorkBudget work(512, std::chrono::milliseconds(2));

    // r8 F2-b / F8: drain the priority queue first. These are prims that
    // evaluateLinkset wants polled now (root or speaker that we don't yet
    // have description for). Distance / avatar filtering still applies.
    //
    // Retry policy (F8): a prim stays in the queue across drains until either
    // its reply arrives (last_replied stamped) or kPriorityRetryCap attempts
    // were spent. The previous policy used the same 30 s round-robin throttle
    // here, so a child whose first Family request was dropped by the sim sat
    // mute for 30 s before round-robin retried — observable as the "16 spk
    // linkset only plays 1 speaker until the user touches it" symptom.
    //
    // Snapshot the queue size before draining so re-queued items don't get
    // re-processed in the same tick (same prim could otherwise be sent twice
    // back-to-back when budget is large).
    int n_priority = 0;
    size_t to_process = mPriorityPollQueue.size();
    while (budget > 0 && to_process > 0 && !mPriorityPollQueue.empty() && work.take())
    {
        --to_process;
        const LLUUID id = mPriorityPollQueue.front();
        mPriorityPollQueue.pop_front();

        LLViewerObject* obj = gObjectList.findObject(id);
        if (!obj || obj->isDead()) continue;
        if (obj->isAvatar()) continue;
        if (!gSavedSettings.getBOOL("Stream3DScanAttachments")
            && (obj->isAttachment() || obj->isHUDAttachment())) continue;

        LLVector3d delta = obj->getPositionGlobal();
        delta -= agent_pos;
        if (static_cast<F32>(delta.magVecSquared()) > max_dist_sq) continue;

        auto& entry = mDescriptionCache[id];

        // Reply already arrived: round-robin owns this prim now.
        if (entry.last_replied > 0.0) continue;
        // Spent the priority budget without a reply: let round-robin retry
        // at its slower cadence. Don't re-queue.
        if (entry.priority_retries >= kPriorityRetryCap) continue;
        // Within retry-wait window since the last send: keep the slot but
        // wait for the next drain.
        if (entry.last_polled > 0.0 && (now - entry.last_polled) < kPriorityRetryWait)
        {
            mPriorityPollQueue.push_back(id);
            continue;
        }

        LLSelectMgr::getInstance()->requestObjectPropertiesFamily(obj);
        entry.last_polled = now;
        entry.priority_retries++;
        --budget;
        ++n_priority;

        // Re-queue so the next drain checks whether the reply arrived and,
        // if not, retries (subject to the cap above).
        mPriorityPollQueue.push_back(id);
    }

    const S32 num = gObjectList.getNumObjects();
    if (num == 0)
    {
        return;
    }

    int n_total = 0, n_filtered = 0, n_far = 0, n_recent = 0, n_sent = 0;
    int examined = 0;

    // Walk in round-robin order so dense regions don't starve prims past
    // the first slice each pass can reach before exhausting the budget.
    while (budget > 0 && examined < num && work.take())
    {
        if (mPollCursor >= num)
        {
            mPollCursor = 0;
        }
        const S32 i = mPollCursor++;
        ++examined;

        LLViewerObject* obj = gObjectList.getObject(i);
        if (!obj || obj->isDead())
        {
            continue;
        }
        ++n_total;

        // Avatars themselves never carry stream tags; attachments / HUDs
        // are excluded unless the user explicitly opts in.
        if (obj->isAvatar()
            || (!gSavedSettings.getBOOL("Stream3DScanAttachments")
                && (obj->isAttachment() || obj->isHUDAttachment())))
        {
            ++n_filtered;
            continue;
        }

        LLVector3d delta = obj->getPositionGlobal();
        delta -= agent_pos;
        if (static_cast<F32>(delta.magVecSquared()) > max_dist_sq)
        {
            ++n_far;
            continue;
        }

        const LLUUID& id = obj->getID();
        // operator[] auto-creates an empty entry; harmless — last_polled stamps
        // it so we don't re-request before the interval elapses.
        auto& entry = mDescriptionCache[id];
        if (entry.last_polled > 0.0 && (now - entry.last_polled) < poll_interval)
        {
            ++n_recent;
            continue;
        }

        LLSelectMgr::getInstance()->requestObjectPropertiesFamily(obj);
        entry.last_polled = now;
        --budget;
        ++n_sent;
    }

    LL_DEBUGS("Stream3D") << "poll: scanned this pass total=" << n_total
                           << " filtered=" << n_filtered
                           << " far=" << n_far
                           << " recent=" << n_recent
                           << " sent=" << n_sent
                           << " priority_sent=" << n_priority
                           << " priority_queue=" << mPriorityPollQueue.size()
                           << " examined=" << examined
                           << " elapsed_ms=" << work.elapsedMilliseconds()
                           << " cursor=" << mPollCursor
                           << "/" << num
                           << LL_ENDL;
}
