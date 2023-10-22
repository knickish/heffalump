// #define ERROR_CHECK_FULL
#include "heffalumpRsc.h"
#include "heffalumpTypes.h"
#include "heffalumpStatics.h"
#include <PalmOS.h>
#include <PalmOSGlue.h>
#include <stddef.h>

// #define HEFFALUMP_TESTING
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

static TootContent* TootContentConstructor(UInt16 length) {
	int size = sizeof(TootContent) + length + sizeof(char);
	TootContent* ret = (TootContent*) MemPtrNew(size);
	MemSet(ret, size, 0);
	ret->content_len = length;
	return ret;
}

static Boolean IsSpace(char c) {
	return c == '\t' || c == ' ';
}

static CharOffsets GetCharOffsetsOfPage(UInt8 page_number, int rows, int available_width) {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");
	int pagelen = rows * available_width;

	CharOffsets offsets = {0};
	int page = 0;
	int char_offset = 0;
	do {
		{ // accumulate the actual width of each character
			
			for (int row = 0; row < rows; row++) {  // row
				int used_width = 0;
				int begin_line_offset = char_offset;
				int current_word_size = 0;
				int current_word_width = 0;
				for (;char_offset < (int)sharedVarsP->current_toot_content_ptr->content_len; char_offset++) {
					char c = sharedVarsP->current_toot_content_ptr->toot_content[char_offset];
					int next_char_width = FntCharWidth(c);

					if (IsSpace(c)) {
						current_word_size = 0;
						current_word_width = 0;
					}
					
					if (c == '\n') {
						char_offset++;
						used_width += (available_width - used_width); // we lose all of the extra space here
						break;
					}
					
					if (used_width + next_char_width >= available_width) {
						if (current_word_width + next_char_width >= available_width) {
							// we're not making progress, time to split the word
						} else {
							// we're in the middle of a word, go back and split the line at the start of the word
							char_offset -= current_word_size;
						}						
						

						break;
					}
					used_width += next_char_width;
					if (!IsSpace(c)) {
						current_word_size++;
						current_word_width+= next_char_width;
					}
				}
			}

			if (page == (int)page_number) {
				offsets.end = char_offset;
			} else {
				offsets.start = char_offset;
			}

			{ //debug
				// char* debug = MemPtrNew(150);
				// MemSet(debug, 150, 0);
				// StrPrintF(debug, "page boundary: %d, start: %d, end: %d", page, offsets.start, offsets.end);
				// FrmCustomAlert(DebugAlert1, debug, NULL, NULL);
				// MemPtrFree(debug);
			}

		}
		

		page++;
	} while(page <= (int)page_number && (offsets.end +1) < (int)sharedVarsP->current_toot_content_ptr->content_len);
	return offsets;
}

