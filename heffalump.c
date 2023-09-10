#define ERROR_CHECK_FULL
#include "heffalumpRsc.h"
#include "heffalumpTypes.h"
#include "heffalumpStatics.h"
#include <PalmOS.h>
#include <stddef.h>


// #define HEFFALUMP_NO_DB_DEV
#ifdef HEFFALUMP_NO_DB_DEV
#include "heffalumpTestRsc.h"
#endif

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

static void LoadTootToGlobals(HeffalumpState* sharedVarsP, UInt16 idx) {
	DmOpenRef content = globalsSlotVal(GLOBALS_SLOT_CONTENT_DB);
	DmOpenRef author = globalsSlotVal(GLOBALS_SLOT_AUTHOR_DB);
	ErrFatalDisplayIf (!sharedVarsP || !author || !content, "Invalid globals state");
	ErrFatalDisplayIf (!DmNumRecords(content) || !DmNumRecords(author), "No toots to read");

	// attempt to lock new content, if it's not available, do nothing and return
	if (idx >= DmNumRecords(content)) {
		return;
	}
	MemHandle content_handle = DmGetRecord(content, idx);
	if (!content_handle) {
		return;
	}
	TootContent* content_inst = (TootContent*) MemHandleLock(content_handle);
	if (!content_inst) {
		return;
	}

	// clear current author
	if (sharedVarsP->current_toot_author_ptr) {
		ErrFatalDisplayIf(MemHandleUnlock((MemHandle)sharedVarsP->current_toot_author_handle), "error while freeing memory");
		DmReleaseRecord(author, sharedVarsP->current_toot_author_record, false);
	}

	// clear current content
	if (sharedVarsP->current_toot_content_ptr) {
		ErrFatalDisplayIf(MemHandleUnlock(sharedVarsP->current_toot_content_handle), "error while freeing memory");
		DmReleaseRecord(content, sharedVarsP->current_toot_content_record, false);
	}

	// load new content
	sharedVarsP->current_toot_content_record = idx;
	sharedVarsP->current_toot_content_ptr = content_inst;
	sharedVarsP->current_toot_content_handle = content_handle;

	// load new author
	MemHandle author_handle = DmGetRecord(author, content_inst->author);
	if (!author_handle) {
		ErrFatalDisplayIf(!author_handle, "Failed to load toot author");
	}
	TootAuthor* author_inst = (TootAuthor*) MemHandleLock(author_handle);
	ErrFatalDisplayIf(!author_inst, "Failed to load toot author");
	sharedVarsP->current_toot_author_record = content_inst->author;
	sharedVarsP->current_toot_author_ptr = author_inst;
	sharedVarsP->current_toot_author_handle = author_handle;
	
}

static void changeToot(FormType* form, FieldType* content, Boolean next, Boolean initial) {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");

	UInt16 NewToot = sharedVarsP->current_toot_content_record;
	if (next) {
		if (sharedVarsP->current_toot_content_record + 1 != 0) {
			NewToot = sharedVarsP->current_toot_content_record + 1;
		}
	} else {
		if (sharedVarsP->current_toot_content_record != 0) {
			NewToot = sharedVarsP->current_toot_content_record - 1;
		}
	}
	if (sharedVarsP->current_toot_content_record != NewToot || initial) {
		LoadTootToGlobals(sharedVarsP, NewToot);
		if (NewToot == sharedVarsP->current_toot_content_record && form != NULL) {
			
			FrmCopyLabel(form, MainAuthorLabel, "                                        "); // blank out the label
			FrmCopyLabel(form, MainAuthorLabel, sharedVarsP->current_toot_author_ptr->author_name);

			MemHandle content_handle = FldGetTextHandle(content);
			FldSetTextHandle(content, NULL);

			if(!content_handle) {
				content_handle = MemHandleNew(sharedVarsP->current_toot_content_ptr->content_len);
			} else {
				ErrFatalDisplayIf(
					MemHandleResize(content_handle, sharedVarsP->current_toot_content_ptr->content_len) != 0,
					"failed to resize content handle"
				);
			}
						
			char* content_str = (char*) MemHandleLock(content_handle);
			MemSet(content_str, sharedVarsP->current_toot_content_ptr->content_len, 0);
			ErrFatalDisplayIf(!content_str, "failed to lock handle");
			StrNCopy(
				content_str, 
				(const char*)&(sharedVarsP->current_toot_content_ptr->toot_content), 
				sharedVarsP->current_toot_content_ptr->content_len -1
			);
			MemPtrUnlock(content_str);
			FldSetTextHandle(content, content_handle);
			FldDrawField(content);
		}
	}
}

