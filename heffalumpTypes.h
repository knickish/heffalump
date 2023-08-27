#ifndef HEFFLUMP_TYPES_H_
#define HEFFLUMP_TYPES_H_

#include <PalmOS.h>
#include <stdint.h>

typedef struct TootContent_s {
    UInt16  author;
    UInt16  is_reply_to;
    UInt16  content_len;
    char    toot_content[];
} TootContent;

typedef struct TootAuthor_s {
    UInt8 author_name_len;
    char  author_name[];
} TootAuthor;

typedef struct HeffalumpState_s {
    UInt16          current_toot_author_record;
    UInt16          current_toot_content_record;
    MemHandle       current_toot_author_handle;
    MemHandle       current_toot_content_handle;
    TootAuthor*     current_toot_author_ptr;
    TootContent*    current_toot_content_ptr;
} HeffalumpState;

#endif