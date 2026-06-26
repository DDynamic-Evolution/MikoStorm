#include "pvassets.h"

#include "fsexportperms.h"
#include "llassetstorage.h"
#include "lldir.h"
#include "lldirpicker.h"
#include "llfilesystem.h"
#include "llformat.h"
#include "llinventorymodel.h"
#include "llnotificationsutil.h"
#include "lluuid.h"
#include "llviewerinventory.h"
#include "llviewercontrol.h"
#include "llviewermenufile.h"
#include "llviewerwindow.h"
#include "llwindow.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "pvextras.h"

struct save_data_entry
{
    LLAssetType::EType asset_type;
    LLFilePicker::ESaveFilter save_type;
    const char* file_ext;
};

static save_data_entry g_asset_save_data[] = {
    LLAssetType::AT_TEXTURE, LLFilePicker::FFSAVE_J2C, "j2c",
    LLAssetType::AT_ANIMATION, LLFilePicker::FFSAVE_ANIM, "anim",
    LLAssetType::AT_SOUND, LLFilePicker::FFSAVE_OGG, "ogg",
    LLAssetType::AT_NONE, LLFilePicker::FFSAVE_ALL, "",
};

static const save_data_entry& get_save_data(LLAssetType::EType asset_type)
{
    auto* entry = g_asset_save_data;
    for (; entry->file_ext[0] != '\0'; ++entry)
    {
        if (entry->asset_type == asset_type)
            return *entry;
    }

    return *entry;
}

static void pv_ensure_file_extension(std::string& filename, const save_data_entry& save_data)
{
    if (save_data.file_ext[0] == '\0')
        return;

    if (gDirUtilp->getExtension(filename) != save_data.file_ext)
    {
        filename += ".";
        filename += save_data.file_ext;
    }
}

struct save_cb_data
{
    save_data_entry data;
    std::string out_filename;
};

