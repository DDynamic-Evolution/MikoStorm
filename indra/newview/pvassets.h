#ifndef PANDAVIEW_PVASSETS_H
#define PANDAVIEW_PVASSETS_H

#include "llassettype.h"
#include "llfilepicker.h"
#include "lluuid.h"

void pv_save_asset(LLUUID asset_uuid, LLAssetType::EType asset_type, std::string out_filename);
void pv_save_asset(LLUUID asset_uuid, LLAssetType::EType asset_type);
void pv_copy_uuid(LLUUID asset_uuid);

void pv_inv_save(LLUUID item_id);
void pv_inv_save_multiple(uuid_vec_t item_ids);

#endif // PANDAVIEW_PVASSETS_H