static Boolean MainMenuHandleEvent(UInt16 menuID) {
	Boolean 	handled = false;
	FormType 	*form;
	// FieldType	*field;

	form = FrmGetActiveForm();
	// field = FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField));

	switch (menuID) {
		// case EditUndo:
		// case EditCut:
		// case EditCopy:
		// case EditPaste:
		// case EditSelectAll:
		// case EditKeyboard:
		// 	handled = HandleEditOptions(menuID, field);
		// 	break;

		// #ifdef PALMOS_GE_V_2
		// case EditGrafittiHelp:
		// 	SysGraffitiReferenceDialog(referenceDefault);
		// 	handled = true;
		// 	break;
		// #endif
		
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
			changeToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)), false, true);
			handled = true;
			break;
		case frmCloseEvent:
			form = FrmGetActiveForm();
			FieldType* content_field = FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField));
			MemHandle content_handle = FldGetTextHandle(content_field);
			FldSetTextHandle(content_field, NULL);
			ErrNonFatalDisplayIf(!MemHandleFree(content_handle), "failed to free handle")
			
			handled = true;
			break;
		case keyDownEvent:
			form = FrmGetActiveForm();
			switch (event->data.keyDown.chr) {
				case vchrPageDown:
					if (form != NULL) {
						changeToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)), true, false);
					}
					break;
				case vchrPageUp:
					if (form != NULL) {
						changeToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)), false, false);
					}					
					break;
				default: 
					break;
			}
			handled = true;
			break;
		case ctlSelectEvent:
			switch (event->data.ctlSelect.controlID) {
				case MainLikeButton:
					handled = true;
					break;
				case MainReplyButton:
					handled = true;
					break;
				case MainRepostButton:
					handled = true;
					break;
				case MainPrevButton:
					form = FrmGetActiveForm();
					changeToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)), false, false);
					handled = true;
					break;
				case MainNextButton:
					form = FrmGetActiveForm();
					changeToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)), true, false);
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

	*globalsSlotPtr(GLOBALS_SLOT_SHARED_VARS) = sharedVars;
	*globalsSlotPtr(GLOBALS_SLOT_AUTHOR_DB) = NULL;
	*globalsSlotPtr(GLOBALS_SLOT_CONTENT_DB) = NULL;
}

static void FreeSharedVariables(void)
{
	DmOpenRef author = globalsSlotVal(GLOBALS_SLOT_AUTHOR_DB);
	DmOpenRef content = globalsSlotVal(GLOBALS_SLOT_CONTENT_DB);

	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables already null");
	if (sharedVarsP->current_toot_author_handle != NULL) {
		sharedVarsP->current_toot_author_ptr = NULL;
		ErrFatalDisplayIf(MemHandleUnlock(sharedVarsP->current_toot_author_handle)!=0, "error while freeing toot author handle");
		DmReleaseRecord(author, sharedVarsP->current_toot_author_record, false);
	}
	if (sharedVarsP->current_toot_content_handle != NULL) {
		sharedVarsP->current_toot_content_ptr = NULL;
		ErrFatalDisplayIf(MemHandleUnlock(sharedVarsP->current_toot_content_handle)!=0, "error while freeing toot content handle");
		DmReleaseRecord(content, sharedVarsP->current_toot_content_record, false);
	}

	ErrFatalDisplayIf(MemPtrFree(sharedVarsP)!=0, "error while freeing shared variables");
	*globalsSlotPtr(GLOBALS_SLOT_SHARED_VARS) = NULL;

	*globalsSlotPtr(GLOBALS_SLOT_AUTHOR_DB) = NULL;
	*globalsSlotPtr(GLOBALS_SLOT_CONTENT_DB) = NULL;

	if (author) {
		ErrFatalDisplayIf(DmCloseDatabase(author) != errNone, "error closeing db");
	}
	if (content) {
		ErrFatalDisplayIf(DmCloseDatabase(content) != errNone, "error closeing db");
	}
	
}

static void AppStop(void) {
	FreeSharedVariables();

	FrmCloseAllForms();
}

