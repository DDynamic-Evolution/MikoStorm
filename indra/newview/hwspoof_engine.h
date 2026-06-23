#ifndef HWSPOOF_ENGINE_H
#define HWSPOOF_ENGINE_H

#include <string>
#include "llsd.h"

const std::string& hwspoof_get_seed();
const std::string& hwspoof_get_username();

void hwspoof_reroll_seed();
void hwspoof_set_seed(const std::string&);
void hwspoof_set_username(const std::string& username);

void hwspoof_set_real_serial(std::string serial);
void hwspoof_set_real_nodeid(unsigned char nodeid[6]);
void hwspoof_set_real_machineid(unsigned char machineid[6]);

const std::string& hwspoof_get_real_serial();
const std::string& hwspoof_get_real_macid_str();
const std::string& hwspoof_get_real_nodeid_str();
const std::string& hwspoof_get_real_machineid_str();

const std::string& hwspoof_get_id0();
const std::string& hwspoof_get_macid();

bool hwspoof_is_initialized();

void hwspoof_get_faux_nodeid(unsigned char out[6]);
const std::string& hwspoof_get_faux_nodeid_str();
void hwspoof_get_faux_machineid(unsigned char out[6]);
const std::string& hwspoof_get_faux_machineid_str();

void hwspoof_fake_support_info(LLSD& info, std::string build_type_string = std::string());

#endif
