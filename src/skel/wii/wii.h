#ifndef _WII_H_
#define _WII_H_

#include "common.h"

typedef struct
{
    void* window;
    RwBool		fullScreen;
    RwV2d		lastMousePos;
    double      mouseWheel;
    bool        cursorIsInWindow;
    RwInt8        joy1id;
    RwInt8        joy2id;
}
psGlobalType;

#define PSGLOBAL(var) (((psGlobalType *)(RsGlobal.ps))->var)

// buttons the GameCube pad has no equivalent for
#define PAD_BUTTON_R1		0x10000
#define PAD_BUTTON_SELECT	0x20000
#define PAD_BUTTON_L3		0x40000
#define PAD_BUTTON_R3		0x80000

void CapturePad(RwInt32 padID);

#endif
