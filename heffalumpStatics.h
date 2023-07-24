#ifndef HEFFLUMP_STATICS_H_
#define HEFFLUMP_STATICS_H_

// #include <PalmOS.h>

//globals (8 slots maximum, each stores a void*, zero-inited at app start)

#define NUM_GLOBALS_SLOTS		8

register void** a5 asm("a5");

static inline void** globalsSlotPtr(UInt8 slotID)	//[0] is reserved
{
    if (!slotID || slotID > NUM_GLOBALS_SLOTS)
        return NULL;

    return a5 + slotID;
}

static inline void* globalsSlotVal(UInt8 slotID)	//[0] is reserved
{
    if (!slotID || slotID > NUM_GLOBALS_SLOTS)
        return NULL;

    return a5[slotID];
}

#define GLOBALS_SLOT_SHARED_VARS			1
#define GLOBALS_SLOT_AUTHOR_DB              2
#define GLOBALS_SLOT_CONTENT_DB             3

#endif