static void pv_save_asset_callback(const LLUUID& asset_uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status)
{
    save_cb_data cbdata = *(save_cb_data*)user_data;
    delete (save_cb_data*)user_data;

    if (status != 0)
    {
        LL_WARNS("LOSaveAsset") << "Problem fetching asset: " << asset_uuid << LL_ENDL;
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    LL_DEBUGS("LOSaveAsset") << "Saving asset " << asset_uuid.asString() << LL_ENDL;

    LLFileSystem file(asset_uuid, type);
    S32 file_length = file.getSize();
    std::vector<U8> buffer(file_length);

    if (!file.read(&buffer[0], file_length) || file.getLastBytesRead() != file_length)
    {
        LL_WARNS("LOSaveAsset") << "Failed to read input file: "  << asset_uuid.asString() << LL_ENDL;
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    if (pv_check_flag(PV_ANONYMIZE_EXPORTS) && type == LLAssetType::AT_TEXTURE)
        pv_strip_jpeg2000_comment(buffer);

    LLAPRFile outfile(cbdata.out_filename, LL_APR_WPB);

    if (outfile.getFileHandle())
    {
        if (outfile.write(buffer.data(), (S32)buffer.size()) != (S32)buffer.size())
        {
            LL_WARNS("LOSaveAsset") << "Failed to write output file: " << cbdata.out_filename << LL_ENDL;
            LLNotificationsUtil::add("ExportFailed");
            return;
        }
    }
    else
    {
        LL_WARNS("LOSaveAsset") << "Failed to open output file: " << cbdata.out_filename << LL_ENDL;
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    LLSD args;
    args["FILENAME"] = cbdata.out_filename;
    LLNotificationsUtil::add("ExportFinished", args);
}

void pv_save_asset(LLUUID asset_uuid, LLAssetType::EType asset_type, std::string out_filename)
{
    if (asset_uuid.isNull())
    {
        LL_WARNS("LOSaveAsset") << "Problem fetching asset: Null UUID" << LL_ENDL;
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    const save_data_entry& save_data = get_save_data(asset_type);

    if (save_data.asset_type == LLAssetType::AT_NONE)
    {
        LL_WARNS("LOSaveAsset") << "Problem fetching asset: Not an exportable asset type" << LL_ENDL;
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    save_cb_data* cbdata = new save_cb_data{ save_data, std::move(out_filename) };
    gAssetStorage->getAssetData(asset_uuid, asset_type, pv_save_asset_callback, (void*)cbdata, true);
}

void pv_save_asset(LLUUID asset_uuid, LLAssetType::EType asset_type)
{
    const save_data_entry& save_data = get_save_data(asset_type);

    if (save_data.asset_type == LLAssetType::AT_NONE)
    {
        LL_WARNS("LOSaveAsset") << "Problem fetching asset: Not an exportable asset type" << LL_ENDL;
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    std::string name;
    std::string description;
    FSExportPermsCheck::canExportAsset(asset_uuid, &name, &description);
    std::string out_filename = name.empty() ? asset_uuid.asString() : name;
    pv_ensure_file_extension(out_filename, save_data);

    LLFilePickerReplyThread::startPicker([=](auto&& filenames, auto&&, auto&&)
    {
        save_cb_data* cbdata = new save_cb_data{ save_data, std::move(filenames[0]) };
        gAssetStorage->getAssetData(asset_uuid, asset_type, pv_save_asset_callback, (void*)cbdata, true);
    }, save_data.save_type, out_filename);
}

void pv_copy_uuid(LLUUID asset_uuid)
{
    gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(asset_uuid.asString()));
}

void pv_inv_save(LLUUID item_id)
{
    const LLViewerInventoryItem* item = gInventory.getItem(item_id);

    if (item && item->getIsLinkType())
        item = gInventory.getItem(item->getLinkedUUID());

    if (!item)
    {
        LL_WARNS("LOSaveAsset") << "Problem fetching inventory item: Null UUID" << LL_ENDL;
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    LLUUID asset_uuid = item->getAssetUUID();
    LLAssetType::EType asset_type = item->getType();
    pv_save_asset(item->getAssetUUID(), asset_type);
}

void pv_inv_save_multiple(uuid_vec_t item_ids)
{
    if (item_ids.size() == 1)
    {
        pv_inv_save(item_ids[0]);
        return;
    }

    std::vector<std::pair<LLUUID, LLAssetType::EType>> assets;

    (new LLDirPickerThread([
        item_ids = std::move(item_ids),
        assets = std::move(assets)
    ](auto&& filenames, auto&&)
    {
        auto&& dirname = filenames[0];
        std::map<std::string, S32> tex_names_map;

        for (const auto& item_id : item_ids)
        {
            const LLViewerInventoryItem* item = gInventory.getItem(item_id);

            if (item && item->getIsLinkType())
                item = gInventory.getItem(item->getLinkedUUID());

            if (!item)
            {
                LL_WARNS("LOSaveAsset") << "Problem fetching inventory item: Null UUID" << LL_ENDL;
                LLNotificationsUtil::add("ExportFailed");
                continue;
            }

            LLUUID asset_uuid = item->getAssetUUID();

            if (asset_uuid.isNull())
            {
                LL_WARNS("LOSaveAsset") << "Problem fetching asset: Null UUID" << LL_ENDL;
                LLNotificationsUtil::add("ExportFailed");
                continue;
            }

            LLAssetType::EType asset_type = item->getType();
            const save_data_entry& save_data = get_save_data(asset_type);

            if (save_data.asset_type == LLAssetType::AT_NONE)
            {
                LL_WARNS("LOSaveAsset") << "Problem fetching asset: Not an exportable asset type" << LL_ENDL;
                LLNotificationsUtil::add("ExportFailed");
                continue;
            }

            std::string out_filename = dirname + gDirUtilp->getDirDelimiter() + item->getName();
            if (tex_names_map[out_filename]++ != 0)
                out_filename += llformat("_%.3d", tex_names_map[out_filename]);
            pv_ensure_file_extension(out_filename, save_data);
            save_cb_data* cbdata = new save_cb_data{ save_data, std::move(out_filename) };
            gAssetStorage->getAssetData(asset_uuid, asset_type, pv_save_asset_callback, (void*)cbdata, true);
        }
    }, std::string()))->run();
}
