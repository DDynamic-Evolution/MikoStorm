#ifndef PANDAVIEW_PVEXTRAS_H
#define PANDAVIEW_PVEXTRAS_H

#include "stdtypes.h"

#include <string>
#include <vector>

#define PV_CONVENIENCE             0x00000001U
#define PV_BYPASS_EXPORT_PERMS     0x00000002U
#define PV_ENHANCED_EXPORT         0x00000004U
#define PV_ANONYMIZE_EXPORTS       0x00000008U

#define PV_FEATURE_MASK            0x0000000FU

void pv_set_flags(unsigned flags, unsigned mask);
unsigned pv_get_flags();
unsigned pv_get_mask();

unsigned pv_new_defaulted_flags();

void pv_enable_flag(unsigned flag);
void pv_disable_flag(unsigned flag);
bool pv_check_flag(unsigned flag);

void pv_strip_jpeg2000_comment(std::string&);
void pv_strip_jpeg2000_comment(std::vector<unsigned char>&);

#endif // PANDAVIEW_PVEXTRAS_H
