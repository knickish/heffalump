#define ERROR_CHECK_FULL
#include "helloWorld_Rsc.h"
#include "heffalumpTypes.h"
#include "heffalumpStatics.h"
#include <PalmOS.h>


#define HEFFALUMP_NO_DB_DEV
#ifdef HEFFALUMP_NO_DB_DEV
#include "helloWorldTestRsc.h"
#endif

static void SaySomething(UInt16 alertID) {
	FormType 	*form = FrmGetActiveForm();
	FieldType	*field = FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainNameField));

	if (FldGetTextLength(field) > 0) {
		FrmCustomAlert(alertID, FldGetTextPtr(field), NULL, NULL);
	} else {
		FrmCustomAlert(alertID, "whoever you are", NULL, NULL);
	}
}

static Boolean HandleEditOptions(UInt16 menuID, FieldType* field) {
	Boolean handled = false;
	switch (menuID) {
		case EditUndo:
			FldUndo(field);
			handled = true;
			break;

		case EditCut:
			FldCut(field);
			handled = true;
			break;

		case EditCopy:
			FldCopy(field);
			handled = true;
			break;

		case EditPaste:
			FldPaste(field);
			handled = true;
			break;

		case EditSelectAll:
			FldSetSelection(field, 0, FldGetTextLength(field));
			handled = true;
			break;

		case EditKeyboard:
			SysKeyboardDialogV10();
			handled = true;
			break;
	}
	return handled;
}

static Boolean MainMenuHandleEvent(UInt16 menuID) {
	Boolean 	handled = false;
	FormType 	*form;
	FieldType	*field;

	form = FrmGetActiveForm();
	field = FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainNameField));

	switch (menuID) {
		case EditUndo:
		case EditCut:
		case EditCopy:
		case EditPaste:
		case EditSelectAll:
		case EditKeyboard:
			handled = HandleEditOptions(menuID, field);
			break;

		#ifdef PALMOS_GE_V_2
		case EditGrafittiHelp:
			SysGraffitiReferenceDialog(referenceDefault);
			handled = true;
			break;
		#endif

		case OptionsAboutHelloWorld2:
			MenuEraseStatus(0);
			form = FrmInitForm(AboutForm);
			FrmDoDialog(form);
			FrmDeleteForm(form);

			handled = true;
			break;

		default:
			break;
	}

	return handled;
}

static Boolean MainFormHandleEvent(EventType* event) {
	Boolean 	handled = false;
	FormType 	*form;

	switch (event->eType) {
		case frmOpenEvent:
			form = FrmGetActiveForm();
			FrmDrawForm(form);
			FrmSetFocus(form, FrmGetObjectIndex(form, MainNameField));
			handled = true;
			break;
		case ctlSelectEvent:
			switch (event->data.ctlSelect.controlID) {
				case MainHelloButton:
					SaySomething(HelloAlert);
					handled = true;
					break;
				case MainGoodbyeButton:
					SaySomething(GoodbyeAlert);
					handled = true;
					break;

				default:
					break;
			}
			break;
		case menuEvent:
			handled = MainMenuHandleEvent(event->data.menu.itemID);
			break;
		
		default:
			break;
	}

	return handled;
}

static Boolean AppHandleEvent(EventType* event) {
	FormType 	*form;
	UInt16		formID;
	Boolean 	handled = false;

	if (event -> eType == frmLoadEvent) {
		formID = event->data.frmLoad.formID;
		form = FrmInitForm(formID);
		FrmSetActiveForm(form);

		switch(formID) {
			case MainForm:
				FrmSetEventHandler(form, MainFormHandleEvent);
				break;

			// case AboutForm:
			// 	FrmSetEventHandler(form, AboutFormEventHandler);
			// 	break;

			default:
				break;
		}

		handled = true;
	}

	return handled;
}

static void AppEventLoop(void)
{
	Err error;
	EventType event;

	do
	{
		/* change timeout if you need periodic nilEvents */
		EvtGetEvent(&event, evtWaitForever);

		if (! SysHandleEvent(&event))
		{
			if (! MenuHandleEvent(0, &event, &error))
			{
				if (! AppHandleEvent(&event))
				{
					FrmDispatchEvent(&event);
				}
			}
		}
	} while (event.eType != appStopEvent);
}

static void MakeSharedVariables(void)
{
	HeffalumpState *sharedVars;

	sharedVars = (HeffalumpState *)MemPtrNew(sizeof(HeffalumpState));
	ErrFatalDisplayIf(!sharedVars, "Failed to allocate memory for shared variables");
	MemSet(sharedVars, sizeof(HeffalumpState), 0);
	sharedVars->current_toot = 69;

	*globalsSlotPtr(GLOBALS_SLOT_SHARED_VARS) = sharedVars;
}

