#include <string.h>

#include "music.h"
#include "m_mpu.h"
#include "m_opl.h"

/* Save, restore routines */

void SaveMusStatus(struct MusData *Buf)
{
    memcpy(Buf, &MusData, sizeof(MusData));
}

void RestoreMusStatus(struct MusData *Buf)
{
    memcpy(&MusData, Buf, sizeof(MusData));
    GM_Reset();
    OPL_Reset();
    #if SUPPORT_AWE
    AWE_Reset();
    #endif
}