static void ChangeToot(UInt16 requested) {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");

	if (
		(
			sharedVarsP->current_toot_content_record != requested
			|| sharedVarsP->current_toot_content_handle == NULL
		) 
		&& requested < sharedVarsP->toot_content_record_count
	) 
	{
		DmOpenRef content = globalsSlotVal(GLOBALS_SLOT_CONTENT_DB);
		DmOpenRef author = globalsSlotVal(GLOBALS_SLOT_AUTHOR_DB);
		ErrFatalDisplayIf (!sharedVarsP || !author || !content, "Invalid globals state");
		ErrFatalDisplayIf (!DmNumRecords(content) || !DmNumRecords(author), "No toots to read");

		// attempt to lock new content, if it's not available, do nothing and return
		MemHandle content_handle = DmGetRecord(content, requested);
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
		sharedVarsP->current_toot_content_record = requested;
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
}

static void LoadTimelineToot(FormType* form, FieldType* content) {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");

	SetLabelInForm(form, MainAuthorLabel, sharedVarsP->current_toot_author_ptr->author_name);

	MemHandle content_handle = FldGetTextHandle(content);
	FldSetTextHandle(content, NULL);
	
	CharOffsets indices = GetCharOffsetsOfPage(0, 10, 153);
	int end_char_index = indices.end;
	if (end_char_index == (int)sharedVarsP->current_toot_content_ptr->content_len) {
		HideButtonInForm(form, MainExpandButton);
	} else {
		ShowButtonInForm(form, MainExpandButton);
	}

	if (sharedVarsP->current_toot_content_record == 0) {
		HideButtonInForm(form, MainPrevButton);
	} else {
		ShowButtonInForm(form, MainPrevButton);
	}

	if(!content_handle) {
		content_handle = MemHandleNew(end_char_index + 1);
	} else {
		ErrFatalDisplayIf(
			MemHandleResize(content_handle, end_char_index + 1) != 0,
			"failed to resize content handle"
		);
	}

	char* content_str = (char*) MemHandleLock(content_handle);
	MemSet(content_str, end_char_index+1, 0);
	ErrFatalDisplayIf(!content_str, "failed to lock handle");
	{
		const char* toot_content_ptr = sharedVarsP->current_toot_content_ptr->toot_content;
		StrNCopy(
			content_str, 
			toot_content_ptr, 
			end_char_index
		);
	}
	MemPtrUnlock(content_str);
	FldSetTextHandle(content, content_handle);
	FldDrawField(content);
}

static void LoadExpandedToot(FormType* form) {
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");

	FieldType* content = FrmGetObjectPtr(form, FrmGetObjectIndex(form, ExpandTootContentField));

	SetLabelInForm(form, ExpandTootAuthorLabel, sharedVarsP->current_toot_author_ptr->author_name);

	CharOffsets indices = GetCharOffsetsOfPage(sharedVarsP->current_toot_page_index, 10, 154);
	int end_char_index = indices.end;
	int start_char_offset = indices.start;
	if (
		sharedVarsP->current_toot_page_index > 0
		&& (
			(start_char_offset >= (int)sharedVarsP->current_toot_content_ptr->content_len)
			|| (start_char_offset == end_char_index)
			)
		) {
		sharedVarsP->current_toot_page_index -= 1;
		indices = GetCharOffsetsOfPage(sharedVarsP->current_toot_page_index, 10, 156);
		end_char_index = indices.end;
		start_char_offset = indices.start;
	}

	int to_load_size = end_char_index - start_char_offset;

	MemHandle content_handle = FldGetTextHandle(content);
	FldSetTextHandle(content, NULL);
	if(!content_handle) {
		// FrmCustomAlert(DebugAlert1, "allocating main form handle", NULL, NULL);
		content_handle = MemHandleNew(to_load_size + 1);
	} else {
		ErrFatalDisplayIf(
			MemHandleResize(content_handle, to_load_size + 1) != 0,
			"failed to resize content handle"
		);
	}
				
	char* content_str = (char*) MemHandleLock(content_handle);
	// FrmCustomAlert(DebugAlert1, "memsetting main form handle", NULL, NULL);

	MemSet(content_str, to_load_size +1, 0);
	ErrFatalDisplayIf(!content_str, "failed to lock handle");
	// FrmCustomAlert(DebugAlert1, "strncopy main form handle", NULL, NULL);
	{
		char* toot_content_ptr = sharedVarsP->current_toot_content_ptr->toot_content;
		StrNCopy(
			content_str, 
			(const char*)(&(toot_content_ptr[start_char_offset])), 
			to_load_size
		);
	}
	
	MemPtrUnlock(content_str);
	FldSetTextHandle(content, content_handle);
	FldDrawField(content);
}

static void MoonWriteAction(HeffalumpState* sharedVarsP, TootWriteType action , TootContent* content) {
	DmOpenRef writes = globalsSlotVal(GLOBALS_SLOT_WRITES_DB);
	if (!writes) return;
	if (!sharedVarsP) return;
	UInt16 size;
	TootWrite* to_write = NULL;

	switch (action) {
		case 0: // favorite
			// FrmCustomAlert(DebugAlert1, "Favoriting Toot", NULL, NULL);
			size =  sizeof(UInt16)+sizeof(UInt16);
			to_write = (TootWrite*) MemPtrNew(size);
			ErrFatalDisplayIf(!to_write, "failed to allocate handle");
			// FrmCustomAlert(DebugAlert1, "writing to type", NULL, NULL);
			to_write->type = (UInt16) action;
			// FrmCustomAlert(DebugAlert1, "writing to content (favorite)", NULL, NULL);
			to_write->content.favorite = sharedVarsP->current_toot_content_record;
			break;
		case 1: // reblog
			size = sizeof(UInt16)+sizeof(UInt16);
			to_write = (TootWrite*) MemPtrNew(size);
			ErrFatalDisplayIf(!to_write, "failed to allocate handle");
			to_write->type = action;
			to_write->content.reblog = sharedVarsP->current_toot_content_record;
			break;
		case 2: // follow
			size = sizeof(UInt16)+sizeof(UInt16);
			to_write = (TootWrite*) MemPtrNew(size);
			ErrFatalDisplayIf(!to_write, "failed to allocate handle");
			to_write->type = (UInt16) action;
			to_write->content.follow = sharedVarsP->current_toot_author_record;
			break;
		case 3: // toot
			if (!content) {
				#ifdef HEFFALUMP_TESTING
				ErrNonFatalDisplay("content is nullptr");
				#endif
				return;
			}
			size = sizeof(UInt16)+ sizeof(TootContent) + content->content_len + sizeof(char);
			to_write = (TootWrite*) MemPtrNew(size);
			MemSet(to_write, size, 0);
			ErrFatalDisplayIf(!to_write, "failed to allocate handle");
			to_write->type = (UInt16) action;
			MemMove(&(to_write->content.toot), (const void *) content, size - sizeof(UInt16));
			break;
		default:
			return;
	}
	
	// FrmCustomAlert(DebugAlert1, "allocating record", NULL, NULL);
	UInt16 record_number = 0;
	MemHandle newRecordH = DmNewRecord(writes, &record_number, size);
	if (newRecordH == NULL) {
		return;
	}
	
	// FrmCustomAlert(DebugAlert1, "locking record", NULL, NULL);
	TootWrite* writeRecord = MemHandleLock(newRecordH);
	ErrFatalDisplayIf (writeRecord == NULL, "Unable to lock mem handle");

	// FrmCustomAlert(DebugAlert1, "writing to record", NULL, NULL);
	DmWrite(
		writeRecord,
		0,
		to_write,
		size
	);

	ErrFatalDisplayIf(MemHandleUnlock(newRecordH), "error while freeing memory");
	// FrmCustomAlert(DebugAlert1, "releasing record", NULL, NULL);
	DmReleaseRecord(writes, record_number, true);
	ErrFatalDisplayIf(MemPtrFree(to_write)!=0, "error while freeing memory");
	// FrmCustomAlert(DebugAlert1, "done writing", NULL, NULL);
}

static Boolean MainMenuHandleEvent(UInt16 menuID) {
	Boolean 	handled = false;
	FormType 	*form;
	FieldType	*field;

	form = FrmGetActiveForm();
	field = FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField));

	switch (menuID) {
		// case EditUndo:
		// case EditCut:
		case EditCopy:
		// case EditPaste:
		case EditSelectAll:
		// case EditKeyboard:
			handled = HandleEditOptions(menuID, field);
			break;

		// #ifdef PALMOS_GE_V_2
		// case EditGrafittiHelp:
		// 	SysGraffitiReferenceDialog(referenceDefault);
		// 	handled = true;
		// 	break;
		// #endif
		
		case OptionsAboutHeffalump:
			MenuEraseStatus(0);
			form = FrmInitForm(AboutForm);
			FrmDoDialog(form);
			FrmDeleteForm(form);

			handled = true;
			break;

		case OptionsComposeToot:
			MenuEraseStatus(0);
			FrmGotoForm(ComposeTootForm);
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
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");

	switch (event->eType) {
		case frmOpenEvent:
			form = FrmGetActiveForm();
			FrmDrawForm(form);
			LoadTimelineToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)));
			handled = true;
			break;
		case frmTitleEnterEvent:
			// todo: Create author page
			// triggered by penDownEvent within the title
			handled = true;
			break;
		case fldEnterEvent:
			// triggered by penDownEvent within the field
			if (event->data.fldEnter.fieldID != MainContentField) {
				ErrNonFatalDisplay("Unexpected event received: fldEnter for unknown field");
				handled = true;
				break;
			}
			FrmGotoForm(ExpandTootForm);
			handled = true;
			break;
		case frmCloseEvent:
			break;
		case keyDownEvent:
			form = FrmGetActiveForm();
			switch (event->data.keyDown.chr) {
				case vchrPageDown:
					if (form != NULL) {
						ChangeToot(CurrentTootRecordNumber() + 1);
						LoadTimelineToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)));
					}
					break;
				case vchrPageUp:
					if (form != NULL) {
						ChangeToot(SaturatingDecrement(CurrentTootRecordNumber()));
						LoadTimelineToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)));
					}					
					break;
				default: 
					break;
			}
			handled = true;
			break;
		case ctlSelectEvent: // user pressed a soft button
			{
				TootWriteType action;
				switch (event->data.ctlSelect.controlID) {
					case MainLikeButton:
						action = Favorite;
						// FrmCustomAlert(DebugAlert1, "Sending Favorite to Action", NULL, NULL);
						MoonWriteAction(
							globalsSlotVal(GLOBALS_SLOT_SHARED_VARS),
							action,
							NULL
						);
						handled = true;
						break;
					case MainReplyButton:
						sharedVarsP->toot_is_reply_to_current = true;
						FrmGotoForm(ComposeTootForm);
						handled = true;
						break;
					case MainRepostButton:
						action = Reblog;
						MoonWriteAction(
							globalsSlotVal(GLOBALS_SLOT_SHARED_VARS),
							action,
							NULL
						);
						handled = true;
						break;
					case MainPrevButton:
						form = FrmGetActiveForm();
						ChangeToot(SaturatingDecrement(CurrentTootRecordNumber()));
						LoadTimelineToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)));
						handled = true;
						break;
					case MainNextButton:
						form = FrmGetActiveForm();
						ChangeToot(CurrentTootRecordNumber() + 1);
						LoadTimelineToot(form, FrmGetObjectPtr(form, FrmGetObjectIndex(form, MainContentField)));
						handled = true;
						break;
					case MainExpandButton:
						FrmGotoForm(ExpandTootForm);
						handled = true;
						break;
					default:
						break;
				}
				break;
			}
		case menuEvent:
			handled = MainMenuHandleEvent(event->data.menu.itemID);
			break;
		
		default:
			break;
	}

	return handled;
}

