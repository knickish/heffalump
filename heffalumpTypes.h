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
    UInt8           current_toot_page_index;
} HeffalumpState;

typedef enum  {
    Favorite = 0,
    Follow = 1,
    Reblog = 2,
    Toot = 3,
    // to ensure the values chosen are < of size u8::MAX
    DoNotUse = 0xFF
} TootWriteType;

typedef union {
    UInt16 favorite;
    UInt16 reblog;
    UInt16 follow;
    TootContent toot;
} TootWriteContent;

typedef struct {
    UInt16 type;
    TootWriteContent content;
} TootWrite;
#endif