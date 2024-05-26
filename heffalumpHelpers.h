#include <PalmOS.h>
#include "heffalumpTypes.h"
#include "heffalumpStatics.h"
#include "heffalumpRsc.h"

static void SetLabelInForm(FormType *form, UInt16 labelID, const char *newText)
{
  UInt16 labelObjectIndex = FrmGetObjectIndex(form, labelID);

  FrmHideObject(form, labelObjectIndex);
  if (newText != NULL) {
	FrmCopyLabel(form, labelID, newText);
  	FrmShowObject(form, labelObjectIndex);
  }
}

static void HideButtonInForm(FormType *form, UInt16 buttonID) {
	UInt16 buttonObjectIndex = FrmGetObjectIndex(form, buttonID);
	FrmHideObject(form, buttonObjectIndex);
}

static void ShowButtonInForm(FormType *form, UInt16 buttonID) {
	UInt16 buttonObjectIndex = FrmGetObjectIndex(form, buttonID);
	FrmShowObject(form, buttonObjectIndex);
}

static UInt16 CurrentTootRecordNumber() {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");
	if (sharedVarsP->viewing_self_timeline) {
		return sharedVarsP->self_toot_content_record;
	} else {
		return sharedVarsP->current_toot_content_record;
	}
}

static UInt16 CurrentTootFirstReplyRecordNumber() {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");
	return sharedVarsP->current_toot_content_ptr->replies_start;
}

UInt16 SaturatingDecrement(UInt16 base) {
	return base == 0 ? base : base - 1;
}

static Boolean IsSpace(char c) {
	return c == '\t' || c == ' ';
}

#ifdef HEFFALUMP_TESTING
void DebugPrefs() {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	char* debug = MemPtrNew(150);
	MemSet(debug, 150, 0);
	StrPrintF(
		debug, 
		"viewing_self_timeline: %u\nviewing_replies: %u\nviewing_replies_to: %u", 
		(UInt16)sharedVarsP->viewing_self_timeline,
		(UInt16)sharedVarsP->viewing_replies,
		sharedVarsP->viewing_replies_to
	);
	FrmCustomAlert(DebugAlert1, debug, NULL, NULL);
	MemPtrFree(debug);				
}
#endif // HEFFALUMP_TESTING

static void GetButtonCoordinates(FormType *form, UInt16 buttonID, RectangleType *buttonRect) {
    // Get the index of the button in the form
    UInt16 buttonIndex = FrmGetObjectIndex(form, buttonID);
    // Get the coordinates of the button
    FrmGetObjectBounds(form, buttonIndex, buttonRect);
}

static void ToggleRepliesButtonInversion(HeffalumpState* sharedVarsP, FormType *form) {
	if (sharedVarsP->inverted_replies_button == NULL) {
		RectangleType* rect = MemPtrNew(sizeof(RectangleType));
		GetButtonCoordinates(form, MainRepliesButton, rect);
		sharedVarsP->inverted_replies_button = rect;
	} else {
		MemPtrFree(sharedVarsP->inverted_replies_button);
		sharedVarsP->inverted_replies_button = NULL;
	}
}

static void PaintRepliesButtonInversion(HeffalumpState* sharedVarsP, FormType *form) {
	if (sharedVarsP->inverted_replies_button != NULL) {
		WinInvertRectangle(sharedVarsP->inverted_replies_button, 2);
	}
}