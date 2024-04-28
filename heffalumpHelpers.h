#include <PalmOS.h>
#include "heffalumpTypes.h"
#include "heffalumpStatics.h"

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

	return sharedVarsP->current_toot_content_record;
}

UInt16 SaturatingDecrement(UInt16 base) {
	return base == 0 ? base : base - 1;
}

static Boolean IsSpace(char c) {
	return c == '\t' || c == ' ';
}