static Boolean ExpandTootFormHandleEvent(EventType* event) {
	Boolean 	handled = false;
	FormType 	*form;
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");

	switch (event->eType) {
		case frmOpenEvent:
			form = FrmGetActiveForm();
			LoadExpandedToot(form);
			FrmDrawForm(form);
			break;
		case frmCloseEvent:
			sharedVarsP->current_toot_page_index = 0;
			break;
		case keyDownEvent:
			form = FrmGetActiveForm();
			switch (event->data.keyDown.chr) {
				case vchrPageDown:
					sharedVarsP->current_toot_page_index +=1; 
					if (form != NULL) {
						LoadExpandedToot(form);
					}
					break;
				case vchrPageUp:
					if (sharedVarsP->current_toot_page_index > 0) {
						sharedVarsP->current_toot_page_index -=1; 
					} 
					if (form != NULL) {
						LoadExpandedToot(form);
					}	
					break;
				default: 
					break;
			}
			handled = true;
			break;
		case ctlSelectEvent: // user pressed a soft button
			{
				TootWriteType action;
				switch (event->data.ctlSelect.controlID) {
					case ExpandTootBackButton:
						sharedVarsP->current_toot_page_index = 0;
						FrmGotoForm(MainForm);
						handled = true;
						break;
					case ExpandTootPrevButton:
						form = FrmGetActiveForm();
						if (sharedVarsP->current_toot_page_index > 0) {
							sharedVarsP->current_toot_page_index -=1; 
						} 
						if (form != NULL) {
							LoadExpandedToot(form);
						}
						handled = true;
						break;
					case ExpandTootNextButton:
						form = FrmGetActiveForm();
						sharedVarsP->current_toot_page_index +=1; 
						if (form != NULL) {
							LoadExpandedToot(form);
						}
						handled = true;
						break;

					default:
						break;
				}
				break;
			}
		case menuEvent:
			handled = MainMenuHandleEvent(event->data.menu.itemID);
			break;
		
		default:
			break;
	}

	return handled;
}

