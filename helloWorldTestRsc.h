#ifndef HEFFLUMP_TEST_RSC_H_
#define HEFFLUMP_TEST_RSC_H_

#include <PalmOS.h>
#include "heffalumpTypes.h"

// static char sample_toot_text[44] = "the quick brown fox jumps over the lazy dog";
// static char sample_toot_author_name[11] = "test author";

// must free the name ptr
TootAuthor* RandomTootAuthor() {
    UInt8 toot_author_name_len = SysRandom(0) % 40;
    if (toot_author_name_len == 0) {
        toot_author_name_len = 5;
    }
    TootAuthor* toot_author_ptr = (TootAuthor*) MemPtrNew((UInt32)(sizeof(TootAuthor) + (UInt32)toot_author_name_len));
    *&(toot_author_ptr->author_name_len) = toot_author_name_len;
    char c = SysRandom(0) % 200;
    if (c == 0) {
        c += 50;
    }
    for (UInt16 i = 0; i<toot_author_name_len; i++) {
        
        *((&(toot_author_ptr->author_name))+i) = c;
    }
    *((&(toot_author_ptr->author_name))+toot_author_name_len) = '\0';
    
    return toot_author_ptr;
}

// typedef struct TootContent_s {
//     UInt16  author;
//     UInt16  is_reply_to;
//     UInt16  content_len;
//     Char*   toot_content;
// } TootContent;

// must free the name ptr
TootContent* RandomTootContent(UInt8 max) {
    // FrmCustomAlert(DebugAlert1, "Generating TOOT Content Author",NULL, NULL);
    if (max == 0) { max = 1; }
    UInt16 toot_author= SysRandom(0) % max;

    
    // char* toot_author_str = "          ";
    // FrmCustomAlert(DebugAlert2, "Toot Content Author is ", StrIToA(toot_author_str, toot_author), NULL);

    UInt16 is_reply_to = 0;
    UInt16 toot_content_len = SysRandom(0) % 1000;
    if (toot_content_len <10) {
        toot_content_len = 10;
    }
    
    TootContent* toot_content_ptr = (TootContent*) MemPtrNew(sizeof(TootContent) + toot_content_len - sizeof(char));
    if (toot_content_ptr == NULL) {
        ErrDisplay("Failed to allocate Toot content mem");
    }

    toot_content_ptr->author = toot_author;
    toot_content_ptr->is_reply_to = is_reply_to;
    toot_content_ptr->content_len = toot_content_len;
    
    char c = SysRandom(0) % 200;
    if (c == 0) {
        c += 50;
    }
    for (UInt16 i = 0; i<toot_content_len; i++) {
        *((&(toot_content_ptr->toot_content))+i) = c;
    }
    
    return toot_content_ptr;
}
#endif