#ifndef HEFFLUMP_TYPES_H_
#define HEFFLUMP_TYPES_H_

#include <PalmOS.h>
#include <stdint.h>

typedef struct HeffalumpState_s {
    UInt16      current_toot;
} HeffalumpState;

typedef struct TootContent_s {
    UInt16  author;
    UInt16  is_reply_to;
    UInt16  content_len;
    char   toot_content;
} TootContent;

typedef struct TootAuthor_s {
    UInt8 author_name_len;
    char  author_name;
} TootAuthor;

// must free content ptr
TootContent GetTootContent(UInt16 idx);
void CleanupLoadedTootContent(TootContent toot);

TootAuthor GetTootAuthor(TootContent toot);
void CleanupLoadedTootAuthor(TootAuthor author);

#endif