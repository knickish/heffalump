#ifndef HEFFLUMP_H_
#define HEFFLUMP_H_

#include <stdint.h>

#define MainForm                1000
#define MainNameField           1000
#define MainHelloButton         1001
#define MainGoodbyeButton       1002

#define MainMenuBar             1000
#define EditUndo                1000
#define EditCut                 1001
#define EditCopy                1002
#define EditPaste               1003
#define EditSelectAll           1004
#define EditKeyboard            1005
#define EditGrafittiHelp        1006
#define OptionsAboutHelloWorld2 1007

#define AboutForm               1001
#define AboutOKButton           1000

#define HelloAlert              1000
#define GoodbyeAlert            1001

#define BmpFamilyAppIcon        1000

typedef struct Toot {
    char*       author;
    uint16_t    is_reply_to;
    uint16_t    toot_id;
} Toot;


char* LoadToot(uint16_t toot_id);
char* GetTootAuthor(Toot toot);
char* GetTootText(Toot toot);

#endif