static void FreeSharedVariables(void)
{
	DmOpenRef author = globalsSlotVal(GLOBALS_SLOT_AUTHOR_DB);
	DmOpenRef content = globalsSlotVal(GLOBALS_SLOT_CONTENT_DB);

	*globalsSlotPtr(GLOBALS_SLOT_AUTHOR_DB) = NULL;
	*globalsSlotPtr(GLOBALS_SLOT_CONTENT_DB) = NULL;

	if (author) {
		DmCloseDatabase(author);

	}
	if (content) {
		DmCloseDatabase(content);
	}

	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables already null");
	ErrFatalDisplayIf(MemPtrSize(sharedVarsP) != sizeof(HeffalumpState), "shared variables wrong size");
	ErrFatalDisplayIf(MemPtrFree(sharedVarsP)!=0, "error while freeing shared variables");
	*globalsSlotPtr(GLOBALS_SLOT_SHARED_VARS) = NULL;
}

static Err AppStart(void) {
	Err e = errNone;
	#ifdef HEFFALUMP_NO_DB_DEV
	ErrFatalDisplayIf(MemSetDebugMode(memDebugModeCheckOnAll) != 0, "failed to set mem debug mode");
	#endif

	MakeSharedVariables();
	HeffalumpState* state = (HeffalumpState*)(globalsSlotVal(GLOBALS_SLOT_SHARED_VARS));
	ErrFatalDisplayIf(!state, "Shared variables could not be loaded");
	// ErrFatalDisplayIf (state->current_toot != 69, "Bob");

	DmOpenRef author;
	DmOpenRef content;

	author = DmOpenDatabaseByTypeCreator(tootAuthorDBType, heffCreatorID, dmModeReadWrite);
	if (!author) {
		e = DmCreateDatabase(0, tootAuthorDBName, heffCreatorID, tootAuthorDBType, false);
		if (e) {return e;}

		author = DmOpenDatabaseByTypeCreator(tootAuthorDBType, heffCreatorID, dmModeReadWrite);
		ErrFatalDisplayIf(author == NULL, "Failed to open author DB");
	}
	*globalsSlotPtr(GLOBALS_SLOT_AUTHOR_DB) = author;


	content = DmOpenDatabaseByTypeCreator(tootContentDBType, heffCreatorID, dmModeReadWrite);
	if (!content) {
		e = DmCreateDatabase(0, tootContentDBName, heffCreatorID, tootContentDBType, false);
		if (e) {return e;}

		content = DmOpenDatabaseByTypeCreator(tootContentDBType, heffCreatorID, dmModeReadWrite);
		ErrFatalDisplayIf(content == NULL, "Failed to open content DB");
	}
	*globalsSlotPtr(GLOBALS_SLOT_CONTENT_DB) = content;

	

	#ifdef HEFFALUMP_NO_DB_DEV
	// seed the random number generator 
	SysRandom(1);

	if (DmNumRecords(author) == 0) { // add author info for test toot
		FrmCustomAlert(DebugAlert1, "Generating random toot author", NULL, NULL);
		TootAuthor* sample_toot_author = RandomTootAuthor();
		FrmCustomAlert(DebugAlert2, "Finished generating random toot author", (const char*)&(sample_toot_author->author_name), NULL);
		// TootAuthor* offsetRecord = 0;

		MemHandle newRecordH = DmNewRecord(author, 0, (UInt32)(sizeof(UInt8) + sample_toot_author->author_name_len));
		if ((UInt32)newRecordH <1000) {
			return DmGetLastErr();
		}
		
		
		// TootAuthor* authorRecord = MemHandleLock(newRecordH);
		// ErrFatalDisplayIf (authorRecord == NULL, "Unable to lock mem handle");
		//  char* authorRecordAddress = "                                 \0";
		//  StrIToH(authorRecordAddress, (UInt32)authorRecord);
    	// FrmCustomAlert(DebugAlert2, "Toot Author ptr is ", authorRecordAddress, NULL);

		// // write the name length
		// ErrFatalDisplayIf (sample_toot_author->author_name_len == 0, "Author record has zero len");
		// DmWrite(
		// 	authorRecord,
		// 	0, 
		// 	&sample_toot_author,
		// 	(UInt32)sizeof(UInt8)
		// );
		
		// // write the name
		// DmWrite(
		// 	authorRecord, 
		// 	(UInt32) &(offsetRecord->author_name), 
		// 	&(sample_toot_author->author_name), 
		// 	(UInt32) sample_toot_author->author_name_len
		// );
		

		// ErrFatalDisplayIf(MemHandleUnlock(newRecordH), "error while freeing memory");
		DmReleaseRecord(author ,0, true);
		ErrFatalDisplayIf(MemPtrFree(sample_toot_author)!=0, "error while freeing memory");
	}

	if (DmNumRecords(content) == 0) { // add content for test toot
		// TootContent* sample_toot = RandomTootContent(1);
		// TootContent* offsetRecord = 0;
		// ErrFatalDisplayIf (sample_toot->content_len < 10, "Random toot contents has <10 len");
		
		// UInt16 record_number = 0;
		// MemHandle newRecordH = DmNewRecord(content, &record_number, sizeof(TootContent) + sample_toot->content_len - sizeof(char));
		// ErrFatalDisplayIf (newRecordH == NULL, "Unable create record");

		// TootContent* lockedTootHandle = MemHandleLock(newRecordH);
		// ErrFatalDisplayIf (lockedTootHandle == NULL, "Unable to lock mem handle");

		// DmWrite(
		// 	lockedTootHandle, 
		// 	(UInt32) (&offsetRecord->author), 
		// 	sample_toot, 
		// 	sizeof(TootContent) - sizeof(char)
		// );
		

		// DmWrite(
		// 	lockedTootHandle, 
		// 	(UInt32) &offsetRecord->toot_content, 
		// 	&(sample_toot->toot_content), 
		// 	(UInt32) sample_toot->content_len
		// );

		// ErrFatalDisplayIf (lockedTootHandle->content_len == 0, "Content record has zero len");

		// ErrFatalDisplayIf(MemHandleUnlock(newRecordH), "error while freeing memory");
		// DmReleaseRecord(content, record_number, true);
		// ErrFatalDisplayIf(MemPtrFree(sample_toot)!=0, "error while freeing memory");

		// FrmCustomAlert(DebugAlert1, "Toot Content Added", NULL, NULL);

	}  

	{ // test that we've actually loaded things in the DB
		// ErrFatalDisplayIf (DmNumRecords(author) == 0, "No Author Records");
		// {
		// 	MemHandle tmp = DmGetRecord(author, 0);
		// 	ErrFatalDisplayIf (tmp == NULL, "Failed to retrieve record");
		// 	TootAuthor* author_inst = MemHandleLock(tmp);
		// 	ErrFatalDisplayIf(author_inst == NULL, "error while locking memory");
		// 	ErrFatalDisplayIf (author_inst->author_name_len == 0, "Author record has zero len");
		// 	// char* test_author_name = (char*) MemPtrNew(author_inst->author_name_len);
		// 	// MemSet(test_author_name, author_inst->author_name_len, 0);
		// 	// StrNCopy(test_author_name, &(author_inst->author_name), author_inst->author_name_len -1);
		// 	// FrmCustomAlert(DebugAlert1, test_author_name, NULL, NULL);
		// 	// MemPtrFree(test_author_name);
		// 	ErrFatalDisplayIf(MemHandleUnlock(tmp), "error while freeing memory");
		// 	DmReleaseRecord(author, 0, false);
		// }
		// ErrFatalDisplayIf (DmNumRecords(content) == 0, "No Content Records");
		{
			// MemHandle tmp = DmGetRecord(content, 0);
			// ErrFatalDisplayIf (tmp == NULL, "Failed to retrieve record");
			// TootContent* content_inst = (TootContent*) MemHandleLock(tmp);
			// ErrFatalDisplayIf (content_inst == NULL, "Failed to lock record");
			// ErrFatalDisplayIf (content_inst->content_len == 0, "Content record has zero len");
			// // char* content_str = (char*) MemPtrNew(10);
			// // MemSet(content_str, 10, 0);
			// // StrNCopy(content_str, &(content_inst->toot_content), 9);
			// // FrmCustomAlert(DebugAlert1, content_str, NULL, NULL);
			// // MemPtrFree(content_str);
			// ErrFatalDisplayIf(MemHandleUnlock(tmp), "error while freeing memory");
			// DmReleaseRecord(content, 0, false);
		}
	}
	#endif


	return e;
}

// TootContent GetTootContent(UInt16 idx) {
	
// }

static void AppStop(void) {

	FreeSharedVariables();
	FrmCloseAllForms();
}

UInt32 PilotMain(UInt16 cmd, void *cmdPBP, UInt16 launchFlags) {
	EventType event;
	char *message1 = "This application was built only";
	char *message2 = "with gcc 9.1 and PilRC 3.2";
	char *message3 = "on Ubuntu 20.04!";

	if (sysAppLaunchCmdNormalLaunch == cmd) {
		if (AppStart()) {
			return 1;
		}  

		FrmGotoForm(MainForm);

		AppEventLoop();

		AppStop();
	}

	return 0;
}

UInt32 __attribute__((section(".vectors"))) __Startup__(void)
{
	SysAppInfoPtr appInfoP;
	void *prevGlobalsP;
	void *globalsP;
	UInt32 ret;
	
	SysAppStartup(&appInfoP, &prevGlobalsP, &globalsP);
	ret = PilotMain(appInfoP->cmd, appInfoP->cmdPBP, appInfoP->launchFlags);
	SysAppExit(appInfoP, prevGlobalsP, globalsP);
	
	return ret;
}