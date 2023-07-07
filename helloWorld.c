#include <PalmOS.h>
#include "helloWorld_Rsc.h"

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

static Err AppStart(void) {
	return errNone;
}

static void AppStop(void) {
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
		// WinDrawChars(message1, StrLen(message1), 19, 71);
		// WinDrawChars(message2, StrLen(message2), 30, 84);
		// WinDrawChars(message3, StrLen(message3), 45, 97);

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