static Err AppStart(void) {
	Err e = errNone;

	MakeSharedVariables();
	HeffalumpState* state = (HeffalumpState*)(globalsSlotVal(GLOBALS_SLOT_SHARED_VARS));
	ErrFatalDisplayIf(!state, "Shared variables could not be loaded");

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
	if (DmNumRecords(author) == 0 && DmNumRecords(content) == 0) { // add info for test toots
		// seed the random number generator 
		SysRandom(1);
		UInt16 toot_count = 2;
		

		for (UInt16 toot_index = 0; toot_index<toot_count;toot_index++) {
			//test author
			{
				TootAuthor* sample_toot_author = RandomTootAuthor();
				ErrFatalDisplayIf (sample_toot_author == NULL, "failed to get toot_author");

				UInt32 toot_author_struct_bytes = sizeof(TootAuthor) + (UInt32)(sample_toot_author->author_name_len) + sizeof(char);
				UInt16 index = toot_index;

				MemHandle newRecordH = DmNewRecord(author, &index, toot_author_struct_bytes);
				if (index != toot_index) {
					FrmCustomAlert(DebugAlert1, "Record not at expected index", NULL, NULL);
				}
				if (newRecordH == NULL) {
					return DmGetLastErr();
				}
				
				TootAuthor* authorRecord = MemHandleLock(newRecordH);
				ErrFatalDisplayIf (authorRecord == NULL, "Unable to lock mem handle");

				DmWrite(
					authorRecord,
					0, 
					sample_toot_author,
					toot_author_struct_bytes
				);

				ErrFatalDisplayIf(MemHandleUnlock(newRecordH), "error while freeing memory");
				DmReleaseRecord(author, 0, true);
				ErrFatalDisplayIf(MemPtrFree(sample_toot_author)!=0, "error while freeing memory");
			}

			// test content
			{
				TootContent* sample_toot = RandomTootContent(1);
				ErrFatalDisplayIf (sample_toot->content_len < 10, "Random toot contents has <10 len");
				
				UInt16 record_number = toot_index;
				MemHandle newRecordH = DmNewRecord(content, &record_number, sizeof(TootContent) + sample_toot->content_len + sizeof(char));
				ErrFatalDisplayIf (newRecordH == NULL, "Unable create record");

				TootContent* lockedTootHandle = MemHandleLock(newRecordH);
				ErrFatalDisplayIf (lockedTootHandle == NULL, "Unable to lock mem handle");

				DmWrite(
					lockedTootHandle, 
					0, 
					sample_toot, 
					sizeof(*sample_toot) +(UInt32) sample_toot->content_len + sizeof(char)
				);
			
				ErrFatalDisplayIf (lockedTootHandle->content_len == 0, "Content record has zero len");

				ErrFatalDisplayIf(MemHandleUnlock(newRecordH), "error while freeing memory");
				DmReleaseRecord(content, record_number, true);
				ErrFatalDisplayIf(MemPtrFree(sample_toot)!=0, "error while freeing memory");
			}
		}
		FrmCustomAlert(DebugAlert1, "Added Examples Toots", NULL, NULL);
	}

	{ // test that we've actually loaded things in the DB
		ErrFatalDisplayIf (DmNumRecords(author) == 0, "No Author Records");
		{
			MemHandle tmp = DmGetRecord(author, 0);
			ErrFatalDisplayIf (tmp == NULL, "Failed to retrieve record");
			TootAuthor* author_inst = MemHandleLock(tmp);
			ErrFatalDisplayIf(author_inst == NULL, "error while locking memory");
			ErrFatalDisplayIf (author_inst->author_name_len == 0, "Author record has zero len");
			char* test_author_name = (char*) MemPtrNew(author_inst->author_name_len);
			MemSet(test_author_name, author_inst->author_name_len, 0);
			StrNCopy(test_author_name, (const char*)&(author_inst->author_name), author_inst->author_name_len -1);
			MemPtrFree(test_author_name);
			ErrFatalDisplayIf(MemHandleUnlock(tmp), "error while freeing memory");
			DmReleaseRecord(author, 0, false);
		}
		ErrFatalDisplayIf (DmNumRecords(content) == 0, "No Content Records");
		{
			MemHandle tmp = DmGetRecord(content, 0);
			ErrFatalDisplayIf (tmp == NULL, "Failed to retrieve record");
			TootContent* content_inst = (TootContent*) MemHandleLock(tmp);
			ErrFatalDisplayIf (content_inst == NULL, "Failed to lock record");
			ErrFatalDisplayIf (content_inst->content_len == 0, "Content record has zero len");
			char* content_str = (char*) MemPtrNew(10);
			MemSet(content_str, 10, 0);
			StrNCopy(content_str,  (const char*)&(content_inst->toot_content), 9);
			MemPtrFree(content_str);
			ErrFatalDisplayIf(MemHandleUnlock(tmp), "error while freeing memory");
			DmReleaseRecord(content, 0, false);
		}
	}
	#endif // HEFFALUMP_NO_DB_DEV

	return e;
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