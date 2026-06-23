#ifndef FLOATER_HWSPOOF_H
#define FLOATER_HWSPOOF_H

#include "llfloater.h"

class FloaterHWSpoof : public LLFloater
{
public:
    FloaterHWSpoof(const LLSD& key);
    virtual ~FloaterHWSpoof() = default;

    bool postBuild() override;

private:
    void updateLabels();
    void onClickReroll();
};

#endif
