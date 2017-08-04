#include <string.h>

#include "m_sr.h"

/* Save, restore routines */

void SaveMusStatus(struct MusData *Buf)
{
    memcpy(Buf, &MusData, sizeof(*Buf));
    /* FIXME: Save music device status */
}

void RestoreMusStatus(const struct MusData *Buf)
{
    memcpy(&MusData, Buf, sizeof(*Buf));
    if(MusData.driver)
    	MusData.driver->reset();
}