static Boolean ComposeTootFormHandleEvent(EventType* event) {
	Boolean 	handled = false;
	FormType 	*form;
	HeffalumpState* sharedVarsP = (HeffalumpState*)globalsSlotVal(GLOBALS_SLOT_SHARED_VARS);
	ErrFatalDisplayIf(!sharedVarsP, "shared variables are null");

	switch (event->eType) {
		case frmOpenEvent:
			form = FrmGetActiveForm();
			FrmDrawForm(form);
			SetLabelInForm(
				form, 
				ComposeTootAuthorLabel,
				sharedVarsP->toot_is_reply_to_current ? sharedVarsP->current_toot_author_ptr->author_name : NULL
			);
			break;
		case frmCloseEvent:
			sharedVarsP->toot_is_reply_to_current = false;
			break;
		case ctlSelectEvent: // user pressed a soft button
			{
				TootWriteType action;
				switch (event->data.ctlSelect.controlID) {
					case ComposeTootSendButton:
						action = Toot;
						form = FrmGetActiveForm();
						FieldType* content = FrmGetObjectPtr(form, FrmGetObjectIndex(form, ComposeTootContentField));
						MemHandle content_handle = FldGetTextHandle(content);
						FldSetTextHandle(content, NULL);
						char* content_str = (char*) MemHandleLock(content_handle);
						TootContent* to_write = TootContentConstructor(StrLen(content_str));
						#ifdef HEFFALUMP_TESTING
						ErrNonFatalDisplayIf(StrLen(content_str) == 0, "Composed toot has zero len");
						#endif
						StrNCopy(to_write->toot_content, (const char*) content_str, StrLen(content_str));
						MoonWriteAction(sharedVarsP, action, to_write);
						MemPtrUnlock(content_str);
						FldSetTextHandle(content, content_handle);
						FrmGotoForm(MainForm);
						handled = true;
						break;
					case ComposeTootCancelButton:
						FrmGotoForm(MainForm);
						handled = true;
						break;

					default:
						break;
				}
				break;
			}
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

			case ExpandTootForm: 
				FrmSetEventHandler(form, ExpandTootFormHandleEvent);
				break;

			case ComposeTootForm:
				FrmSetEventHandler(form, ComposeTootFormHandleEvent);
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
	*globalsSlotPtr(GLOBALS_SLOT_WRITES_DB) = NULL;
}

static void FreeSharedVariables(void)
{
	DmOpenRef author = globalsSlotVal(GLOBALS_SLOT_AUTHOR_DB);
	DmOpenRef content = globalsSlotVal(GLOBALS_SLOT_CONTENT_DB);
	DmOpenRef writes = globalsSlotVal(GLOBALS_SLOT_WRITES_DB);

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
	*globalsSlotPtr(GLOBALS_SLOT_WRITES_DB) = NULL;

	if (author) {
		ErrFatalDisplayIf(DmCloseDatabase(author) != errNone, "error closing db");
	}
	if (content) {
		ErrFatalDisplayIf(DmCloseDatabase(content) != errNone, "error closing db");
	}
	if (writes) {
		ErrFatalDisplayIf(DmCloseDatabase(writes) != errNone, "error closing db");
	}
	
}

static void AppStop(void) {
	FrmCloseAllForms();
	FreeSharedVariables();
}

static Err AppStart(void) {
	Err e = errNone;
	// ErrFatalDisplayIf(MemSetDebugMode(memDebugModeCheckOnAll) != 0, "failed to set mem debug mode");

	MakeSharedVariables();
	HeffalumpState* state = (HeffalumpState*)(globalsSlotVal(GLOBALS_SLOT_SHARED_VARS));
	ErrFatalDisplayIf(!state, "Shared variables could not be loaded");

	DmOpenRef author;
	DmOpenRef content;
	DmOpenRef writes;

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

	writes = DmOpenDatabaseByTypeCreator(tootWritesDBType, heffCreatorID, dmModeReadWrite);
	if (!writes) {
		e = DmCreateDatabase(0, tootWritesDBName, heffCreatorID, tootWritesDBType, false);
		if (e) {return e;}

		writes = DmOpenDatabaseByTypeCreator(tootWritesDBType, heffCreatorID, dmModeReadWrite);
		ErrFatalDisplayIf(writes == NULL, "Failed to open writes DB");
	}
	*globalsSlotPtr(GLOBALS_SLOT_WRITES_DB) = writes;

	ErrNonFatalDisplayIf( DmNumRecords(author) == 0 && DmNumRecords(content) == 0, "No toots loaded, please hotsync again" );

	state->toot_content_record_count = DmNumRecords(content);

	#ifdef HEFFALUMP_TESTING
	LocalID writeDbId = DmFindDatabase(0, tootWritesDBName);
	UInt16 attributes = 0;
	ErrNonFatalDisplayIf(writeDbId == 0, "Failed to find localID for write db");
	ErrNonFatalDisplayIf(errNone != DmDatabaseInfo(0, writeDbId, NULL, &attributes, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), "Failed to rerive db info");
	attributes |= dmHdrAttrBackup;
	ErrNonFatalDisplayIf(errNone != DmSetDatabaseInfo(0, writeDbId, NULL, &attributes, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), "failed to set db info");
	#endif

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

	// load the initial toot to globals
	ChangeToot(0);
	return e;
}

UInt32 PilotMain(UInt16 cmd, void *cmdPBP, UInt16 launchFlags) {
	EventType event;

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