/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2022
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef HW_RVL
#include <di/di.h>
#include <wiiuse/wpad.h>
#endif

#include "vbagx.h"
#include "vbasupport.h"
#include "video.h"
#include "filebrowser.h"
#include "gcunzip.h"
#include "networkop.h"
#include "fileop.h"
#include "preferences.h"
#include "button_mapping.h"
#include "input.h"
#include "filelist.h"
#include "menu.h"
#include "gamesettings.h"
#include "gui/gui.h"
#include "utils/gettext.h"
#include "utils/FreeTypeGX.h"

#define THREAD_SLEEP 100

#ifdef HW_RVL
GuiImageData * pointer[4];
#endif

#ifdef HW_RVL
	#include "mem2.h"

	#define MEM_ALLOC(A) (u8*)mem2_malloc(A)
	#define MEM_DEALLOC(A) mem2_free(A)
#else
	#define MEM_ALLOC(A) (u8*)memalign(32, A)
	#define MEM_DEALLOC(A) free(A)
#endif

static GuiTrigger * trigA = NULL;
static GuiTrigger * trig2 = NULL;

static GuiButton * btnLogo = NULL;
#ifdef HW_RVL
static GuiButton * batteryBtn[4];
#endif
static GuiImageData * gameScreen = NULL;
static GuiImage * gameScreenImg = NULL;
static GuiImage * bgTopImg = NULL;
static GuiImage * bgBottomImg = NULL;
static GuiSound * bgMusic = NULL;
static GuiSound * enterSound = NULL;
static GuiSound * exitSound = NULL;
static GuiWindow * mainWindow = NULL;
static GuiText * settingText = NULL;
static GuiText * settingText2 = NULL;
static int lastMenu = MENU_NONE;
static int mapMenuCtrl = 0;

static lwp_t guithread = LWP_THREAD_NULL;
static lwp_t progressthread = LWP_THREAD_NULL;
static bool guiHalt = true;
static int showProgress = 0;

static char progressTitle[101];
static char progressMsg[201];
static int progressDone = 0;
static int progressTotal = 0;

u8 * bg_music;
u32 bg_music_size;

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void
ResumeGui()
{
	guiHalt = false;
	LWP_ResumeThread (guithread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
static void
HaltGui()
{
	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(THREAD_SLEEP);
}

static void ResetText()
{
	LoadLanguage();

	if(mainWindow)
	{
		HaltGui();
		mainWindow->ResetText();
		ResumeGui();
	}
}

static int currentLanguage = -1;

void ChangeLanguage() {
	if(currentLanguage == GCSettings.language) {
		return;
	}

	if(GCSettings.language == LANG_JAPANESE || GCSettings.language == LANG_KOREAN || GCSettings.language == LANG_SIMP_CHINESE) {
#ifdef HW_RVL
		char filepath[MAXPATHLEN];

		switch(GCSettings.language) {
			case LANG_KOREAN:
				sprintf(filepath, "%s/ko.ttf", appPath);
				break;
			case LANG_JAPANESE:
				sprintf(filepath, "%s/jp.ttf", appPath);
				break;
			case LANG_SIMP_CHINESE:
				sprintf(filepath, "%s/zh.ttf", appPath);
				break;
		}

		size_t fontSize = LoadFont(filepath);

		if(fontSize > 0) {
			HaltGui();
			DeinitFreeType();
			InitFreeType((u8*)ext_font_ttf, fontSize);
		}
		else {
			GCSettings.language = currentLanguage;
		}
#else
	GCSettings.language = currentLanguage;
	ErrorPrompt("Unsupported language!");
#endif
	}
#ifdef HW_RVL
	else {
		if(ext_font_ttf != NULL) {
			HaltGui();
			DeinitFreeType();
			mem2_free(ext_font_ttf);
			ext_font_ttf = NULL;
			InitFreeType((u8*)font_ttf, font_ttf_size);
		}
	}
#endif
	ResetText();
	currentLanguage = GCSettings.language;
}

/****************************************************************************
 * WindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice
 ***************************************************************************/
int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label, bool btn1Default)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
		return 0;

	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_prompt_png);
	GuiImageData btnOutlineOver(button_prompt_over_png);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);
	GuiText msgTxt(msg, 26, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 430);

	GuiText btn1Txt(btn1Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetSoundClick(&btnSoundClick);
	btn1.SetTrigger(trigA);
	btn1.SetTrigger(trig2);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetSoundClick(&btnSoundClick);
	btn2.SetTrigger(trigA);
	btn2.SetTrigger(trig2);
	btn2.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	CancelAction();
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	if(btn2Label)
	{
		if (btn1Default)
		{
			btn2.ResetState();
			btn1.SetState(STATE_SELECTED);
		}
		else
		{
			btn1.ResetState();
			btn2.SetState(STATE_SELECTED);
		}
	}
	ResumeGui();

	while(choice == -1)
	{
		usleep(THREAD_SLEEP);

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(THREAD_SLEEP);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}

int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	return WindowPrompt(title, msg, btn1Label, btn2Label, false);
}

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/
static void *
UpdateGUI (void *arg)
{
	int i;

	while(1)
	{
		if(guiHalt)
			LWP_SuspendThread(guithread);

		UpdatePads();
		mainWindow->Draw();

		if (mainWindow->GetState() != STATE_DISABLED)
			mainWindow->DrawTooltip();

		#ifdef HW_RVL
		i = 3;
		do
		{
			if(userInput[i].wpad->ir.valid)
				Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
					96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
			DoRumble(i);
			--i;
		} while(i>=0);
		#endif

		Menu_Render();

		mainWindow->Update(&userInput[3]);
		mainWindow->Update(&userInput[2]);
		mainWindow->Update(&userInput[1]);
		mainWindow->Update(&userInput[0]);

		if(ExitRequested || ShutdownRequested)
		{
			for(i = 0; i <= 255; i += 15)
			{
				mainWindow->Draw();
				Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, (u8)i},1);
				Menu_Render();
			}
			ExitApp();
		}
		usleep(THREAD_SLEEP);
	}
	return NULL;
}

/****************************************************************************
 * ProgressWindow
 *
 * Opens a window, which displays progress to the user. Can either display a
 * progress bar showing % completion, or a throbber that only shows that an
 * action is in progress.
 ***************************************************************************/
static int progsleep = 0;

static void
ProgressWindow(char *title, char *msg)
{
	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiImageData progressbarOutline(progressbar_outline_png);
	GuiImage progressbarOutlineImg(&progressbarOutline);
	progressbarOutlineImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarOutlineImg.SetPosition(25, 40);

	GuiImageData progressbarEmpty(progressbar_empty_png);
	GuiImage progressbarEmptyImg(&progressbarEmpty);
	progressbarEmptyImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarEmptyImg.SetPosition(25, 40);
	progressbarEmptyImg.SetTile(100);

	GuiImageData progressbar(progressbar_png);
	GuiImage progressbarImg(&progressbar);
	progressbarImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarImg.SetPosition(25, 40);

	GuiImageData throbber(throbber_png);
	GuiImage throbberImg(&throbber);
	throbberImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	throbberImg.SetPosition(0, 40);

	GuiText titleTxt(title, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);
	GuiText msgTxt(msg, 26, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,80);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);

	if(showProgress == 1)
	{
		promptWindow.Append(&progressbarEmptyImg);
		promptWindow.Append(&progressbarImg);
		promptWindow.Append(&progressbarOutlineImg);
	}
	else
	{
		promptWindow.Append(&throbberImg);
	}

	// wait to see if progress flag changes soon
	progsleep = 800000;

	while(progsleep > 0)
	{
		if(!showProgress)
			break;
		usleep(THREAD_SLEEP);
		progsleep -= THREAD_SLEEP;
	}

	if(!showProgress)
		return;

	HaltGui();
	int oldState = mainWindow->GetState();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	float angle = 0;
	u32 count = 0;

	while(showProgress)
	{
		progsleep = 20000;

		while(progsleep > 0)
		{
			if(!showProgress)
				break;
			usleep(THREAD_SLEEP);
			progsleep -= THREAD_SLEEP;
		}

		if(showProgress == 1)
		{
			progressbarImg.SetTile(100*progressDone/progressTotal);
		}
		else if(showProgress == 2)
		{
			if(count % 5 == 0)
			{
				angle+=45.0f;
				if(angle >= 360.0f)
					angle = 0;
				throbberImg.SetAngle(angle);
			}
			++count;
		}
	}

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(oldState);
	ResumeGui();
}

static void * ProgressThread (void *arg)
{
	while(1)
	{
		if(!showProgress)
			LWP_SuspendThread (progressthread);

		ProgressWindow(progressTitle, progressMsg);
		usleep(THREAD_SLEEP);
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
void
InitGUIThreads()
{
	LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
	LWP_CreateThread (&progressthread, ProgressThread, NULL, NULL, 0, 40);
}

/****************************************************************************
 * CancelAction
 *
 * Signals the GUI progress window thread to halt, and waits for it to
 * finish. Prevents multiple progress window events from interfering /
 * overriding each other.
 ***************************************************************************/
void
CancelAction()
{
	showProgress = 0;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(progressthread))
		usleep(THREAD_SLEEP);
}

/****************************************************************************
 * ShowProgress
 *
 * Updates the variables used by the progress window for drawing a progress
 * bar. Also resumes the progress window thread if it is suspended.
 ***************************************************************************/
void
ShowProgress (const char *msg, int done, int total)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
		return;

	if(total < (256*1024))
		return;

	if(done > total) // this shouldn't happen
		done = total;

	if(showProgress != 1)
		CancelAction(); // wait for previous progress window to finish

	snprintf(progressMsg, 200, "%s", msg);
	sprintf(progressTitle, "Please Wait");
	showProgress = 1;
	progressTotal = total;
	progressDone = done;
	LWP_ResumeThread (progressthread);
}

/****************************************************************************
 * ShowAction
 *
 * Shows that an action is underway. Also resumes the progress window thread
 * if it is suspended.
 ***************************************************************************/
void
ShowAction (const char *msg)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
		return;

	if(showProgress != 0)
		CancelAction(); // wait for previous progress window to finish

	snprintf(progressMsg, 200, "%s", msg);
	sprintf(progressTitle, "Please Wait");
	showProgress = 2;
	progressDone = 0;
	progressTotal = 0;
	LWP_ResumeThread (progressthread);
}

void ErrorPrompt(const char *msg)
{
	WindowPrompt("Error", msg, "OK", NULL);
}

int ErrorPromptRetry(const char *msg)
{
	return WindowPrompt("Error", msg, "Retry", "Cancel");
}

void InfoPrompt(const char *msg)
{
	WindowPrompt("Information", msg, "OK", NULL);
}

int YesNoPrompt(const char *msg, bool yesDefault)
{
	return WindowPrompt("Goomba", msg, "Yes", "No", yesDefault);
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, u32 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetSoundClick(&btnSoundClick);
	okBtn.SetTrigger(trigA);
	okBtn.SetTrigger(trig2);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetSoundClick(&btnSoundClick);
	cancelBtn.SetTrigger(trigA);
	cancelBtn.SetTrigger(trig2);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}

/****************************************************************************
 * SettingWindow
 *
 * Opens a new window, with the specified window element appended. Allows
 * for a customizable prompted setting.
 ***************************************************************************/
static int
SettingWindow(const char * title, GuiWindow * w)
{
	int save = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(20, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetSoundClick(&btnSoundClick);
	okBtn.SetTrigger(trigA);
	okBtn.SetTrigger(trig2);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-20, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetSoundClick(&btnSoundClick);
	cancelBtn.SetTrigger(trigA);
	cancelBtn.SetTrigger(trig2);
	cancelBtn.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&okBtn);
	promptWindow.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->Append(w);
	mainWindow->ChangeFocus(w);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->Remove(w);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return save;
}

/****************************************************************************
 * WindowCredits
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/
static void WindowCredits(void * ptr)
{
	if(btnLogo->GetState() != STATE_CLICKED)
		return;

	btnLogo->ResetState();

	bool exit = false;
	int i = 0;
	int y = 20;

	GuiWindow creditsWindow(screenwidth,screenheight);
	GuiWindow creditsWindowBox(580,448);
	creditsWindowBox.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	creditsWindowBox.Append(&creditsBoxImg);

	int numEntries = 24;
	GuiText * txt[numEntries];

	txt[i] = new GuiText("Credits", 20, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=24;

	txt[i] = new GuiText("Official Site: https://github.com/dborth/vbagx", 20, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=32;

	GuiText::SetPresets(20, (GXColor){0, 0, 0, 255}, 0, FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP, ALIGN_LEFT, ALIGN_TOP);

	txt[i] = new GuiText("Main developer");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("Tantric");
	txt[i]->SetPosition(250,y); i++; y+=48;

	txt[i] = new GuiText("Additional coding");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("Zopenko, Glitch, libertyernie");
	txt[i]->SetPosition(250,y); i++; y+=24;
	txt[i] = new GuiText("cebolleto, bgK, Carl Kenner, dancinninjac");
	txt[i]->SetPosition(250,y); i++; y+=48;

	txt[i] = new GuiText("Menu artwork");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("the3seashells");
	txt[i]->SetPosition(250,y); i++; y+=24;
	txt[i] = new GuiText("Menu sound");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("Peter de Man");
	txt[i]->SetPosition(250,y); i++; y+=32;

	txt[i] = new GuiText("VBA GameCube");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("SoftDev, emukidid");
	txt[i]->SetPosition(250,y); i++; y+=24;
	txt[i] = new GuiText("Visual Boy Advance - M");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("VBA-M Team");
	txt[i]->SetPosition(250,y); i++; y+=24;
	txt[i] = new GuiText("Visual Boy Advance");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("Forgotten");
	txt[i]->SetPosition(250,y); i++; y+=24;

	txt[i] = new GuiText("libogc / devkitPPC");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("shagkur & WinterMute");
	txt[i]->SetPosition(250,y); i++; y+=24;
	txt[i] = new GuiText("FreeTypeGX");
	txt[i]->SetPosition(40,y); i++;
	txt[i] = new GuiText("Armin Tamzarian");
	txt[i]->SetPosition(250,y); i++;

	char wiiDetails[30];
	char wiiInfo[20];

#ifdef HW_RVL
	if(!IsWiiU()) {
		sprintf(wiiInfo, "Wii");
	}
	else if(IsWiiUFastCPU()) {
		sprintf(wiiInfo, "vWii (1.215 GHz)");
	}
	else {
		sprintf(wiiInfo, "vWii (729 MHz)");
	}
	sprintf(wiiDetails, "IOS: %d / %s", IOS_GetVersion(), wiiInfo);
#endif

	txt[i] = new GuiText(wiiDetails, 14, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	txt[i]->SetPosition(-20, -46); i++;

	GuiText::SetPresets(12, (GXColor){0, 0, 0, 255}, 0, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_TOP, ALIGN_CENTRE, ALIGN_BOTTOM);

	txt[i] = new GuiText("This software is open source and may be copied, distributed, or modified");
	txt[i]->SetPosition(0, -32); i++;
	txt[i] = new GuiText("under the terms of the GNU General Public License (GPL) Version 2.");
	txt[i]->SetPosition(0, -20);

	for(i=0; i < numEntries; i++)
		creditsWindowBox.Append(txt[i]);

	creditsWindow.Append(&creditsWindowBox);

	while(!exit)
	{
		UpdatePads();

		gameScreenImg->Draw();
		bgBottomImg->Draw();
		bgTopImg->Draw();
		creditsWindow.Draw();

		#ifdef HW_RVL
		i = 3;
		do {	
		if(userInput[i].wpad->ir.valid)
			Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
				96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
			DoRumble(i);
			--i;
		} while(i >= 0);
		#endif

		Menu_Render();

		if((userInput[0].wpad->btns_d || userInput[0].pad.btns_d || userInput[0].wiidrcdata.btns_d) ||
		   (userInput[1].wpad->btns_d || userInput[1].pad.btns_d || userInput[1].wiidrcdata.btns_d) ||
		   (userInput[2].wpad->btns_d || userInput[2].pad.btns_d || userInput[2].wiidrcdata.btns_d) ||
		   (userInput[3].wpad->btns_d || userInput[3].pad.btns_d || userInput[3].wiidrcdata.btns_d))
		{
			exit = true;
		}
		usleep(THREAD_SLEEP);
	}

	// clear buttons pressed
	for(i=0; i < 4; i++)
	{
		userInput[i].wiidrcdata.btns_d = 0;
		userInput[i].wpad->btns_d = 0;
		userInput[i].pad.btns_d = 0;
	}

	for(i=0; i < numEntries; i++)
		delete txt[i];
}

/****************************************************************************
 * MenuGameSelection
 *
 * Displays a list of games on the specified load device, and allows the user
 * to browse and select from this list.
 ***************************************************************************/
static char* getImageFolder()
{
	switch(GCSettings.PreviewImage)
	{
		case 1 : return GCSettings.CoverFolder; break;
		case 2 : return GCSettings.ArtworkFolder; break;
		default: return GCSettings.ScreenshotsFolder; break;
	}
}

static int MenuGameSelection()
{
	int menu = MENU_NONE;
	bool res;
	int i;

	GuiText titleTxt("Choose Game", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData iconHome(icon_home_png);
	GuiImageData iconSettings(icon_settings_png);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);
	GuiImageData bgPreviewImg(bg_preview_png);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0, WIIDRC_BUTTON_HOME);

	GuiText settingsBtnTxt("Settings", 22, (GXColor){0, 0, 0, 255});
	GuiImage settingsBtnIcon(&iconSettings);
	settingsBtnIcon.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	settingsBtnIcon.SetPosition(14,0);
	GuiImage settingsBtnImg(&btnOutline);
	GuiImage settingsBtnImgOver(&btnOutlineOver);
	GuiButton settingsBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	settingsBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	settingsBtn.SetPosition(90, -35);
	settingsBtn.SetLabel(&settingsBtnTxt);
	settingsBtn.SetIcon(&settingsBtnIcon);
	settingsBtn.SetImage(&settingsBtnImg);
	settingsBtn.SetImageOver(&settingsBtnImgOver);
	settingsBtn.SetSoundOver(&btnSoundOver);
	settingsBtn.SetSoundClick(&btnSoundClick);
	settingsBtn.SetTrigger(trigA);
	settingsBtn.SetTrigger(trig2);
	settingsBtn.SetEffectGrow();

	GuiText exitBtnTxt("Exit", 22, (GXColor){0, 0, 0, 255});
	GuiImage exitBtnIcon(&iconHome);
	exitBtnIcon.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	exitBtnIcon.SetPosition(14,0);
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	exitBtn.SetPosition(-90, -35);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetIcon(&exitBtnIcon);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetSoundOver(&btnSoundOver);
	exitBtn.SetSoundClick(&btnSoundClick);
	exitBtn.SetTrigger(trigA);
	exitBtn.SetTrigger(trig2);
	exitBtn.SetTrigger(&trigHome);
	exitBtn.SetEffectGrow();

	GuiWindow buttonWindow(screenwidth, screenheight);
	buttonWindow.Append(&settingsBtn);
	buttonWindow.Append(&exitBtn);

	GuiFileBrowser gameBrowser(330, 268);
	gameBrowser.SetPosition(20, 98);
	ResetBrowser();

	GuiTrigger trigPlusMinus;
	trigPlusMinus.SetButtonOnlyTrigger(-1, WPAD_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_PLUS, PAD_TRIGGER_Z, WIIDRC_BUTTON_PLUS);

	GuiImage bgPreview(&bgPreviewImg);
	GuiButton bgPreviewBtn(bgPreview.GetWidth(), bgPreview.GetHeight());
	bgPreviewBtn.SetImage(&bgPreview);
	bgPreviewBtn.SetPosition(365, 98);
	bgPreviewBtn.SetTrigger(&trigPlusMinus);
	int previousPreviewImg = GCSettings.PreviewImage;
	
	GuiImage preview;
	preview.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	preview.SetPosition(174, -8);
	u8* imgBuffer = MEM_ALLOC(640 * 480 * 4);
	int  previousBrowserIndex = -1;
	char imagePath[MAXJOLIET + 1];

	HaltGui();
	btnLogo->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	btnLogo->SetPosition(-50, 24);
	mainWindow->Append(&titleTxt);
	mainWindow->Append(&gameBrowser);
	mainWindow->Append(&buttonWindow);
	mainWindow->Append(&bgPreviewBtn);
	mainWindow->Append(&preview);
	ResumeGui();

	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	// populate initial directory listing
	selectLoadedFile = 1;
	OpenGameList();

	gameBrowser.ResetState();
	gameBrowser.fileList[0]->SetState(STATE_SELECTED);
	gameBrowser.TriggerUpdate();
	titleTxt.SetText(inSz ? szname : "Choose Game");

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(selectLoadedFile == 2)
		{
			selectLoadedFile = 0;
			mainWindow->ChangeFocus(&gameBrowser);
			gameBrowser.TriggerUpdate();
		}

		// update gameWindow based on arrow buttons
		// set MENU_EXIT if A button pressed on a game
		for(i=0; i < FILE_PAGESIZE; i++)
		{
			if(gameBrowser.fileList[i]->GetState() == STATE_CLICKED)
			{
				gameBrowser.fileList[i]->ResetState();
				
				// check corresponding browser entry
				if(browserList[browser.selIndex].isdir || IsSz())
				{	
					HaltGui();
					res = BrowserChangeFolder();
					if(res)
					{
						gameBrowser.ResetState();
						gameBrowser.fileList[0]->SetState(STATE_SELECTED);
						gameBrowser.TriggerUpdate();
						previousBrowserIndex = -1;			
					}
					else
					{
						menu = MENU_GAMESELECTION;
						break;
					}

					titleTxt.SetText(inSz ? szname : "Choose Game");
					
					ResumeGui();
				}
				else
				{
					#ifdef HW_RVL
					ShutoffRumble();
					#endif
					mainWindow->SetState(STATE_DISABLED);
					SavePrefs(SILENT);
					if(BrowserLoadFile())
						menu = MENU_EXIT;
					else
						mainWindow->SetState(STATE_DEFAULT);
				}
			}
		}
		
		//update gamelist image
		if(previousBrowserIndex != browser.selIndex || previousPreviewImg != GCSettings.PreviewImage)
		{
			previousBrowserIndex = browser.selIndex;
			previousPreviewImg = GCSettings.PreviewImage;
			snprintf(imagePath, MAXJOLIET, "%s%s/%s.png", pathPrefix[GCSettings.LoadMethod], getImageFolder(), browserList[browser.selIndex].displayname);

			int width, height;
			if(DecodePNGFromFile(imagePath, &width, &height, imgBuffer, 640, 480))
			{
				preview.SetImage(imgBuffer, width, height);
				preview.SetScale( MIN(225.0f / width, 235.0f / height) );
			}
			else
			{
				preview.SetImage(NULL, 0, 0);
			}
		}

		if(settingsBtn.GetState() == STATE_CLICKED)
			menu = MENU_SETTINGS;
		else if(exitBtn.GetState() == STATE_CLICKED)
			ExitRequested = 1;
		else if(bgPreviewBtn.GetState() == STATE_CLICKED)
		{
			GCSettings.PreviewImage = (GCSettings.PreviewImage + 1) % 3;
			bgPreviewBtn.ResetState();
		}
	}

	HaltParseThread(); // halt parsing
	HaltGui();
	ResetBrowser();
	mainWindow->Remove(&titleTxt);
	mainWindow->Remove(&buttonWindow);
	mainWindow->Remove(&gameBrowser);
	mainWindow->Remove(&bgPreviewBtn);
	mainWindow->Remove(&preview);
	MEM_DEALLOC(imgBuffer);
	return menu;
}

#ifdef HW_RVL
static int playerMappingChan = 0;

static void PlayerMappingWindowUpdate(void * ptr, int dir)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		playerMapping[playerMappingChan] += dir;

		if(playerMapping[playerMappingChan] > 3)
			playerMapping[playerMappingChan] = 0;
		if(playerMapping[playerMappingChan] < 0)
			playerMapping[playerMappingChan] = 3;

		char playerNumber[20];
		sprintf(playerNumber, "Player %d", playerMapping[playerMappingChan]+1);

		settingText->SetText(playerNumber);
		b->ResetState();
	}
}

static void PlayerMappingWindowLeftClick(void * ptr) { PlayerMappingWindowUpdate(ptr, -1); }
static void PlayerMappingWindowRightClick(void * ptr) { PlayerMappingWindowUpdate(ptr, +1); }

static void PlayerMappingWindow(int chan)
{
	playerMappingChan = chan;

	GuiWindow * w = new GuiWindow(300,250);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT, WIIDRC_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT, WIIDRC_BUTTON_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(trigA);
	arrowLeftBtn.SetTrigger(trig2);
	arrowLeftBtn.SetTrigger(&trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(PlayerMappingWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(trigA);
	arrowRightBtn.SetTrigger(trig2);
	arrowRightBtn.SetTrigger(&trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(PlayerMappingWindowRightClick);

	char playerNumber[20];
	sprintf(playerNumber, "Player %d", playerMapping[playerMappingChan]+1);

	settingText = new GuiText(playerNumber, 22, (GXColor){0, 0, 0, 255});

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(settingText);

	char title[50];
	sprintf(title, "Player Mapping - Controller %d", chan+1);

	int previousPlayerMapping = playerMapping[playerMappingChan];

	if(!SettingWindow(title,w))
		playerMapping[playerMappingChan] = previousPlayerMapping; // undo changes

	delete(w);
	delete(settingText);
}
#endif

/****************************************************************************
 * MenuGame
 *
 * Menu displayed when returning to the menu from in-game.
 ***************************************************************************/
static int MenuGame()
{
	int menu = MENU_NONE;

	// Weather menu if a game with Boktai solar sensor
	bool isBoktai = ((RomIdCode & 0xFF)=='U');
    char s[64];

	GuiText titleTxt(ROMFilename, 22, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconGameSettings(icon_game_settings_png);
	GuiImageData iconLoad(icon_game_load_png);
	GuiImageData iconSave(icon_game_save_png);
	GuiImageData iconDelete(icon_game_delete_png);
	GuiImageData iconReset(icon_game_reset_png);

	GuiImageData battery(battery_png);
	GuiImageData batteryRed(battery_red_png);
	GuiImageData batteryBar(battery_bar_png);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0, WIIDRC_BUTTON_HOME);

	int xOffset=125;
	if (isBoktai) 
		xOffset=200;

	GuiText saveBtnTxt("Save", 22, (GXColor){0, 0, 0, 255});
	GuiImage saveBtnImg(&btnLargeOutline);
	GuiImage saveBtnImgOver(&btnLargeOutlineOver);
	GuiImage saveBtnIcon(&iconSave);
	GuiButton saveBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	saveBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	saveBtn.SetPosition(-200, 120);
	saveBtn.SetLabel(&saveBtnTxt);
	saveBtn.SetImage(&saveBtnImg);
	saveBtn.SetImageOver(&saveBtnImgOver);
	saveBtn.SetIcon(&saveBtnIcon);
	saveBtn.SetSoundOver(&btnSoundOver);
	saveBtn.SetSoundClick(&btnSoundClick);
	saveBtn.SetTrigger(trigA);
	saveBtn.SetTrigger(trig2);
	saveBtn.SetEffectGrow();

	GuiText loadBtnTxt("Load", 22, (GXColor){0, 0, 0, 255});
	GuiImage loadBtnImg(&btnLargeOutline);
	GuiImage loadBtnImgOver(&btnLargeOutlineOver);
	GuiImage loadBtnIcon(&iconLoad);
	GuiButton loadBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	loadBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	loadBtn.SetPosition(0, 120);
	loadBtn.SetLabel(&loadBtnTxt);
	loadBtn.SetImage(&loadBtnImg);
	loadBtn.SetImageOver(&loadBtnImgOver);
	loadBtn.SetIcon(&loadBtnIcon);
	loadBtn.SetSoundOver(&btnSoundOver);
	loadBtn.SetSoundClick(&btnSoundClick);
	loadBtn.SetTrigger(trigA);
	loadBtn.SetTrigger(trig2);
	loadBtn.SetEffectGrow();

	GuiText deleteBtnTxt("Delete", 22, (GXColor){0, 0, 0, 255});
	GuiImage deleteBtnImg(&btnLargeOutline);
	GuiImage deleteBtnImgOver(&btnLargeOutlineOver);
	GuiImage deleteBtnIcon(&iconDelete);
	GuiButton deleteBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	deleteBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	deleteBtn.SetPosition(200, 120);
	deleteBtn.SetLabel(&deleteBtnTxt);
	deleteBtn.SetImage(&deleteBtnImg);
	deleteBtn.SetImageOver(&deleteBtnImgOver);
	deleteBtn.SetIcon(&deleteBtnIcon);
	deleteBtn.SetSoundOver(&btnSoundOver);
	deleteBtn.SetSoundClick(&btnSoundClick);
	deleteBtn.SetTrigger(trigA);
	deleteBtn.SetTrigger(trig2);
	deleteBtn.SetEffectGrow();
	
	// Boktai adds an extra button for setting the sun.
	GuiText *sunBtnTxt = NULL;
	GuiImage *sunBtnImg = NULL;
	GuiImage *sunBtnImgOver = NULL;
	GuiButton *sunBtn = NULL;
	if (isBoktai) 
	{
		struct tm *newtime;
		time_t long_time;

		// regardless of the weather, there should be no sun at night time!
		time(&long_time); // Get time as long integer.
		newtime = localtime(&long_time); // Convert to local time.
		if (newtime->tm_hour > 21 || newtime->tm_hour < 5)
		{
			sprintf(s, "Weather: Night Time");
		} 
		else 
			sprintf(s, "Weather: %d%% sun", SunBars*10);
		
		sunBtnTxt = new GuiText(s, 22, (GXColor){0, 0, 0, 255});
		sunBtnTxt->SetWrap(true, btnLargeOutline.GetWidth()-30);
		sunBtnImg = new GuiImage(&btnLargeOutline);
		sunBtnImgOver = new GuiImage(&btnLargeOutlineOver);
		sunBtn = new GuiButton(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
		sunBtn->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
		sunBtn->SetPosition(0, 250);
		sunBtn->SetLabel(sunBtnTxt);
		sunBtn->SetImage(sunBtnImg);
		sunBtn->SetImageOver(sunBtnImgOver);
		sunBtn->SetSoundOver(&btnSoundOver);
		sunBtn->SetSoundClick(&btnSoundClick);
		sunBtn->SetTrigger(trigA);
		sunBtn->SetTrigger(trig2);
		sunBtn->SetEffectGrow();
	}

	GuiText resetBtnTxt("Reset", 22, (GXColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnLargeOutline);
	GuiImage resetBtnImgOver(&btnLargeOutlineOver);
	GuiImage resetBtnIcon(&iconReset);
	GuiButton resetBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	resetBtn.SetPosition(xOffset, 250);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetIcon(&resetBtnIcon);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetSoundClick(&btnSoundClick);
	resetBtn.SetTrigger(trigA);
	resetBtn.SetTrigger(trig2);
	resetBtn.SetEffectGrow();

	GuiText gameSettingsBtnTxt("Game Settings", 22, (GXColor){0, 0, 0, 255});
	gameSettingsBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage gameSettingsBtnImg(&btnLargeOutline);
	GuiImage gameSettingsBtnImgOver(&btnLargeOutlineOver);
	GuiImage gameSettingsBtnIcon(&iconGameSettings);
	GuiButton gameSettingsBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	gameSettingsBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	gameSettingsBtn.SetPosition(-xOffset, 250);
	gameSettingsBtn.SetLabel(&gameSettingsBtnTxt);
	gameSettingsBtn.SetImage(&gameSettingsBtnImg);
	gameSettingsBtn.SetImageOver(&gameSettingsBtnImgOver);
	gameSettingsBtn.SetIcon(&gameSettingsBtnIcon);
	gameSettingsBtn.SetSoundOver(&btnSoundOver);
	gameSettingsBtn.SetSoundClick(&btnSoundClick);
	gameSettingsBtn.SetTrigger(trigA);
	gameSettingsBtn.SetTrigger(trig2);
	gameSettingsBtn.SetEffectGrow();

	GuiText mainmenuBtnTxt("Main Menu", 22, (GXColor){0, 0, 0, 255});
	if(GCSettings.AutoloadGame) {
		mainmenuBtnTxt.SetText("Exit");
	}
	GuiImage mainmenuBtnImg(&btnOutline);
	GuiImage mainmenuBtnImgOver(&btnOutlineOver);
	GuiButton mainmenuBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	mainmenuBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	mainmenuBtn.SetPosition(0, -35);
	mainmenuBtn.SetLabel(&mainmenuBtnTxt);
	mainmenuBtn.SetImage(&mainmenuBtnImg);
	mainmenuBtn.SetImageOver(&mainmenuBtnImgOver);
	mainmenuBtn.SetSoundOver(&btnSoundOver);
	mainmenuBtn.SetSoundClick(&btnSoundClick);
	mainmenuBtn.SetTrigger(trigA);
	mainmenuBtn.SetTrigger(trig2);
	mainmenuBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 20, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-50, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetSoundClick(&btnSoundClick);
	closeBtn.SetTrigger(trigA);
	closeBtn.SetTrigger(trig2);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	#ifdef HW_RVL
	int i;
	char txt[3];
	bool status[4] = { false, false, false, false };
	int level[4] = { 0, 0, 0, 0 };
	bool newStatus;
	int newLevel;
	GuiText * batteryTxt[4];
	GuiImage * batteryImg[4];
	GuiImage * batteryBarImg[4];
	GuiButton * batteryBtn[4];

	for(i=0; i < 4; ++i)
	{
		sprintf(txt, "P%d", i+1);

		batteryTxt[i] = new GuiText(txt, 20, (GXColor){255, 255, 255, 255});
		batteryTxt[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i] = new GuiImage(&battery);
		batteryImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i]->SetPosition(30, 0);
		batteryBarImg[i] = new GuiImage(&batteryBar);
		batteryBarImg[i]->SetTile(0);
		batteryBarImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryBarImg[i]->SetPosition(34, 0);

		batteryBtn[i] = new GuiButton(70, 20);
		batteryBtn[i]->SetLabel(batteryTxt[i]);
		batteryBtn[i]->SetImage(batteryImg[i]);
		batteryBtn[i]->SetIcon(batteryBarImg[i]);
		batteryBtn[i]->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		batteryBtn[i]->SetTrigger(trigA);
		batteryBtn[i]->SetSoundOver(&btnSoundOver);
		batteryBtn[i]->SetSoundClick(&btnSoundClick);
		batteryBtn[i]->SetSelectable(false);
		batteryBtn[i]->SetState(STATE_DISABLED);
		batteryBtn[i]->SetAlpha(150);
	}

	batteryBtn[0]->SetPosition(45, -65);
	batteryBtn[1]->SetPosition(135, -65);
	batteryBtn[2]->SetPosition(45, -40);
	batteryBtn[3]->SetPosition(135, -40);
	#endif

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&saveBtn);
	w.Append(&loadBtn);
	w.Append(&deleteBtn);
	w.Append(&resetBtn);
	w.Append(&gameSettingsBtn);
	if (isBoktai)
		w.Append(sunBtn);

	#ifdef HW_RVL
	w.Append(batteryBtn[0]);
	w.Append(batteryBtn[1]);
	w.Append(batteryBtn[2]);
	w.Append(batteryBtn[3]);
	#endif

	w.Append(&mainmenuBtn);
	w.Append(&closeBtn);

	btnLogo->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btnLogo->SetPosition(-50, -40);
	mainWindow->Append(&w);

	if(lastMenu == MENU_NONE)
	{
		enterSound->Play();
		bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		mainmenuBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		#ifdef HW_RVL
		batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		#endif

		w.SetEffect(EFFECT_FADE, 15);
	}

	ResumeGui();

	if(lastMenu == MENU_NONE)
	{
		if (GCSettings.AutoSave == 1)
		{
			SaveBatteryOrStateAuto(FILE_SRAM, SILENT); // save battery
		}
		else if (GCSettings.AutoSave == 2)
		{
			if (WindowPrompt("Save", "Save State?", "Save", "Don't Save") )
				SaveBatteryOrStateAuto(FILE_SNAPSHOT, NOTSILENT); // save state
		}
		else if (GCSettings.AutoSave == 3)
		{
			if (WindowPrompt("Save", "Save SRAM and State?", "Save", "Don't Save") )
			{
				SaveBatteryOrStateAuto(FILE_SRAM, NOTSILENT); // save battery
				SaveBatteryOrStateAuto(FILE_SNAPSHOT, NOTSILENT); // save state
			}
		}
	}

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		#ifdef HW_RVL
		for(i=0; i < 4; i++)
		{
			if(WPAD_Probe(i, NULL) == WPAD_ERR_NONE)
			{
				newStatus = true;
				newLevel = (userInput[i].wpad->battery_level / 100.0) * 4;
				if(newLevel > 4) newLevel = 4;
			}
			else
			{
				newStatus = false;
				newLevel = 0;
			}

			if(status[i] != newStatus || level[i] != newLevel)
			{
				if(newStatus == true) // controller connected
				{
					batteryBtn[i]->SetAlpha(255);
					batteryBtn[i]->SetState(STATE_DEFAULT);
					batteryBarImg[i]->SetTile(newLevel);

					if(newLevel == 0)
						batteryImg[i]->SetImage(&batteryRed);
					else
						batteryImg[i]->SetImage(&battery);
				}
				else // controller not connected
				{
					batteryBtn[i]->SetAlpha(150);
					batteryBtn[i]->SetState(STATE_DISABLED);
					batteryBarImg[i]->SetTile(0);
					batteryImg[i]->SetImage(&battery);
				}
				status[i] = newStatus;
				level[i] = newLevel;
			}
		}
		#endif

		if (isBoktai)
		{
			if (sunBtn->GetState() == STATE_CLICKED) 
			{
				++SunBars;
				if (SunBars>10) SunBars=0;
				menu = MENU_GAME;
			}
		}

		if(saveBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME_SAVE;
		}
		else if(loadBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME_LOAD;
		}
		else if(deleteBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME_DELETE;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			if (WindowPrompt("Reset Game", "Reset this game? Any unsaved progress will be lost.", "OK", "Cancel"))
			{
				emulator.emuReset();
				menu = MENU_EXIT;
			}
		}
		else if(gameSettingsBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS;
		}
#ifdef HW_RVL
		else if(batteryBtn[0]->GetState() == STATE_CLICKED)
		{
			PlayerMappingWindow(0);
		}
		else if(batteryBtn[1]->GetState() == STATE_CLICKED)
		{
			PlayerMappingWindow(1);
		}
		else if(batteryBtn[2]->GetState() == STATE_CLICKED)
		{
			PlayerMappingWindow(2);
		}
		else if(batteryBtn[3]->GetState() == STATE_CLICKED)
		{
			PlayerMappingWindow(3);
		}
#endif
		else if(mainmenuBtn.GetState() == STATE_CLICKED)
		{
			if (WindowPrompt("Quit Game", "Quit this game? Any unsaved progress will be lost.", "OK", "Cancel"))
			{
				HaltGui();
				mainWindow->Remove(gameScreenImg);
				delete gameScreenImg;
				delete gameScreen;
				gameScreen = NULL;
				ClearScreenshot();
				if(GCSettings.AutoloadGame) {
					ExitApp();
				}
				else {
					gameScreenImg = new GuiImage(screenwidth, screenheight, (GXColor){236, 226, 238, 255});
					gameScreenImg->ColorStripe(10);
					mainWindow->Insert(gameScreenImg, 0);
					ResumeGui();
					#ifndef NO_SOUND
					bgMusic->Play(); // startup music
					#endif
					menu = MENU_GAMESELECTION;
				}
			}
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;

			exitSound->Play();
			bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			mainmenuBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			#ifdef HW_RVL
			batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			#endif

			w.SetEffect(EFFECT_FADE, -15);
			usleep(350000); // wait for effects to finish
		}
	}

	HaltGui();

	if (isBoktai) {
		delete sunBtnTxt;
		delete sunBtnImg;
		delete sunBtnImgOver;
		delete sunBtn;
	}

	#ifdef HW_RVL
	for(i=0; i < 4; ++i)
	{
		delete batteryTxt[i];
		delete batteryImg[i];
		delete batteryBarImg[i];
		delete batteryBtn[i];
	}
	#endif

	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * FindGameSaveNum
 *
 * Determines the save file number of the given file name
 * Returns -1 if none is found
 ***************************************************************************/
static int FindGameSaveNum(char * savefile, int method)
{
	int n = -1;
	int romlen = strlen(ROMFilename);
	int savelen = strlen(savefile);
	int diff = savelen-romlen;

	if(strncmp(savefile, ROMFilename, romlen) != 0)
		return -1;

	if(savefile[romlen] == ' ')
	{
		if(diff == 5 && strncmp(&savefile[romlen+1], "Auto", 4) == 0)
			n = 0; // found Auto save
		else if(diff == 2 || diff == 3)
			n = atoi(&savefile[romlen+1]);
	}

	if(n >= 0 && n < MAX_SAVES)
		return n;
	else
		return -1;
}

/****************************************************************************
 * MenuGameSaves
 *
 * Allows the user to load or save progress.
 ***************************************************************************/
static int MenuGameSaves(int action)
{
	SaveList saves;
	struct stat filestat;
	struct tm * timeinfo;

	int menu = MENU_NONE;
	int ret, result;
	int i, n, type, len, len2;
	int j = 0;

	char filepath[1024];
	char deletepath[1024];
	char scrfile[1024];
	char tmp[MAXJOLIET+1];

	int method = GCSettings.SaveMethod;

	if(method == DEVICE_AUTO)
		autoSaveMethod(NOTSILENT);

	if(!ChangeInterface(method, NOTSILENT))
		return MENU_GAME;

	GuiText titleTxt(NULL, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	if(action == 0)
		titleTxt.SetText("Load Game");
	else if (action == 2)
		titleTxt.SetText("Delete Saves");
	else
		titleTxt.SetText("Save Game");

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0, WIIDRC_BUTTON_HOME);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(50, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 20, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-50, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetSoundClick(&btnSoundClick);
	closeBtn.SetTrigger(trigA);
	closeBtn.SetTrigger(trig2);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	w.Append(&closeBtn);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	memset(&saves, 0, sizeof(saves));

	sprintf(browser.dir, "%s%s", pathPrefix[GCSettings.SaveMethod], GCSettings.SaveFolder);
	ParseDirectory(true, false);

	len = strlen(ROMFilename);

	// find matching files
	AllocSaveBuffer();

	for(i=0; i < browser.numEntries; i++)
	{
		len2 = strlen(browserList[i].filename);

		if(len2 < 6 || len2-len < 5)
			continue;

		if(strncmp(&browserList[i].filename[len2-4], ".sav", 4) == 0)
			type = FILE_SRAM;
		else if(strncmp(&browserList[i].filename[len2-4], ".sgm", 4) == 0)
			type = FILE_SNAPSHOT;
		else
			continue;

		strcpy(tmp, browserList[i].filename);
		tmp[len2-4] = 0;
		n = FindGameSaveNum(tmp, method);

		if(n >= 0)
		{
			saves.type[j] = type;
			saves.files[saves.type[j]][n] = 1;
			strcpy(saves.filename[j], browserList[i].filename);

			if(saves.type[j] == FILE_SNAPSHOT)
			{
				sprintf(scrfile, "%s%s/%s.png", pathPrefix[GCSettings.SaveMethod], GCSettings.SaveFolder, tmp);

				memset(savebuffer, 0, SAVEBUFFERSIZE);
				if(LoadFile(scrfile, SILENT))
					saves.previewImg[j] = new GuiImageData(savebuffer, 64, 48);
			}
			snprintf(filepath, 1024, "%s%s/%s", pathPrefix[GCSettings.SaveMethod], GCSettings.SaveFolder, saves.filename[j]);
			if (stat(filepath, &filestat) == 0)
			{
				timeinfo = localtime(&filestat.st_mtime);
				strftime(saves.date[j], 20, "%a %b %d", timeinfo);
				strftime(saves.time[j], 10, "%I:%M %p", timeinfo);
			}
			++j;
		}
	}

	FreeSaveBuffer();
	saves.length = j;

	if((saves.length == 0 && action == 0) || (saves.length == 0 && action == 2)) 
	{
		InfoPrompt("No game saves found.");
		menu = MENU_GAME;
	}

	GuiSaveBrowser saveBrowser(552, 248, &saves, action);
	saveBrowser.SetPosition(0, 108);
	saveBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	mainWindow->Append(&saveBrowser);
	mainWindow->ChangeFocus(&saveBrowser);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = saveBrowser.GetClickedSave();

		// load, save and delete save games
		if(ret > -3)
		{
			result = 0;

			if(action == 0) // load
			{
				MakeFilePath(filepath, saves.type[ret], saves.filename[ret]);
				switch(saves.type[ret])
				{
					case FILE_SRAM:
						result = LoadBatteryOrState(filepath, saves.type[ret], NOTSILENT);
						emulator.emuReset();
						break;
					case FILE_SNAPSHOT:
						result = LoadBatteryOrState(filepath, saves.type[ret], NOTSILENT);
						break;
				}
				if(result)
					menu = MENU_EXIT;
			}
			else if(action == 2) // delete SRAM/State
			{
				if (WindowPrompt("Delete File", "Delete this save file? Deleted files can not be restored.", "OK", "Cancel"))
				{
					MakeFilePath(filepath, saves.type[ret], saves.filename[ret]);
					switch(saves.type[ret])
					{
						case FILE_SRAM:
							strncpy(deletepath, filepath, 1024);
							deletepath[strlen(deletepath)-4] = 0;
							strcat(deletepath, ".sav");
							remove(deletepath); // Delete the *.srm file (Battery save file)
						break;
						case FILE_SNAPSHOT:
							strncpy(deletepath, filepath, 1024);
							deletepath[strlen(deletepath)-4] = 0;
							strcat(deletepath, ".png");
							remove(deletepath); // Delete the *.png file (Screenshot file)
							strncpy(deletepath, filepath, 1024);
							deletepath[strlen(deletepath)-4] = 0;
							strcat(deletepath, ".sgm");
							remove(deletepath); // Delete the *.frz file (Save State file)
						break;
					}							
				}
				menu = MENU_GAME_DELETE;
			
			
			}
			else // save
			{
				if(ret == -2) // new SRAM
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_SRAM][i] == 0)
							break;

					if(i < 100)
					{
						MakeFilePath(filepath, FILE_SRAM, ROMFilename, i);
						SaveBatteryOrState(filepath, FILE_SRAM, NOTSILENT);
						menu = MENU_GAME_SAVE;
					}
				}
				else if(ret == -1) // new State
				{
					for(i=1; i < 100; i++)
						if(saves.files[FILE_SNAPSHOT][i] == 0)
							break;

					if(i < 100)
					{
						MakeFilePath(filepath, FILE_SNAPSHOT, ROMFilename, i);
						SaveBatteryOrState(filepath, FILE_SNAPSHOT, NOTSILENT);
						menu = MENU_GAME_SAVE;
					}
				}
				else // overwrite SRAM/State
				{
					MakeFilePath(filepath, saves.type[ret], saves.filename[ret]);
					switch(saves.type[ret])
					{
						case FILE_SRAM:
							SaveBatteryOrState(filepath, FILE_SRAM, NOTSILENT);
							break;
						case FILE_SNAPSHOT:
							SaveBatteryOrState(filepath, FILE_SNAPSHOT, NOTSILENT);
							break;
					}
					menu = MENU_GAME_SAVE;
				}
			}
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;

			exitSound->Play();
			bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			backBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);

			w.SetEffect(EFFECT_FADE, -15);

			usleep(350000); // wait for effects to finish
		}
	}

	HaltGui();

	for(i=0; i < saves.length; i++)
		if(saves.previewImg[i])
			delete saves.previewImg[i];

	mainWindow->Remove(&saveBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	ResetBrowser();
	return menu;
}


/****************************************************************************
 * MenuGameSettings
 ***************************************************************************/
static int MenuGameSettings()
{
	int menu = MENU_NONE;
	char s[4];
	char filepath[1024];

	GuiText titleTxt("Game Settings", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconMappings(icon_settings_mappings_png);
	GuiImageData iconVideo(icon_settings_video_png);
#ifdef HW_RVL
	GuiImageData iconWiiControls(icon_settings_nunchuk_png);
#else
	GuiImageData iconWiiControls(icon_settings_gamecube_png);
#endif
	GuiImageData iconScreenshot(icon_settings_screenshot_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0, WIIDRC_BUTTON_HOME);

	GuiText mappingBtnTxt("Button Mappings", 22, (GXColor){0, 0, 0, 255});
	mappingBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage mappingBtnImg(&btnLargeOutline);
	GuiImage mappingBtnImgOver(&btnLargeOutlineOver);
	GuiImage mappingBtnIcon(&iconMappings);
	GuiButton mappingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	mappingBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	mappingBtn.SetPosition(-125, 120);
	mappingBtn.SetLabel(&mappingBtnTxt);
	mappingBtn.SetImage(&mappingBtnImg);
	mappingBtn.SetImageOver(&mappingBtnImgOver);
	mappingBtn.SetIcon(&mappingBtnIcon);
	mappingBtn.SetSoundOver(&btnSoundOver);
	mappingBtn.SetSoundClick(&btnSoundClick);
	mappingBtn.SetTrigger(trigA);
	mappingBtn.SetTrigger(trig2);
	mappingBtn.SetEffectGrow();

	GuiText videoBtnTxt("Video", 22, (GXColor){0, 0, 0, 255});
	videoBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage videoBtnImg(&btnLargeOutline);
	GuiImage videoBtnImgOver(&btnLargeOutlineOver);
	GuiImage videoBtnIcon(&iconVideo);
	GuiButton videoBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	videoBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	videoBtn.SetPosition(125, 120);
	videoBtn.SetLabel(&videoBtnTxt);
	videoBtn.SetImage(&videoBtnImg);
	videoBtn.SetImageOver(&videoBtnImgOver);
	videoBtn.SetIcon(&videoBtnIcon);
	videoBtn.SetSoundOver(&btnSoundOver);
	videoBtn.SetSoundClick(&btnSoundClick);
	videoBtn.SetTrigger(trigA);
	videoBtn.SetTrigger(trig2);
	videoBtn.SetEffectGrow();

	#ifdef HW_RVL
	GuiText wiiControlsBtnTxt1("Match Wii Controls", 22, (GXColor){0, 0, 0, 255});
	#else
	GuiText wiiControlsBtnTxt1("Match GC Controls", 22, (GXColor){0, 0, 0, 255});
	#endif
	if (GCSettings.WiiControls) sprintf(s, "ON");
	else sprintf(s, "OFF");
	GuiText wiiControlsBtnTxt2(s, 18, (GXColor){0, 0, 0, 255});
	wiiControlsBtnTxt1.SetPosition(0, -10);
	wiiControlsBtnTxt1.SetWrap(true, btnLargeOutline.GetWidth()-30);
	wiiControlsBtnTxt2.SetPosition(0, +30);
	GuiImage wiiControlsBtnImg(&btnLargeOutline);
	GuiImage wiiControlsBtnImgOver(&btnLargeOutlineOver);
	GuiImage wiiControlsBtnIcon(&iconWiiControls);
	GuiButton wiiControlsBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	wiiControlsBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	wiiControlsBtn.SetPosition(-125, 250);
	wiiControlsBtn.SetLabel(&wiiControlsBtnTxt1, 0);
	wiiControlsBtn.SetLabel(&wiiControlsBtnTxt2, 1);
	wiiControlsBtn.SetImage(&wiiControlsBtnImg);
	wiiControlsBtn.SetImageOver(&wiiControlsBtnImgOver);
	wiiControlsBtn.SetIcon(&wiiControlsBtnIcon);
	wiiControlsBtn.SetSoundOver(&btnSoundOver);
	wiiControlsBtn.SetSoundClick(&btnSoundClick);
	wiiControlsBtn.SetTrigger(trigA);
	wiiControlsBtn.SetTrigger(trig2);
	wiiControlsBtn.SetEffectGrow();

	GuiText screenshotBtnTxt("Screenshot", 22, (GXColor){0, 0, 0, 255});
	GuiImage screenshotBtnImg(&btnLargeOutline);
	GuiImage screenshotBtnImgOver(&btnLargeOutlineOver);
	GuiImage screenshotBtnIcon(&iconScreenshot);
	GuiButton screenshotBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	screenshotBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	screenshotBtn.SetPosition(125, 250);
	screenshotBtn.SetLabel(&screenshotBtnTxt);
	screenshotBtn.SetImage(&screenshotBtnImg);
	screenshotBtn.SetImageOver(&screenshotBtnImgOver);
	screenshotBtn.SetIcon(&screenshotBtnIcon);
	screenshotBtn.SetSoundOver(&btnSoundOver);
	screenshotBtn.SetSoundClick(&btnSoundClick);
	screenshotBtn.SetTrigger(trigA);
	screenshotBtn.SetTrigger(trig2);
	screenshotBtn.SetEffectGrow();
	
	GuiText closeBtnTxt("Close", 20, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-50, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetSoundClick(&btnSoundClick);
	closeBtn.SetTrigger(trigA);
	closeBtn.SetTrigger(trig2);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(50, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&mappingBtn);
	w.Append(&videoBtn);
	w.Append(&wiiControlsBtn);
	w.Append(&screenshotBtn);
	w.Append(&closeBtn);
	w.Append(&backBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(mappingBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS;
		}
		else if(videoBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_VIDEO;
		}
		else if(wiiControlsBtn.GetState() == STATE_CLICKED)
		{
			GCSettings.WiiControls ^= 1;
			if (GCSettings.WiiControls) sprintf(s, "ON");
			else sprintf(s, "OFF");
			wiiControlsBtnTxt2.SetText(s);
			wiiControlsBtn.ResetState();
		}
		else if(screenshotBtn.GetState() == STATE_CLICKED)
		{
			if (WindowPrompt("Preview Screenshot", "Save a new Preview Screenshot? Current Screenshot image will be overwritten.", "OK", "Cancel"))
			{
				snprintf(filepath, 1024, "%s%s/%s", pathPrefix[GCSettings.SaveMethod], GCSettings.ScreenshotsFolder, ROMFilename);
				SavePreviewImg(filepath, SILENT); 
			}
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;

			exitSound->Play();
			bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			backBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);

			w.SetEffect(EFFECT_FADE, -15);

			usleep(350000); // wait for effects to finish
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAME;
		}
	}

	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MenuSettingsMappings
 ***************************************************************************/
static int MenuSettingsMappings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Game Settings - Button Mappings", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconWiimote(icon_settings_wiimote_png);
	GuiImageData iconClassic(icon_settings_classic_png);
	GuiImageData iconGamecube(icon_settings_gamecube_png);
	GuiImageData iconNunchuk(icon_settings_nunchuk_png);
	GuiImageData iconWiiupro(icon_settings_wiiupro_png);
	GuiImageData iconDrc(icon_settings_drc_png);

	GuiText gamecubeBtnTxt("GameCube Controller", 22, (GXColor){0, 0, 0, 255});
	gamecubeBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage gamecubeBtnImg(&btnLargeOutline);
	GuiImage gamecubeBtnImgOver(&btnLargeOutlineOver);
	GuiImage gamecubeBtnIcon(&iconGamecube);
	GuiButton gamecubeBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	gamecubeBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	gamecubeBtn.SetPosition(-125, 120);
	gamecubeBtn.SetLabel(&gamecubeBtnTxt);
	gamecubeBtn.SetImage(&gamecubeBtnImg);
	gamecubeBtn.SetImageOver(&gamecubeBtnImgOver);
	gamecubeBtn.SetIcon(&gamecubeBtnIcon);
	gamecubeBtn.SetSoundOver(&btnSoundOver);
	gamecubeBtn.SetSoundClick(&btnSoundClick);
	gamecubeBtn.SetTrigger(trigA);
	gamecubeBtn.SetTrigger(trig2);
	gamecubeBtn.SetEffectGrow();
	
	GuiText wiimoteBtnTxt("Wiimote", 22, (GXColor){0, 0, 0, 255});
	GuiImage wiimoteBtnImg(&btnLargeOutline);
	GuiImage wiimoteBtnImgOver(&btnLargeOutlineOver);
	GuiImage wiimoteBtnIcon(&iconWiimote);
	GuiButton wiimoteBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	wiimoteBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	wiimoteBtn.SetPosition(125, 120);
	wiimoteBtn.SetLabel(&wiimoteBtnTxt);
	wiimoteBtn.SetImage(&wiimoteBtnImg);
	wiimoteBtn.SetImageOver(&wiimoteBtnImgOver);
	wiimoteBtn.SetIcon(&wiimoteBtnIcon);
	wiimoteBtn.SetSoundOver(&btnSoundOver);
	wiimoteBtn.SetSoundClick(&btnSoundClick);
	wiimoteBtn.SetTrigger(trigA);
	wiimoteBtn.SetTrigger(trig2);
	wiimoteBtn.SetEffectGrow();

	GuiText drcBtnTxt("Wii U GamePad", 22, (GXColor){0, 0, 0, 255});
	drcBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage drcBtnImg(&btnLargeOutline);
	GuiImage drcBtnImgOver(&btnLargeOutlineOver);
	GuiImage drcBtnIcon(&iconDrc);
	GuiButton drcBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	drcBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	drcBtn.SetPosition(200, 120);
	drcBtn.SetLabel(&drcBtnTxt);
	drcBtn.SetImage(&drcBtnImg);
	drcBtn.SetImageOver(&drcBtnImgOver);
	drcBtn.SetIcon(&drcBtnIcon);
	drcBtn.SetSoundOver(&btnSoundOver);
	drcBtn.SetSoundClick(&btnSoundClick);
	drcBtn.SetTrigger(trigA);
	drcBtn.SetTrigger(trig2);
	drcBtn.SetEffectGrow();
	
	GuiText classicBtnTxt("Classic Controller", 22, (GXColor){0, 0, 0, 255});
	classicBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage classicBtnImg(&btnLargeOutline);
	GuiImage classicBtnImgOver(&btnLargeOutlineOver);
	GuiImage classicBtnIcon(&iconClassic);
	GuiButton classicBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	classicBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	classicBtn.SetPosition(-200, 250);
	classicBtn.SetLabel(&classicBtnTxt);
	classicBtn.SetImage(&classicBtnImg);
	classicBtn.SetImageOver(&classicBtnImgOver);
	classicBtn.SetIcon(&classicBtnIcon);
	classicBtn.SetSoundOver(&btnSoundOver);
	classicBtn.SetSoundClick(&btnSoundClick);
	classicBtn.SetTrigger(trigA);
	classicBtn.SetTrigger(trig2);
	classicBtn.SetEffectGrow();

	GuiText nunchukBtnTxt1("Wiimote", 22, (GXColor){0, 0, 0, 255});
	GuiText nunchukBtnTxt2("&", 18, (GXColor){0, 0, 0, 255});
	GuiText nunchukBtnTxt3("Nunchuk", 22, (GXColor){0, 0, 0, 255});
	nunchukBtnTxt1.SetPosition(0, -20);
	nunchukBtnTxt3.SetPosition(0, +20);
	GuiImage nunchukBtnImg(&btnLargeOutline);
	GuiImage nunchukBtnImgOver(&btnLargeOutlineOver);
	GuiImage nunchukBtnIcon(&iconNunchuk);
	GuiButton nunchukBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	nunchukBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	nunchukBtn.SetPosition(0, 250);
	nunchukBtn.SetLabel(&nunchukBtnTxt1, 0);
	nunchukBtn.SetLabel(&nunchukBtnTxt2, 1);
	nunchukBtn.SetLabel(&nunchukBtnTxt3, 2);
	nunchukBtn.SetImage(&nunchukBtnImg);
	nunchukBtn.SetImageOver(&nunchukBtnImgOver);
	nunchukBtn.SetIcon(&nunchukBtnIcon);
	nunchukBtn.SetSoundOver(&btnSoundOver);
	nunchukBtn.SetSoundClick(&btnSoundClick);
	nunchukBtn.SetTrigger(trigA);
	nunchukBtn.SetTrigger(trig2);
	nunchukBtn.SetEffectGrow();

	GuiText wiiuproBtnTxt("Wii U Pro Controller", 22, (GXColor){0, 0, 0, 255});
	wiiuproBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-20);
	GuiImage wiiuproBtnImg(&btnLargeOutline);
	GuiImage wiiuproBtnImgOver(&btnLargeOutlineOver);
	GuiImage wiiuproBtnIcon(&iconWiiupro);
	GuiButton wiiuproBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	wiiuproBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	wiiuproBtn.SetPosition(200, 250);
	wiiuproBtn.SetLabel(&wiiuproBtnTxt);
	wiiuproBtn.SetImage(&wiiuproBtnImg);
	wiiuproBtn.SetImageOver(&wiiuproBtnImgOver);
	wiiuproBtn.SetIcon(&wiiuproBtnIcon);
	wiiuproBtn.SetSoundOver(&btnSoundOver);
	wiiuproBtn.SetSoundClick(&btnSoundClick);
	wiiuproBtn.SetTrigger(trigA);
	wiiuproBtn.SetTrigger(trig2);
	wiiuproBtn.SetEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(50, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);

	w.Append(&gamecubeBtn);
#ifdef HW_RVL
	w.Append(&wiimoteBtn);
	
	if(WiiDRC_Inited() && WiiDRC_Connected()) {
		gamecubeBtn.SetPosition(-200, 120);
		wiimoteBtn.SetPosition(0, 120);
		w.Append(&drcBtn);
	}
	
	w.Append(&classicBtn);
	w.Append(&nunchukBtn);
	w.Append(&wiiuproBtn);
#endif
	w.Append(&backBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(wiimoteBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_WIIMOTE;
		}
		else if(nunchukBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_NUNCHUK;
		}
		else if(classicBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_CLASSIC;
		}
		else if(wiiuproBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_WUPC;
		}
		else if(drcBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_WIIDRC;
		}
		else if(gamecubeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS_MAP;
			mapMenuCtrl = CTRLR_GCPAD;
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * ButtonMappingWindow
 ***************************************************************************/
static u32
ButtonMappingWindow()
{
	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt("Button Mapping", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);

	char msg[200];

	switch(mapMenuCtrl)
	{
		case CTRLR_GCPAD:
			#ifdef HW_RVL
			sprintf(msg, "Press any button on the GameCube Controller now. Press Home or the C-Stick in any direction to clear the existing mapping.");
			#else
			sprintf(msg, "Press any button on the GameCube Controller now. Press the C-Stick in any direction to clear the existing mapping.");
			#endif
			break;
		case CTRLR_WIIMOTE:
			sprintf(msg, "Press any button on the Wiimote now. Press Home to clear the existing mapping.");
			break;
		case CTRLR_CLASSIC:
			sprintf(msg, "Press any button on the Classic Controller now. Press Home to clear the existing mapping.");
			break;
		case CTRLR_WUPC:
			sprintf(msg, "Press any button on the Wii U Pro Controller now. Press Home to clear the existing mapping.");
			break;
		case CTRLR_WIIDRC:
			sprintf(msg, "Press any button on the Wii U GamePad now. Press Home to clear the existing mapping.");
			break;
		case CTRLR_NUNCHUK:
			sprintf(msg, "Press any button on the Wiimote or Nunchuk now. Press Home to clear the existing mapping.");
			break;
	}

	GuiText msgTxt(msg, 26, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 430);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	u32 pressed = 0;

	while(pressed == 0)
	{
		usleep(THREAD_SLEEP);

		if(mapMenuCtrl == CTRLR_GCPAD)
		{
			pressed = userInput[0].pad.btns_d;


			if(userInput[0].pad.substickX < -70 ||
					userInput[0].pad.substickX > 70 ||
					userInput[0].pad.substickY < -70 ||
					userInput[0].pad.substickY > 70)
				pressed = WPAD_BUTTON_HOME;

			if(userInput[0].wpad->btns_d == WPAD_BUTTON_HOME)
				pressed = WPAD_BUTTON_HOME;
		}
		else if(mapMenuCtrl == CTRLR_WIIDRC)
		{
			pressed = userInput[0].wiidrcdata.btns_d;
		}
		else
		{
			pressed = userInput[0].wpad->btns_d;

			// always allow Home button to be pressed to cancel
			if(pressed != WPAD_BUTTON_HOME)
			{
				switch(mapMenuCtrl)
				{
					case CTRLR_WIIMOTE:
						if(pressed > 0x1000)
							pressed = 0; // not a valid input
						break;
					case CTRLR_CLASSIC:
						if(userInput[0].wpad->exp.type != WPAD_EXP_CLASSIC && userInput[0].wpad->exp.classic.type < 2)
							pressed = 0; // not a valid input
						else if(pressed <= 0x1000)
							pressed = 0;
						break;
					case CTRLR_WUPC:
						if(userInput[0].wpad->exp.type != WPAD_EXP_CLASSIC && userInput[0].wpad->exp.classic.type == 2)
							pressed = 0; // not a valid input
						else if(pressed <= 0x1000)
							pressed = 0;
						break;
					case CTRLR_NUNCHUK:
						if(userInput[0].wpad->exp.type != WPAD_EXP_NUNCHUK)
							pressed = 0; // not a valid input
						break;
				}
			}
		}
	}

	if(mapMenuCtrl == CTRLR_WIIDRC) {
		if(pressed == WIIDRC_BUTTON_HOME) {
			pressed = 0;
		}
	}
	else if(pressed == WPAD_BUTTON_HOME || pressed == WPAD_CLASSIC_BUTTON_HOME) {
		pressed = 0;
	}

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();

	return pressed;
}

static int MenuSettingsMappingsMap()
{
	int menu = MENU_NONE;
	int ret,i,j;
	bool firstRun = true;
	OptionList options;

	char menuTitle[100];
	char menuSubtitle[100];
	sprintf(menuTitle, "Game Settings - Button Mappings");

	GuiText titleTxt(menuTitle, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,30);

	sprintf(menuSubtitle, "%s", ctrlrName[mapMenuCtrl]);
	GuiText subtitleTxt(menuSubtitle, 20, (GXColor){255, 255, 255, 255});
	subtitleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	subtitleTxt.SetPosition(50,60);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnShortOutline(button_short_png);
	GuiImageData btnShortOutlineOver(button_short_over_png);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(50, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiText resetBtnTxt("Reset Mappings", 22, (GXColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnShortOutline);
	GuiImage resetBtnImgOver(&btnShortOutlineOver);
	GuiButton resetBtn(btnShortOutline.GetWidth(), btnShortOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	resetBtn.SetPosition(260, -35);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetSoundClick(&btnSoundClick);
	resetBtn.SetTrigger(trigA);
	resetBtn.SetTrigger(trig2);
	resetBtn.SetEffectGrow();

	i=0;
	sprintf(options.name[i++], "B");
	sprintf(options.name[i++], "A");
	sprintf(options.name[i++], "Select");
	sprintf(options.name[i++], "Start");
	sprintf(options.name[i++], "Up");
	sprintf(options.name[i++], "Down");
	sprintf(options.name[i++], "Left");
	sprintf(options.name[i++], "Right");
	sprintf(options.name[i++], "L");
	sprintf(options.name[i++], "R");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(215);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	w.Append(&resetBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	mainWindow->Append(&subtitleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS_MAPPINGS;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			resetBtn.ResetState();

			int choice = WindowPrompt(
				"Reset Mappings",
				"Are you sure that you want to reset your mappings?",
				"Yes",
				"No");

			if(choice == 1)
			{
				ResetControls(mapMenuCtrl);
				firstRun = true;
			}
		}

		ret = optionBrowser.GetClickedOption();

		if(ret >= 0)
		{
			// get a button selection from user
			btnmap[mapMenuCtrl][ret] = ButtonMappingWindow();
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			for(i=0; i < options.length; i++)
			{
				for(j=0; j < ctrlr_def[mapMenuCtrl].num_btns; j++)
				{
					if(btnmap[mapMenuCtrl][i] == 0)
					{
						options.value[i][0] = 0;
					}
					else if(btnmap[mapMenuCtrl][i] ==
						ctrlr_def[mapMenuCtrl].map[j].btn)
					{
						if(strcmp(options.value[i], ctrlr_def[mapMenuCtrl].map[j].name) != 0)
							sprintf(options.value[i], ctrlr_def[mapMenuCtrl].map[j].name);
						break;
					}
				}
			}
			optionBrowser.TriggerUpdate();
		}
	}

	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	mainWindow->Remove(&subtitleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsVideo
 ***************************************************************************/

static void ScreenZoomWindowUpdate(void * ptr, float h, float v)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		char zoom[10], zoom2[10];
		
		if(IsGBAGame())
		{
			GCSettings.gbaZoomHor += h;
			GCSettings.gbaZoomVert += v;
			sprintf(zoom, "%.2f%%", GCSettings.gbaZoomHor*100);
			sprintf(zoom2, "%.2f%%", GCSettings.gbaZoomVert*100);
		}
		else
		{
			GCSettings.gbZoomHor += h;
			GCSettings.gbZoomVert += v;
			sprintf(zoom, "%.2f%%", GCSettings.gbZoomHor*100);
			sprintf(zoom2, "%.2f%%", GCSettings.gbZoomVert*100);
		}
		
		settingText->SetText(zoom);
		settingText2->SetText(zoom2);
		b->ResetState();
	}
}

static void ScreenZoomWindowLeftClick(void * ptr) { ScreenZoomWindowUpdate(ptr, -0.01, 0); }
static void ScreenZoomWindowRightClick(void * ptr) { ScreenZoomWindowUpdate(ptr, +0.01, 0); }
static void ScreenZoomWindowUpClick(void * ptr) { ScreenZoomWindowUpdate(ptr, 0, +0.01); }
static void ScreenZoomWindowDownClick(void * ptr) { ScreenZoomWindowUpdate(ptr, 0, -0.01); }

static void ScreenZoomWindow()
{
	GuiWindow * w = new GuiWindow(200,200);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT, WIIDRC_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT, WIIDRC_BUTTON_RIGHT);

	GuiTrigger trigUp;
	trigUp.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP, PAD_BUTTON_UP, WIIDRC_BUTTON_UP);

	GuiTrigger trigDown;
	trigDown.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN, PAD_BUTTON_DOWN, WIIDRC_BUTTON_DOWN);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	arrowLeftBtn.SetPosition(50, 0);
	arrowLeftBtn.SetTrigger(trigA);
	arrowLeftBtn.SetTrigger(trig2);
	arrowLeftBtn.SetTrigger(&trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenZoomWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	arrowRightBtn.SetPosition(164, 0);
	arrowRightBtn.SetTrigger(trigA);
	arrowRightBtn.SetTrigger(trig2);
	arrowRightBtn.SetTrigger(&trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenZoomWindowRightClick);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiButton arrowUpBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	arrowUpBtn.SetImage(&arrowUpImg);
	arrowUpBtn.SetImageOver(&arrowUpOverImg);
	arrowUpBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	arrowUpBtn.SetPosition(-76, -27);
	arrowUpBtn.SetTrigger(trigA);
	arrowUpBtn.SetTrigger(trig2);
	arrowUpBtn.SetTrigger(&trigUp);
	arrowUpBtn.SetSelectable(false);
	arrowUpBtn.SetUpdateCallback(ScreenZoomWindowUpClick);

	GuiImageData arrowDown(button_arrow_down_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(button_arrow_down_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiButton arrowDownBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	arrowDownBtn.SetImage(&arrowDownImg);
	arrowDownBtn.SetImageOver(&arrowDownOverImg);
	arrowDownBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	arrowDownBtn.SetPosition(-76, 27);
	arrowDownBtn.SetTrigger(trigA);
	arrowDownBtn.SetTrigger(trig2);
	arrowDownBtn.SetTrigger(&trigDown);
	arrowDownBtn.SetSelectable(false);
	arrowDownBtn.SetUpdateCallback(ScreenZoomWindowDownClick);

	GuiImageData screenPosition(screen_position_png);
	GuiImage screenPositionImg(&screenPosition);
	screenPositionImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	screenPositionImg.SetPosition(0, 0);

	settingText = new GuiText(NULL, 20, (GXColor){0, 0, 0, 255});
	settingText2 = new GuiText(NULL, 20, (GXColor){0, 0, 0, 255});
	char zoom[10], zoom2[10];
	float currentZoomHor, currentZoomVert;
	
	if(IsGBAGame())
	{
		sprintf(zoom, "%.2f%%", GCSettings.gbaZoomHor*100);
		sprintf(zoom2, "%.2f%%", GCSettings.gbaZoomVert*100);
		currentZoomHor = GCSettings.gbaZoomHor;
		currentZoomVert = GCSettings.gbaZoomVert;
	}
	else
	{
		sprintf(zoom, "%.2f%%", GCSettings.gbZoomHor*100);
		sprintf(zoom2, "%.2f%%", GCSettings.gbZoomVert*100);
		currentZoomHor = GCSettings.gbZoomHor;
		currentZoomVert = GCSettings.gbZoomVert;
	}

	settingText->SetText(zoom);
	settingText->SetPosition(108, 0);
	settingText2->SetText(zoom2);
	settingText2->SetPosition(-76, 0);

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(&arrowUpBtn);
	w->Append(&arrowDownBtn);
	w->Append(&screenPositionImg);
	w->Append(settingText);
	w->Append(settingText2);
	
	char windowName[20];
	if(IsGBAGame())
		sprintf(windowName, "GBA Screen Zoom");
	else
		sprintf(windowName, "GB Screen Zoom");

	if(!SettingWindow(windowName,w))
	{
		// undo changes
		if(IsGBAGame())
		{
			GCSettings.gbaZoomHor = currentZoomHor;
			GCSettings.gbaZoomVert = currentZoomVert;
		}
		else
		{
			GCSettings.gbZoomHor = currentZoomHor;
			GCSettings.gbZoomVert = currentZoomVert;
		}
	}

	delete(w);
	delete(settingText);
	delete(settingText2);
}

static void ScreenPositionWindowUpdate(void * ptr, int x, int y)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		GCSettings.xshift += x;
		GCSettings.yshift += y;

		if(!(GCSettings.xshift > -50 && GCSettings.xshift < 50))
			GCSettings.xshift = 0;
		if(!(GCSettings.yshift > -50 && GCSettings.yshift < 50))
			GCSettings.yshift = 0;

		char shift[10];
		sprintf(shift, "%hd, %hd", GCSettings.xshift, GCSettings.yshift);
		settingText->SetText(shift);
		b->ResetState();
	}
}

static void ScreenPositionWindowLeftClick(void * ptr) { ScreenPositionWindowUpdate(ptr, -1, 0); }
static void ScreenPositionWindowRightClick(void * ptr) { ScreenPositionWindowUpdate(ptr, +1, 0); }
static void ScreenPositionWindowUpClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, -1); }
static void ScreenPositionWindowDownClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, +1); }

static void ScreenPositionWindow()
{
	GuiWindow * w = new GuiWindow(150,150);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	w->SetPosition(0, -10);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT, WIIDRC_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT, WIIDRC_BUTTON_RIGHT);

	GuiTrigger trigUp;
	trigUp.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP, PAD_BUTTON_UP, WIIDRC_BUTTON_UP);

	GuiTrigger trigDown;
	trigDown.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN, PAD_BUTTON_DOWN, WIIDRC_BUTTON_DOWN);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(trigA);
	arrowLeftBtn.SetTrigger(trig2);
	arrowLeftBtn.SetTrigger(&trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenPositionWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(trigA);
	arrowRightBtn.SetTrigger(trig2);
	arrowRightBtn.SetTrigger(&trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenPositionWindowRightClick);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiButton arrowUpBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	arrowUpBtn.SetImage(&arrowUpImg);
	arrowUpBtn.SetImageOver(&arrowUpOverImg);
	arrowUpBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	arrowUpBtn.SetTrigger(trigA);
	arrowUpBtn.SetTrigger(trig2);
	arrowUpBtn.SetTrigger(&trigUp);
	arrowUpBtn.SetSelectable(false);
	arrowUpBtn.SetUpdateCallback(ScreenPositionWindowUpClick);

	GuiImageData arrowDown(button_arrow_down_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(button_arrow_down_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiButton arrowDownBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	arrowDownBtn.SetImage(&arrowDownImg);
	arrowDownBtn.SetImageOver(&arrowDownOverImg);
	arrowDownBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	arrowDownBtn.SetTrigger(trigA);
	arrowDownBtn.SetTrigger(trig2);
	arrowDownBtn.SetTrigger(&trigDown);
	arrowDownBtn.SetSelectable(false);
	arrowDownBtn.SetUpdateCallback(ScreenPositionWindowDownClick);

	GuiImageData screenPosition(screen_position_png);
	GuiImage screenPositionImg(&screenPosition);
	screenPositionImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	settingText = new GuiText(NULL, 20, (GXColor){0, 0, 0, 255});
	char shift[10];
	sprintf(shift, "%i, %i", GCSettings.xshift, GCSettings.yshift);
	settingText->SetText(shift);

	int currentX = GCSettings.xshift;
	int currentY = GCSettings.yshift;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(&arrowUpBtn);
	w->Append(&arrowDownBtn);
	w->Append(&screenPositionImg);
	w->Append(settingText);

	if(!SettingWindow("Screen Position",w))
	{
		// undo changes
		GCSettings.xshift = currentX;
		GCSettings.yshift = currentY;
	}

	delete(w);
	delete(settingText);
}

static int MenuSettingsVideo()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Rendering");
	sprintf(options.name[i++], "Scaling");
	if(IsGBAGame()) {
		sprintf(options.name[i++], "GBA Screen Zoom");
		sprintf(options.name[i++], "GBA Fixed Pixel Ratio");
	} else {
		sprintf(options.name[i++], "GB Screen Zoom");
		sprintf(options.name[i++], "GB Fixed Pixel Ratio");
	}
	sprintf(options.name[i++], "Screen Position");
	sprintf(options.name[i++], "Video Mode");
	sprintf(options.name[i++], "GB Mono Colorization");
	sprintf(options.name[i++], "GB Palette");
	sprintf(options.name[i++], "GBA Frameskip");
	sprintf(options.name[i++], "Enable Turbo Mode");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;
	
	if(IsGBAGame())
		options.name[6][0] = 0;

	if(!IsGameboyGame())
		options.name[7][0] = 0; // disable palette option for GBA/GBC

	GuiText titleTxt("Game Settings - Video", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(50, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetCol2Position(240);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.render++;
				if (GCSettings.render > 4)
					GCSettings.render = 1;
				break;

			case 1:
				GCSettings.scaling++;
				if (GCSettings.scaling > 3)
					GCSettings.scaling = 0;
				// disable Widescreen correction in Wii mode - determined automatically
				#ifdef HW_RVL
				if(GCSettings.scaling == 3)
					GCSettings.scaling = 0;
				#endif
				break;

			case 2:
				ScreenZoomWindow();
				break;

			case 3:
				if(IsGBAGame()) {
					GCSettings.gbaFixed++;
					if(GCSettings.gbaFixed > 3)
						GCSettings.gbaFixed = 0;
				} else {
					GCSettings.gbFixed++;
					if(GCSettings.gbFixed > 3)
						GCSettings.gbFixed = 0;
				}
				break;

			case 4:
				ScreenPositionWindow();
				break;

			case 5:
				GCSettings.videomode++;
				if(GCSettings.videomode > 6)
					GCSettings.videomode = 0;
				break;

			case 6:
				GCSettings.colorize ^= 1;
				break;

			case 7:
				menu = MENU_GAMESETTINGS_PALETTE;
				break;

			case 8:
				GCSettings.gbaFrameskip ^= 1;
				break;
			case 9:
				GCSettings.TurboModeEnabled ^= 1;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			if (GCSettings.render == 0)
				sprintf (options.value[0], "Original");
			else if (GCSettings.render == 1)
				sprintf (options.value[0], "Filtered (Auto)");
			else if (GCSettings.render == 2)
				sprintf (options.value[0], "Unfiltered");
			else if (GCSettings.render == 3)
				sprintf (options.value[0], "Filtered (Sharp)");
			else if (GCSettings.render == 4)
				sprintf (options.value[0], "Filtered (Soft)");

			if (GCSettings.scaling == 0)
				sprintf (options.value[1], "Maintain Aspect Ratio");
			else if (GCSettings.scaling == 1)
				sprintf (options.value[1], "Partial Stretch");
			else if (GCSettings.scaling == 2)
				sprintf (options.value[1], "Stretch to Fit");
			else if (GCSettings.scaling == 3)
				sprintf (options.value[1], "16:9 Correction");

			int fixed;
			if(IsGBAGame()) {
				sprintf (options.value[2], "%.2f%%, %.2f%%", GCSettings.gbaZoomHor*100, GCSettings.gbaZoomVert*100);
				fixed = GCSettings.gbaFixed;
			} else {
				sprintf (options.value[2], "%.2f%%, %.2f%%", GCSettings.gbZoomHor*100, GCSettings.gbZoomVert*100);
				fixed = GCSettings.gbFixed;
			}

			if (fixed) {
				int w = fixed / 10;
				int ratio = fixed % 10;
				const char* widescreen = w
					? "(16:9 Correction)"
					: "";
				
				sprintf (options.value[3], "%dx %s", ratio, widescreen);
			} else {
				sprintf (options.value[3], "Disabled");
			}

			sprintf (options.value[4], "%d, %d", GCSettings.xshift, GCSettings.yshift);

			switch(GCSettings.videomode)
			{
				case 0:
					sprintf (options.value[5], "Automatic (Recommended)"); break;
				case 1:
					sprintf (options.value[5], "NTSC (480i)"); break;
				case 2:
					sprintf (options.value[5], "NTSC (480p)"); break;
				case 3:
					sprintf (options.value[5], "PAL (576i)"); break;
				case 4:
					sprintf (options.value[5], "European RGB (240i)"); break;
				case 5:
					sprintf (options.value[5], "NTSC (240p)"); break;
				case 6:
					sprintf (options.value[5], "European RGB (240p)"); break;
			}

			if (GCSettings.colorize)
				sprintf (options.value[6], "On");
			else
				sprintf (options.value[6], "Off");

			if(strcmp(CurrentPalette.gameName, "default"))
				sprintf(options.value[7], "Custom");
			else
				sprintf(options.value[7], "Default");

			if (GCSettings.gbaFrameskip)
				sprintf (options.value[8], "On");
			else
				sprintf (options.value[8], "Off");
			sprintf (options.value[9], "%s", GCSettings.TurboModeEnabled == 1 ? "On" : "Off");
			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

static int MenuSettingsEmulation()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Hardware (GB/GBC)");
	sprintf(options.name[i++], "Super Game Boy border");
	sprintf(options.name[i++], "Offset from UTC (hours)");
	sprintf(options.name[i++], "GB Screen Palette");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;
	
	GuiText titleTxt("Game Settings - Emulation", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(50, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetCol2Position(240);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.GBHardware++;
				if (GCSettings.GBHardware > 5)
					GCSettings.GBHardware = 0;
				break;
			
			case 1:
				GCSettings.SGBBorder++;
				if (GCSettings.SGBBorder > 2)
					GCSettings.SGBBorder = 0;
				break;
			
			case 2:
				GCSettings.OffsetMinutesUTC += 15;
				if (GCSettings.OffsetMinutesUTC > 60*14) {
					GCSettings.OffsetMinutesUTC = -60*12;
				}
				break;
			case 3:
				GCSettings.BasicPalette ^= 1;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			if (GCSettings.GBHardware == 0)
				sprintf (options.value[0], "Auto");
			else if (GCSettings.GBHardware == 1)
				sprintf (options.value[0], "Game Boy Color");
			else if (GCSettings.GBHardware == 2)
				sprintf (options.value[0], "Super Game Boy");
			else if (GCSettings.GBHardware == 3)
				sprintf (options.value[0], "Game Boy");
			else if (GCSettings.GBHardware == 4)
				sprintf (options.value[0], "Game Boy Advance");
			else if (GCSettings.GBHardware == 5)
				sprintf (options.value[0], "Super Game Boy 2");
			
			if (GCSettings.SGBBorder == 0)
				sprintf (options.value[1], "Off");
			else if (GCSettings.SGBBorder == 1)
				sprintf (options.value[1], "From game (SGB only)");
			else if (GCSettings.SGBBorder == 2)
				sprintf (options.value[1], "From .png file");
			
			sprintf (options.value[2], "%+.2f", GCSettings.OffsetMinutesUTC / 60.0);

			if (GCSettings.BasicPalette == 0)
				sprintf (options.value[3], "Green Screen");
			else
				sprintf (options.value[3], "Monochrome Screen");
			
			
			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	InitialisePalette();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettings
 ***************************************************************************/
static int MenuSettings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Settings", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData iconFile(icon_settings_file_png);
	GuiImageData iconMenu(icon_settings_menu_png);
	GuiImageData iconNetwork(icon_settings_network_png);
	GuiImageData iconEmulation(icon_game_settings_png);

	GuiText savingBtnTxt1("Saving", 22, (GXColor){0, 0, 0, 255});
	GuiText savingBtnTxt2("&", 18, (GXColor){0, 0, 0, 255});
	GuiText savingBtnTxt3("Loading", 22, (GXColor){0, 0, 0, 255});
	savingBtnTxt1.SetPosition(0, -20);
	savingBtnTxt3.SetPosition(0, +20);
	GuiImage savingBtnImg(&btnLargeOutline);
	GuiImage savingBtnImgOver(&btnLargeOutlineOver);
	GuiImage fileBtnIcon(&iconFile);
	GuiButton savingBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	savingBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	savingBtn.SetPosition(-125, 120);
	savingBtn.SetLabel(&savingBtnTxt1, 0);
	savingBtn.SetLabel(&savingBtnTxt2, 1);
	savingBtn.SetLabel(&savingBtnTxt3, 2);
	savingBtn.SetImage(&savingBtnImg);
	savingBtn.SetImageOver(&savingBtnImgOver);
	savingBtn.SetIcon(&fileBtnIcon);
	savingBtn.SetSoundOver(&btnSoundOver);
	savingBtn.SetSoundClick(&btnSoundClick);
	savingBtn.SetTrigger(trigA);
	savingBtn.SetTrigger(trig2);
	savingBtn.SetEffectGrow();

	GuiText menuBtnTxt("Menu", 22, (GXColor){0, 0, 0, 255});
	menuBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage menuBtnImg(&btnLargeOutline);
	GuiImage menuBtnImgOver(&btnLargeOutlineOver);
	GuiImage menuBtnIcon(&iconMenu);
	GuiButton menuBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	menuBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	menuBtn.SetPosition(125, 120);
	menuBtn.SetLabel(&menuBtnTxt);
	menuBtn.SetImage(&menuBtnImg);
	menuBtn.SetImageOver(&menuBtnImgOver);
	menuBtn.SetIcon(&menuBtnIcon);
	menuBtn.SetSoundOver(&btnSoundOver);
	menuBtn.SetSoundClick(&btnSoundClick);
	menuBtn.SetTrigger(trigA);
	menuBtn.SetTrigger(trig2);
	menuBtn.SetEffectGrow();

	GuiText networkBtnTxt("Network", 22, (GXColor){0, 0, 0, 255});
	networkBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage networkBtnImg(&btnLargeOutline);
	GuiImage networkBtnImgOver(&btnLargeOutlineOver);
	GuiImage networkBtnIcon(&iconNetwork);
	GuiButton networkBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	networkBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	networkBtn.SetPosition(-125, 250);
	networkBtn.SetLabel(&networkBtnTxt);
	networkBtn.SetImage(&networkBtnImg);
	networkBtn.SetImageOver(&networkBtnImgOver);
	networkBtn.SetIcon(&networkBtnIcon);
	networkBtn.SetSoundOver(&btnSoundOver);
	networkBtn.SetSoundClick(&btnSoundClick);
	networkBtn.SetTrigger(trigA);
	networkBtn.SetTrigger(trig2);
	networkBtn.SetEffectGrow();

	GuiText emulationBtnTxt("Emulation", 22, (GXColor){0, 0, 0, 255});
	GuiImage emulationBtnImg(&btnLargeOutline);
	GuiImage emulationBtnImgOver(&btnLargeOutlineOver);
	GuiImage emulationBtnIcon(&iconEmulation);
	GuiButton emulationBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	emulationBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	emulationBtn.SetPosition(125, 250);
	emulationBtn.SetLabel(&emulationBtnTxt);
	emulationBtn.SetImage(&emulationBtnImg);
	emulationBtn.SetImageOver(&emulationBtnImgOver);
	emulationBtn.SetIcon(&emulationBtnIcon);
	emulationBtn.SetSoundOver(&btnSoundOver);
	emulationBtn.SetSoundClick(&btnSoundClick);
	emulationBtn.SetTrigger(trigA);
	emulationBtn.SetTrigger(trig2);
	emulationBtn.SetEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(90, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiText resetBtnTxt("Reset Settings", 22, (GXColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnOutline);
	GuiImage resetBtnImgOver(&btnOutlineOver);
	GuiButton resetBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	resetBtn.SetPosition(-90, -35);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetSoundClick(&btnSoundClick);
	resetBtn.SetTrigger(trigA);
	resetBtn.SetTrigger(trig2);
	resetBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&savingBtn);
	w.Append(&menuBtn);
	w.Append(&networkBtn);
	w.Append(&emulationBtn);
	w.Append(&backBtn);
	w.Append(&resetBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(savingBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_FILE;
		}
		else if(menuBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_MENU;
		}
		else if(networkBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_NETWORK;
		}
		else if(emulationBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS_EMULATION;
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_GAMESELECTION;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			resetBtn.ResetState();

			int choice = WindowPrompt(
				"Reset Settings",
				"Are you sure that you want to reset your settings?",
				"Yes",
				"No");

			if(choice == 1)
				DefaultSettings();
		}
	}

	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MenuSettingsFile
 ***************************************************************************/

static int MenuSettingsFile()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	sprintf(options.name[i++], "Load Device");
	sprintf(options.name[i++], "Save Device");
	sprintf(options.name[i++], "Load Folder");
	sprintf(options.name[i++], "Save Folder");
	sprintf(options.name[i++], "Screenshots Folder");
	sprintf(options.name[i++], "Covers Folder");
	sprintf(options.name[i++], "Artwork Folder");
	sprintf(options.name[i++], "Auto Load");
	sprintf(options.name[i++], "Auto Save");
	sprintf(options.name[i++], "Append Auto to .SAV Files");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Settings - Saving & Loading", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(90, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(215);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.LoadMethod++;
				break;

			case 1:
				GCSettings.SaveMethod++;
				break;

			case 2:
				OnScreenKeyboard(GCSettings.LoadFolder, MAXPATHLEN);
				break;

			case 3:
				OnScreenKeyboard(GCSettings.SaveFolder, MAXPATHLEN);
				break;

			case 4:
				OnScreenKeyboard(GCSettings.ScreenshotsFolder, MAXPATHLEN);
				break;

			case 5:
				OnScreenKeyboard(GCSettings.CoverFolder, MAXPATHLEN);
				break;

			case 6:
				OnScreenKeyboard(GCSettings.ArtworkFolder, MAXPATHLEN);
				break;
			
			case 7:
				GCSettings.AutoLoad++;
				if (GCSettings.AutoLoad > 2)
					GCSettings.AutoLoad = 0;
				break;

			case 8:
				GCSettings.AutoSave++;
				if (GCSettings.AutoSave > 3)
					GCSettings.AutoSave = 0;
				break;

			case 9:
				GCSettings.AppendAuto++;
				if (GCSettings.AppendAuto > 1)
					GCSettings.AppendAuto = 0;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			// some load/save methods are not implemented - here's where we skip them
			// they need to be skipped in the order they were enumerated

			// no SD/USB ports on GameCube
			#ifdef HW_DOL
			if(GCSettings.LoadMethod == DEVICE_SD)
				GCSettings.LoadMethod++;
			if(GCSettings.SaveMethod == DEVICE_SD)
				GCSettings.SaveMethod++;
			if(GCSettings.LoadMethod == DEVICE_USB)
				GCSettings.LoadMethod++;
			if(GCSettings.SaveMethod == DEVICE_USB)
				GCSettings.SaveMethod++;
			#endif

			// saving to DVD is impossible
			if(GCSettings.SaveMethod == DEVICE_DVD)
				GCSettings.SaveMethod++;

			// don't allow SD Gecko on Wii
			#ifdef HW_RVL
			if(GCSettings.LoadMethod == DEVICE_SD_SLOTA)
				GCSettings.LoadMethod++;
			if(GCSettings.SaveMethod == DEVICE_SD_SLOTA)
				GCSettings.SaveMethod++;
			if(GCSettings.LoadMethod == DEVICE_SD_SLOTB)
				GCSettings.LoadMethod++;
			if(GCSettings.SaveMethod == DEVICE_SD_SLOTB)
				GCSettings.SaveMethod++;
			if(GCSettings.LoadMethod == DEVICE_SD_PORT2)
				GCSettings.LoadMethod++;
			if(GCSettings.SaveMethod == DEVICE_SD_PORT2)
				GCSettings.SaveMethod++;
			#endif

			// correct load/save methods out of bounds
			if(GCSettings.LoadMethod > 7)
				GCSettings.LoadMethod = 0;
			if(GCSettings.SaveMethod > 7)
				GCSettings.SaveMethod = 0;

			if (GCSettings.LoadMethod == DEVICE_AUTO) sprintf (options.value[0],"Auto Detect");
			else if (GCSettings.LoadMethod == DEVICE_SD) sprintf (options.value[0],"SD");
			else if (GCSettings.LoadMethod == DEVICE_USB) sprintf (options.value[0],"USB");
			else if (GCSettings.LoadMethod == DEVICE_DVD) sprintf (options.value[0],"DVD");
			else if (GCSettings.LoadMethod == DEVICE_SMB) sprintf (options.value[0],"Network");
			else if (GCSettings.LoadMethod == DEVICE_SD_SLOTA) sprintf (options.value[0],"SD Gecko Slot A");
			else if (GCSettings.LoadMethod == DEVICE_SD_SLOTB) sprintf (options.value[0],"SD Gecko Slot B");
			else if (GCSettings.LoadMethod == DEVICE_SD_PORT2) sprintf (options.value[0],"SD in SP2");

			if (GCSettings.SaveMethod == DEVICE_AUTO) sprintf (options.value[1],"Auto Detect");
			else if (GCSettings.SaveMethod == DEVICE_SD) sprintf (options.value[1],"SD");
			else if (GCSettings.SaveMethod == DEVICE_USB) sprintf (options.value[1],"USB");
			else if (GCSettings.SaveMethod == DEVICE_SMB) sprintf (options.value[1],"Network");
			else if (GCSettings.SaveMethod == DEVICE_SD_SLOTA) sprintf (options.value[1],"SD Gecko Slot A");
			else if (GCSettings.SaveMethod == DEVICE_SD_SLOTB) sprintf (options.value[1],"SD Gecko Slot B");
			else if (GCSettings.SaveMethod == DEVICE_SD_PORT2) sprintf (options.value[1],"SD in SP2");

			snprintf (options.value[2], 35, "%s", GCSettings.LoadFolder);
			snprintf (options.value[3], 35, "%s", GCSettings.SaveFolder);
			snprintf (options.value[4], 35, "%s", GCSettings.ScreenshotsFolder);
			snprintf (options.value[5], 35, "%s", GCSettings.CoverFolder);
			snprintf (options.value[6], 35, "%s", GCSettings.ArtworkFolder);
			
			if (GCSettings.AutoLoad == 0) sprintf (options.value[7],"Off");
			else if (GCSettings.AutoLoad == 1) sprintf (options.value[7],"SRAM");
			else if (GCSettings.AutoLoad == 2) sprintf (options.value[7],"State");

			if (GCSettings.AutoSave == 0) sprintf (options.value[8],"Off");
			else if (GCSettings.AutoSave == 1) sprintf (options.value[8],"SRAM");
			else if (GCSettings.AutoSave == 2) sprintf (options.value[8],"State");
			else if (GCSettings.AutoSave == 3) sprintf (options.value[8],"Both");

			if (GCSettings.AppendAuto == 0) sprintf (options.value[9],"Off");
			else if (GCSettings.AppendAuto == 1) sprintf (options.value[9],"On");

			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsMenu
 ***************************************************************************/
static int MenuSettingsMenu()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	currentLanguage = GCSettings.language;

	sprintf(options.name[i++], "Exit Action");
	sprintf(options.name[i++], "Wiimote Orientation");
	sprintf(options.name[i++], "Music Volume");
	sprintf(options.name[i++], "Sound Effects Volume");
	sprintf(options.name[i++], "Rumble");
	sprintf(options.name[i++], "Language");
	sprintf(options.name[i++], "Preview Image");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Settings - Menu", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(90, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(275);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				GCSettings.ExitAction++;
				if(GCSettings.ExitAction > 3)
					GCSettings.ExitAction = 0;
				break;
			case 1:
				GCSettings.WiimoteOrientation ^= 1;
				break;
			case 2:
				GCSettings.MusicVolume += 10;
				if(GCSettings.MusicVolume > 100)
					GCSettings.MusicVolume = 0;
				bgMusic->SetVolume(GCSettings.MusicVolume);
				break;
			case 3:
				GCSettings.SFXVolume += 10;
				if(GCSettings.SFXVolume > 100)
					GCSettings.SFXVolume = 0;
				break;
			case 4:
				GCSettings.Rumble ^= 1;
				break;
			case 5:
				GCSettings.language++;
				
				if(GCSettings.language == LANG_TRAD_CHINESE) // skip (not supported)
					GCSettings.language = LANG_KOREAN;
				else if(GCSettings.language >= LANG_LENGTH)
					GCSettings.language = LANG_JAPANESE;
				break;			
			case 6:
				GCSettings.PreviewImage++;
				if(GCSettings.PreviewImage > 2)
					GCSettings.PreviewImage = 0;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			#ifdef HW_RVL
			if (GCSettings.ExitAction == 1)
				sprintf (options.value[0], "Return to Wii Menu");
			else if (GCSettings.ExitAction == 2)
				sprintf (options.value[0], "Power off Wii");
			else if (GCSettings.ExitAction == 3)
				sprintf (options.value[0], "Return to Loader");
			else
				sprintf (options.value[0], "Auto");
			#else // GameCube
			if(GCSettings.ExitAction > 1)
				GCSettings.ExitAction = 0;
			if (GCSettings.ExitAction == 0)
				sprintf (options.value[0], "Return to Loader");
			else
				sprintf (options.value[0], "Reboot");

			options.name[1][0] = 0; // Wiimote
			options.name[2][0] = 0; // Music
			options.name[3][0] = 0; // Sound Effects
			options.name[4][0] = 0; // Rumble
			#endif

			if (GCSettings.WiimoteOrientation == 0)
				sprintf (options.value[1], "Vertical");
			else if (GCSettings.WiimoteOrientation == 1)
				sprintf (options.value[1], "Horizontal");

			if(GCSettings.MusicVolume > 0)
				sprintf(options.value[2], "%d%%", GCSettings.MusicVolume);
			else
				sprintf(options.value[2], "Mute");

			if(GCSettings.SFXVolume > 0)
				sprintf(options.value[3], "%d%%", GCSettings.SFXVolume);
			else
				sprintf(options.value[3], "Mute");

			if (GCSettings.Rumble == 1)
				sprintf (options.value[4], "Enabled");
			else
				sprintf (options.value[4], "Disabled");

			switch(GCSettings.language)
			{
				case LANG_JAPANESE:		sprintf(options.value[5], "Japanese"); break;
				case LANG_ENGLISH:		sprintf(options.value[5], "English"); break;
				case LANG_GERMAN:		sprintf(options.value[5], "German"); break;
				case LANG_FRENCH:		sprintf(options.value[5], "French"); break;
				case LANG_SPANISH:		sprintf(options.value[5], "Spanish"); break;
				case LANG_ITALIAN:		sprintf(options.value[5], "Italian"); break;
				case LANG_DUTCH:		sprintf(options.value[5], "Dutch"); break;
				case LANG_SIMP_CHINESE:	sprintf(options.value[5], "Chinese (Simplified)"); break;
				case LANG_TRAD_CHINESE:	sprintf(options.value[5], "Chinese (Traditional)"); break;
				case LANG_KOREAN:		sprintf(options.value[5], "Korean"); break;
				case LANG_PORTUGUESE:	sprintf(options.value[5], "Portuguese"); break;
				case LANG_BRAZILIAN_PORTUGUESE: sprintf(options.value[5], "Brazilian Portuguese"); break;
				case LANG_CATALAN:		sprintf(options.value[5], "Catalan"); break;
				case LANG_TURKISH:		sprintf(options.value[5], "Turkish"); break;
			}
	
			switch(GCSettings.PreviewImage)
			{
				case 0:	
					sprintf(options.value[6], "Screenshots"); 
					break; 
				case 1:	
					sprintf(options.value[6], "Covers");	  
					break; 
				case 2:	
					sprintf(options.value[6], "Artwork");
					break; 
			}
			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	ChangeLanguage();
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuSettingsNetwork
 ***************************************************************************/
static int MenuSettingsNetwork()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	sprintf(options.name[i++], "SMB Share IP");
	sprintf(options.name[i++], "SMB Share Name");
	sprintf(options.name[i++], "SMB Share Username");
	sprintf(options.name[i++], "SMB Share Password");
	options.length = i;

	for(i=0; i < options.length; i++)
		options.value[i][0] = 0;

	GuiText titleTxt("Settings - Network", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_long_png);
	GuiImageData btnOutlineOver(button_long_over_png);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(90, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(290);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				OnScreenKeyboard(GCSettings.smbip, 16);
				break;

			case 1:
				OnScreenKeyboard(GCSettings.smbshare, 20);
				break;

			case 2:
				OnScreenKeyboard(GCSettings.smbuser, 20);
				break;

			case 3:
				OnScreenKeyboard(GCSettings.smbpwd, 20);
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			snprintf (options.value[0], 25, "%s", GCSettings.smbip);
			snprintf (options.value[1], 19, "%s", GCSettings.smbshare);
			snprintf (options.value[2], 19, "%s", GCSettings.smbuser);
			snprintf (options.value[3], 19, "%s", GCSettings.smbpwd);
			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	CloseShare();
	return menu;
}


static int redAmount=128, greenAmount=128, blueAmount=128;
static GuiText *redText;
static GuiText *greenText;
static GuiText *blueText;
static GuiText *sampleText;

static void RGBWindowUpdate(void * ptr, int red, int green, int blue)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		redAmount += red;
		if (redAmount>255) redAmount=255;
		else if (redAmount<0) redAmount=0;
		greenAmount += green;
		if (greenAmount>255) greenAmount=255;
		else if (greenAmount<0) greenAmount=0;
		blueAmount += blue;
		if (blueAmount>255) blueAmount=255;
		else if (blueAmount<0) blueAmount=0;

		redText->SetColor((GXColor){(u8)redAmount, 0, 0, 0xFF});
		greenText->SetColor((GXColor){0, (u8)greenAmount, 0, 0xFF});
		blueText->SetColor((GXColor){0, 0, (u8)blueAmount, 0xFF});
		sampleText->SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});

		char shift[10];
		sprintf(shift, "%2x", redAmount);
		redText->SetText(shift);
		sprintf(shift, "%2x", greenAmount);
		greenText->SetText(shift);
		sprintf(shift, "%2x", blueAmount);
		blueText->SetText(shift);

		b->ResetState();
	}
}

static void LessRedClick(void * ptr) { RGBWindowUpdate(ptr, -8, 0, 0); }
static void LessGreenClick(void * ptr) { RGBWindowUpdate(ptr, 0, -8, 0); }
static void LessBlueClick(void * ptr) { RGBWindowUpdate(ptr, 0, 0, -8); }
static void MoreRedClick(void * ptr) { RGBWindowUpdate(ptr, +8, 0, 0); }
static void MoreGreenClick(void * ptr) { RGBWindowUpdate(ptr, 0, +8, 0); }
static void MoreBlueClick(void * ptr) { RGBWindowUpdate(ptr, 0, 0, +8); }

static void PaletteWindow(const char *name)
{
	GuiWindow * w = new GuiWindow(500,480);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	w->SetPosition(0, -10);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImageData arrowDown(button_arrow_down_png);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImageData arrowDownOver(button_arrow_down_over_png);

	GuiImage moreRedImg(&arrowUp);
	GuiImage moreRedOverImg(&arrowUpOver);
	GuiButton moreRedBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	moreRedBtn.SetImage(&moreRedImg);
	moreRedBtn.SetImageOver(&moreRedOverImg);
	moreRedBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	moreRedBtn.SetPosition(-150,-60);
	moreRedBtn.SetTrigger(trigA);
	moreRedBtn.SetTrigger(trig2);
	moreRedBtn.SetSelectable(true);
	moreRedBtn.SetUpdateCallback(MoreRedClick);

	GuiImage lessRedImg(&arrowDown);
	GuiImage lessRedOverImg(&arrowDownOver);
	GuiButton lessRedBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	lessRedBtn.SetImage(&lessRedImg);
	lessRedBtn.SetImageOver(&lessRedOverImg);
	lessRedBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	lessRedBtn.SetPosition(-150,+50);
	lessRedBtn.SetTrigger(trigA);
	lessRedBtn.SetTrigger(trig2);
	lessRedBtn.SetSelectable(true);
	lessRedBtn.SetUpdateCallback(LessRedClick);

	GuiImage moreGreenImg(&arrowUp);
	GuiImage moreGreenOverImg(&arrowUpOver);
	GuiButton moreGreenBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	moreGreenBtn.SetImage(&moreGreenImg);
	moreGreenBtn.SetImageOver(&moreGreenOverImg);
	moreGreenBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	moreGreenBtn.SetPosition(-50,-60);
	moreGreenBtn.SetTrigger(trigA);
	moreGreenBtn.SetTrigger(trig2);
	moreGreenBtn.SetSelectable(true);
	moreGreenBtn.SetUpdateCallback(MoreGreenClick);

	GuiImage lessGreenImg(&arrowDown);
	GuiImage lessGreenOverImg(&arrowDownOver);
	GuiButton lessGreenBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	lessGreenBtn.SetImage(&lessGreenImg);
	lessGreenBtn.SetImageOver(&lessGreenOverImg);
	lessGreenBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	lessGreenBtn.SetPosition(-50,+50);
	lessGreenBtn.SetTrigger(trigA);
	lessGreenBtn.SetTrigger(trig2);
	lessGreenBtn.SetSelectable(true);
	lessGreenBtn.SetUpdateCallback(LessGreenClick);

	GuiImage moreBlueImg(&arrowUp);
	GuiImage moreBlueOverImg(&arrowUpOver);
	GuiButton moreBlueBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	moreBlueBtn.SetImage(&moreBlueImg);
	moreBlueBtn.SetImageOver(&moreBlueOverImg);
	moreBlueBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	moreBlueBtn.SetPosition(50,-60);
	moreBlueBtn.SetTrigger(trigA);
	moreBlueBtn.SetTrigger(trig2);
	moreBlueBtn.SetSelectable(true);
	moreBlueBtn.SetUpdateCallback(MoreBlueClick);

	GuiImage lessBlueImg(&arrowDown);
	GuiImage lessBlueOverImg(&arrowDownOver);
	GuiButton lessBlueBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	lessBlueBtn.SetImage(&lessBlueImg);
	lessBlueBtn.SetImageOver(&lessBlueOverImg);
	lessBlueBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	lessBlueBtn.SetPosition(50,+50);
	lessBlueBtn.SetTrigger(trigA);
	lessBlueBtn.SetTrigger(trig2);
	lessBlueBtn.SetSelectable(true);
	lessBlueBtn.SetUpdateCallback(LessBlueClick);

	GuiImageData box(screen_position_png);

	GuiImage redBoxImg(&box);
	redBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	redBoxImg.SetPosition(-150, 0);

	GuiImage greenBoxImg(&box);
	greenBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	greenBoxImg.SetPosition(-50, 0);

	GuiImage blueBoxImg(&box);
	blueBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	blueBoxImg.SetPosition(+50, 0);

	char shift[10];
	redText = new GuiText(NULL, 20, (GXColor){128, 0, 0, 255});
	redText->SetPosition(-150,0);
	sprintf(shift, "%2x", redAmount);
	redText->SetText(shift);

	greenText = new GuiText(NULL, 20, (GXColor){0, 128, 0, 255});
	greenText->SetPosition(-50, 0);
	sprintf(shift, "%2x", greenAmount);
	greenText->SetText(shift);

	blueText = new GuiText(NULL, 20, (GXColor){0, 0, 128, 255});
	blueText->SetPosition(+50,0);
	sprintf(shift, "%2x", blueAmount);
	blueText->SetText(shift);

	sampleText = new GuiText(NULL, 20, (GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
	sampleText->SetPosition(+150,0);
	sampleText->SetText(name);

	int currentRed = redAmount;
	int currentGreen = greenAmount;
	int currentBlue = blueAmount;

	w->Append(&lessRedBtn);
	w->Append(&moreRedBtn);
	w->Append(&lessGreenBtn);
	w->Append(&moreGreenBtn);
	w->Append(&lessBlueBtn);
	w->Append(&moreBlueBtn);

	w->Append(&redBoxImg);
	w->Append(&greenBoxImg);
	w->Append(&blueBoxImg);

	w->Append(sampleText);
	w->Append(redText);
	w->Append(greenText);
	w->Append(blueText);

	if(!SettingWindow("Red Green Blue",w))
	{
		redAmount = currentRed; // undo changes
		greenAmount = currentGreen;
		blueAmount = currentBlue;
	}

	delete(w);
	delete(redText);
	delete(greenText);
	delete(blueText);
}

GXColor GetCol(int i) {
	int c = 0;
	u8 r=0, g=0, b=0;
	if (unsigned(i) <= 13)
	{ 
		c = CurrentPalette.palette[i];
		r = (c >> 16) & 255;
		g = (c >> 8) & 255;
		b = (c) & 255;
		
	}
	return (GXColor){r,g,b,255};
}

/****************************************************************************
 * MenuPalette
 *
 * Menu displayed when returning to the menu from in-game.
 ***************************************************************************/
static int MenuPalette()
{
	// We are now using a custom palette
	strncpy(CurrentPalette.gameName, RomTitle, 17);

	int menu = MENU_NONE;

	GuiText titleTxt("Palette", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0, WIIDRC_BUTTON_HOME);

	GuiText bg0BtnTxt("BG 0", 24, GetCol(0));
	GuiImage bg0BtnImg(&btnCloseOutline);
	GuiImage bg0BtnImgOver(&btnCloseOutlineOver);
	GuiButton bg0Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	bg0Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	bg0Btn.SetPosition(-200, 120);
	bg0Btn.SetLabel(&bg0BtnTxt);
	bg0Btn.SetImage(&bg0BtnImg);
	bg0Btn.SetImageOver(&bg0BtnImgOver);
	bg0Btn.SetSoundOver(&btnSoundOver);
	bg0Btn.SetSoundClick(&btnSoundClick);
	bg0Btn.SetTrigger(trigA);
	bg0Btn.SetTrigger(trig2);
	bg0Btn.SetEffectGrow();

	GuiText bg1BtnTxt("BG 1", 24, GetCol(1));
	GuiImage bg1BtnImg(&btnCloseOutline);
	GuiImage bg1BtnImgOver(&btnCloseOutlineOver);
	GuiButton bg1Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	bg1Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	bg1Btn.SetPosition(-200, 180);
	bg1Btn.SetLabel(&bg1BtnTxt);
	bg1Btn.SetImage(&bg1BtnImg);
	bg1Btn.SetImageOver(&bg1BtnImgOver);
	bg1Btn.SetSoundOver(&btnSoundOver);
	bg1Btn.SetSoundClick(&btnSoundClick);
	bg1Btn.SetTrigger(trigA);
	bg1Btn.SetTrigger(trig2);
	bg1Btn.SetEffectGrow();

	GuiText bg2BtnTxt("BG 2", 24, GetCol(2));
	GuiImage bg2BtnImg(&btnCloseOutline);
	GuiImage bg2BtnImgOver(&btnCloseOutlineOver);
	GuiButton bg2Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	bg2Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	bg2Btn.SetPosition(-200, 240);
	bg2Btn.SetLabel(&bg2BtnTxt);
	bg2Btn.SetImage(&bg2BtnImg);
	bg2Btn.SetImageOver(&bg2BtnImgOver);
	bg2Btn.SetSoundOver(&btnSoundOver);
	bg2Btn.SetSoundClick(&btnSoundClick);
	bg2Btn.SetTrigger(trigA);
	bg2Btn.SetTrigger(trig2);
	bg2Btn.SetEffectGrow();

	GuiText bg3BtnTxt("BG 3", 24, GetCol(3));
	GuiImage bg3BtnImg(&btnCloseOutline);
	GuiImage bg3BtnImgOver(&btnCloseOutlineOver);
	GuiButton bg3Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	bg3Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	bg3Btn.SetPosition(-200, 300);
	bg3Btn.SetLabel(&bg3BtnTxt);
	bg3Btn.SetImage(&bg3BtnImg);
	bg3Btn.SetImageOver(&bg3BtnImgOver);
	bg3Btn.SetSoundOver(&btnSoundOver);
	bg3Btn.SetSoundClick(&btnSoundClick);
	bg3Btn.SetTrigger(trigA);
	bg3Btn.SetTrigger(trig2);
	bg3Btn.SetEffectGrow();

	GuiText win0BtnTxt("WIN 0", 24, GetCol(4));
	GuiImage win0BtnImg(&btnCloseOutline);
	GuiImage win0BtnImgOver(&btnCloseOutlineOver);
	GuiButton win0Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	win0Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	win0Btn.SetPosition(-70, 120);
	win0Btn.SetLabel(&win0BtnTxt);
	win0Btn.SetImage(&win0BtnImg);
	win0Btn.SetImageOver(&win0BtnImgOver);
	win0Btn.SetSoundOver(&btnSoundOver);
	win0Btn.SetSoundClick(&btnSoundClick);
	win0Btn.SetTrigger(trigA);
	win0Btn.SetTrigger(trig2);
	win0Btn.SetEffectGrow();

	GuiText win1BtnTxt("WIN 1", 24, GetCol(5));
	GuiImage win1BtnImg(&btnCloseOutline);
	GuiImage win1BtnImgOver(&btnCloseOutlineOver);
	GuiButton win1Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	win1Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	win1Btn.SetPosition(-70, 180);
	win1Btn.SetLabel(&win1BtnTxt);
	win1Btn.SetImage(&win1BtnImg);
	win1Btn.SetImageOver(&win1BtnImgOver);
	win1Btn.SetSoundOver(&btnSoundOver);
	win1Btn.SetSoundClick(&btnSoundClick);
	win1Btn.SetTrigger(trigA);
	win1Btn.SetTrigger(trig2);
	win1Btn.SetEffectGrow();

	GuiText win2BtnTxt("WIN 2", 24, GetCol(6));
	GuiImage win2BtnImg(&btnCloseOutline);
	GuiImage win2BtnImgOver(&btnCloseOutlineOver);
	GuiButton win2Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	win2Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	win2Btn.SetPosition(-70, 240);
	win2Btn.SetLabel(&win2BtnTxt);
	win2Btn.SetImage(&win2BtnImg);
	win2Btn.SetImageOver(&win2BtnImgOver);
	win2Btn.SetSoundOver(&btnSoundOver);
	win2Btn.SetSoundClick(&btnSoundClick);
	win2Btn.SetTrigger(trigA);
	win2Btn.SetTrigger(trig2);
	win2Btn.SetEffectGrow();

	GuiText win3BtnTxt("WIN 3", 24, GetCol(7));
	GuiImage win3BtnImg(&btnCloseOutline);
	GuiImage win3BtnImgOver(&btnCloseOutlineOver);
	GuiButton win3Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	win3Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	win3Btn.SetPosition(-70, 300);
	win3Btn.SetLabel(&win3BtnTxt);
	win3Btn.SetImage(&win3BtnImg);
	win3Btn.SetImageOver(&win3BtnImgOver);
	win3Btn.SetSoundOver(&btnSoundOver);
	win3Btn.SetSoundClick(&btnSoundClick);
	win3Btn.SetTrigger(trigA);
	win3Btn.SetTrigger(trig2);
	win3Btn.SetEffectGrow();

	GuiText obj0BtnTxt("OBJ 0", 24, GetCol(8));
	GuiImage obj0BtnImg(&btnCloseOutline);
	GuiImage obj0BtnImgOver(&btnCloseOutlineOver);
	GuiButton obj0Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	obj0Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	obj0Btn.SetPosition(+70, 120);
	obj0Btn.SetLabel(&obj0BtnTxt);
	obj0Btn.SetImage(&obj0BtnImg);
	obj0Btn.SetImageOver(&obj0BtnImgOver);
	obj0Btn.SetSoundOver(&btnSoundOver);
	obj0Btn.SetSoundClick(&btnSoundClick);
	obj0Btn.SetTrigger(trigA);
	obj0Btn.SetTrigger(trig2);
	obj0Btn.SetEffectGrow();

	GuiText obj1BtnTxt("OBJ 1", 24, GetCol(9));
	GuiImage obj1BtnImg(&btnCloseOutline);
	GuiImage obj1BtnImgOver(&btnCloseOutlineOver);
	GuiButton obj1Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	obj1Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	obj1Btn.SetPosition(+70, 180);
	obj1Btn.SetLabel(&obj1BtnTxt);
	obj1Btn.SetImage(&obj1BtnImg);
	obj1Btn.SetImageOver(&obj1BtnImgOver);
	obj1Btn.SetSoundOver(&btnSoundOver);
	obj1Btn.SetSoundClick(&btnSoundClick);
	obj1Btn.SetTrigger(trigA);
	obj1Btn.SetTrigger(trig2);
	obj1Btn.SetEffectGrow();

	GuiText obj2BtnTxt("OBJ 2", 24, GetCol(10));
	GuiImage obj2BtnImg(&btnCloseOutline);
	GuiImage obj2BtnImgOver(&btnCloseOutlineOver);
	GuiButton obj2Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	obj2Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	obj2Btn.SetPosition(+70, 240);
	obj2Btn.SetLabel(&obj2BtnTxt);
	obj2Btn.SetImage(&obj2BtnImg);
	obj2Btn.SetImageOver(&obj2BtnImgOver);
	obj2Btn.SetSoundOver(&btnSoundOver);
	obj2Btn.SetSoundClick(&btnSoundClick);
	obj2Btn.SetTrigger(trigA);
	obj2Btn.SetTrigger(trig2);
	obj2Btn.SetEffectGrow();

	GuiText spr0BtnTxt("SPR 0", 24, GetCol(11));
	GuiImage spr0BtnImg(&btnCloseOutline);
	GuiImage spr0BtnImgOver(&btnCloseOutlineOver);
	GuiButton spr0Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	spr0Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	spr0Btn.SetPosition(+200, 120);
	spr0Btn.SetLabel(&spr0BtnTxt);
	spr0Btn.SetImage(&spr0BtnImg);
	spr0Btn.SetImageOver(&spr0BtnImgOver);
	spr0Btn.SetSoundOver(&btnSoundOver);
	spr0Btn.SetSoundClick(&btnSoundClick);
	spr0Btn.SetTrigger(trigA);
	spr0Btn.SetTrigger(trig2);
	spr0Btn.SetEffectGrow();

	GuiText spr1BtnTxt("SPR 1", 24, GetCol(12));
	GuiImage spr1BtnImg(&btnCloseOutline);
	GuiImage spr1BtnImgOver(&btnCloseOutlineOver);
	GuiButton spr1Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	spr1Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	spr1Btn.SetPosition(+200, 180);
	spr1Btn.SetLabel(&spr1BtnTxt);
	spr1Btn.SetImage(&spr1BtnImg);
	spr1Btn.SetImageOver(&spr1BtnImgOver);
	spr1Btn.SetSoundOver(&btnSoundOver);
	spr1Btn.SetSoundClick(&btnSoundClick);
	spr1Btn.SetTrigger(trigA);
	spr1Btn.SetTrigger(trig2);
	spr1Btn.SetEffectGrow();

	GuiText spr2BtnTxt("SPR 2", 24, GetCol(13));
	GuiImage spr2BtnImg(&btnCloseOutline);
	GuiImage spr2BtnImgOver(&btnCloseOutlineOver);
	GuiButton spr2Btn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	spr2Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	spr2Btn.SetPosition(+200, 240);
	spr2Btn.SetLabel(&spr2BtnTxt);
	spr2Btn.SetImage(&spr2BtnImg);
	spr2Btn.SetImageOver(&spr2BtnImgOver);
	spr2Btn.SetSoundOver(&btnSoundOver);
	spr2Btn.SetSoundClick(&btnSoundClick);
	spr2Btn.SetTrigger(trigA);
	spr2Btn.SetTrigger(trig2);
	spr2Btn.SetEffectGrow();

	GuiText importBtnTxt("Load / Save", 22, (GXColor){0, 0, 0, 255});
	importBtnTxt.SetWrap(true, btnOutline.GetWidth()-30);
	GuiImage importBtnImg(&btnOutline);
	GuiImage importBtnImgOver(&btnOutlineOver);
	GuiButton importBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	importBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	importBtn.SetPosition(140, 300);
	importBtn.SetLabel(&importBtnTxt);
	importBtn.SetImage(&importBtnImg);
	importBtn.SetImageOver(&importBtnImgOver);
	importBtn.SetSoundOver(&btnSoundOver);
	importBtn.SetSoundClick(&btnSoundClick);
	importBtn.SetTrigger(trigA);
	importBtn.SetTrigger(trig2);
	importBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 20, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-50, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetSoundClick(&btnSoundClick);
	closeBtn.SetTrigger(trigA);
	closeBtn.SetTrigger(trig2);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetSoundOver(&btnSoundOver);
	backBtn.SetSoundClick(&btnSoundClick);
	backBtn.SetTrigger(trigA);
	backBtn.SetTrigger(trig2);
	backBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&bg0Btn);
	w.Append(&bg1Btn);
	w.Append(&bg2Btn);
	w.Append(&bg3Btn);
	w.Append(&win0Btn);
	w.Append(&win1Btn);
	w.Append(&win2Btn);
	w.Append(&win3Btn);
	w.Append(&obj0Btn);
	w.Append(&obj1Btn);
	w.Append(&obj2Btn);
	w.Append(&spr0Btn);
	w.Append(&spr1Btn);
	w.Append(&spr2Btn);
	w.Append(&importBtn);
	w.Append(&closeBtn);
	w.Append(&backBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(bg0Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[0] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[0] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[0] >> 0) & 0xFF;
			PaletteWindow("BG 0");
			CurrentPalette.palette[0] = redAmount << 16 | greenAmount << 8 | blueAmount;
			bg0BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			bg0Btn.ResetState();
		}
		else if(bg1Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[1] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[1] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[1] >> 0) & 0xFF;
			PaletteWindow("BG 1");
			CurrentPalette.palette[1] = redAmount << 16 | greenAmount << 8 | blueAmount;
			bg1BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			bg1Btn.ResetState();
		}
		else if(bg2Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[2] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[2] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[2] >> 0) & 0xFF;
			PaletteWindow("BG 2");
			CurrentPalette.palette[2] = redAmount << 16 | greenAmount << 8 | blueAmount;
			bg2BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			bg2Btn.ResetState();
		}
		else if(bg3Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[3] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[3] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[3] >> 0) & 0xFF;
			PaletteWindow("BG 3");
			CurrentPalette.palette[3] = redAmount << 16 | greenAmount << 8 | blueAmount;
			bg3BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			bg3Btn.ResetState();
		}
		else if(win0Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[4] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[4] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[4] >> 0) & 0xFF;
			PaletteWindow("WIN 0");
			CurrentPalette.palette[4] = redAmount << 16 | greenAmount << 8 | blueAmount;
			win0BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			win0Btn.ResetState();
		}
		else if(win1Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[5] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[5] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[5] >> 0) & 0xFF;
			PaletteWindow("WIN 1");
			CurrentPalette.palette[5] = redAmount << 16 | greenAmount << 8 | blueAmount;
			win1BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			win1Btn.ResetState();
		}
		else if(win2Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[6] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[6] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[6] >> 0) & 0xFF;
			PaletteWindow("WIN 2");
			CurrentPalette.palette[6] = redAmount << 16 | greenAmount << 8 | blueAmount;
			win2BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			win2Btn.ResetState();
		}
		else if(win3Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[7] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[7] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[7] >> 0) & 0xFF;
			PaletteWindow("WIN 3");
			CurrentPalette.palette[7] = redAmount << 16 | greenAmount << 8 | blueAmount;
			win3BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			win3Btn.ResetState();
		}
		else if(obj0Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[8] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[8] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[8] >> 0) & 0xFF;
			PaletteWindow("OBJ 0");
			CurrentPalette.palette[8] = redAmount << 16 | greenAmount << 8 | blueAmount;
			obj0BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			obj0Btn.ResetState();
		}
		else if(obj1Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[9] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[9] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[9] >> 0) & 0xFF;
			PaletteWindow("OBJ 1");
			CurrentPalette.palette[9] = redAmount << 16 | greenAmount << 8 | blueAmount;
			obj1BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			obj1Btn.ResetState();
		}
		else if(obj2Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[10] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[10] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[10] >> 0) & 0xFF;
			PaletteWindow("OBJ 2");
			CurrentPalette.palette[10] = redAmount << 16 | greenAmount << 8 | blueAmount;
			obj2BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			obj2Btn.ResetState();
		}
		else if(spr0Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[11] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[11] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[11] >> 0) & 0xFF;
			PaletteWindow("SPR 0");
			CurrentPalette.palette[11] = redAmount << 16 | greenAmount << 8 | blueAmount;
			spr0BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			spr0Btn.ResetState();
		}
		else if(spr1Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[12] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[12] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[12] >> 0) & 0xFF;
			PaletteWindow("SPR 1");
			CurrentPalette.palette[12] = redAmount << 16 | greenAmount << 8 | blueAmount;
			spr1BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			spr1Btn.ResetState();
		}
		else if(spr2Btn.GetState() == STATE_CLICKED)
		{
			redAmount = (CurrentPalette.palette[13] >> 16) & 0xFF;
			greenAmount = (CurrentPalette.palette[13] >> 8) & 0xFF;
			blueAmount = (CurrentPalette.palette[13] >> 0) & 0xFF;
			PaletteWindow("SPR 2");
			CurrentPalette.palette[13] = redAmount << 16 | greenAmount << 8 | blueAmount;
			spr2BtnTxt.SetColor((GXColor){(u8)redAmount, (u8)greenAmount, (u8)blueAmount, 0xFF});
			spr2Btn.ResetState();
		}
		else if(importBtn.GetState() == STATE_CLICKED)
		{
			SavePaletteAs(NOTSILENT, RomTitle);
			menu = MENU_GAMESETTINGS_PALETTE;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;
			SavePaletteAs(SILENT, RomTitle);

			exitSound->Play();
			bgTopImg->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			backBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			bgBottomImg->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			btnLogo->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);

			w.SetEffect(EFFECT_FADE, -15);

			usleep(350000); // wait for effects to finish
		}
		else if(backBtn.GetState() == STATE_CLICKED)
		{
			SavePaletteAs(SILENT, RomTitle);
			menu = MENU_GAMESETTINGS_VIDEO;
		}
	}

	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/
void
MainMenu (int menu)
{
	static bool firstRun = true;
	int currentMenu = menu;
	lastMenu = MENU_NONE;
	
	if(firstRun)
	{
		#ifdef HW_RVL
		pointer[0] = new GuiImageData(player1_point_png);
		pointer[1] = new GuiImageData(player2_point_png);
		pointer[2] = new GuiImageData(player3_point_png);
		pointer[3] = new GuiImageData(player4_point_png);
		#endif

		trigA = new GuiTrigger;
		trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A, WIIDRC_BUTTON_A);
		trig2 = new GuiTrigger;
		trig2->SetSimpleTrigger(-1, WPAD_BUTTON_2, 0, 0);
	}

	mainWindow = new GuiWindow(screenwidth, screenheight);

	if(menu == MENU_GAME)
	{
		gameScreen = new GuiImageData(gameScreenPng);
		gameScreenImg = new GuiImage(gameScreen);
		gameScreenImg->SetAlpha(192);
		gameScreenImg->ColorStripe(30);
		gameScreenImg->SetScaleX(screenwidth/(float)vmode->fbWidth);
		gameScreenImg->SetScaleY(screenheight/(float)vmode->efbHeight);
	}
	else
	{
		gameScreenImg = new GuiImage(screenwidth, screenheight, (GXColor){236, 226, 238, 255});
		gameScreenImg->ColorStripe(10);
	}

	mainWindow->Append(gameScreenImg);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData bgTop(bg_top_png);
	bgTopImg = new GuiImage(&bgTop);
	GuiImageData bgBottom(bg_bottom_png);
	bgBottomImg = new GuiImage(&bgBottom);
	bgBottomImg->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	GuiImageData logo(logo_png);
	GuiImage logoImg(&logo);
	GuiImageData logoOver(logo_over_png);
	GuiImage logoImgOver(&logoOver);
	GuiText logoTxt(APPVERSION, 18, (GXColor){255, 255, 255, 255});
	logoTxt.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	logoTxt.SetPosition(0, 4);
	btnLogo = new GuiButton(logoImg.GetWidth(), logoImg.GetHeight());
	btnLogo->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	btnLogo->SetPosition(-50, 24);
	btnLogo->SetImage(&logoImg);
	btnLogo->SetImageOver(&logoImgOver);
	btnLogo->SetLabel(&logoTxt);
	btnLogo->SetSoundOver(&btnSoundOver);
	btnLogo->SetSoundClick(&btnSoundClick);
	btnLogo->SetTrigger(trigA);
	btnLogo->SetTrigger(trig2);
	btnLogo->SetUpdateCallback(WindowCredits);

	mainWindow->Append(bgTopImg);
	mainWindow->Append(bgBottomImg);
	mainWindow->Append(btnLogo);

	if(currentMenu == MENU_GAMESELECTION)
		ResumeGui();

	if(firstRun) {
		// Load preferences
		if(!LoadPrefs())
			SavePrefs(SILENT);
	}

#ifdef HW_RVL
	if(firstRun)
	{
		u32 ios = IOS_GetVersion();

		if(!SupportedIOS(ios))
			ErrorPrompt("The current IOS is unsupported. Functionality and/or stability may be adversely affected.");
		else if(!SaneIOS(ios))
			ErrorPrompt("The current IOS has been altered (fake-signed). Functionality and/or stability may be adversely affected.");
	}
#endif

	#ifndef NO_SOUND
	if(firstRun) {
		bgMusic = new GuiSound(bg_music, bg_music_size, SOUND_OGG);
		bgMusic->SetVolume(GCSettings.MusicVolume);
		bgMusic->SetLoop(true);
		enterSound = new GuiSound(enter_ogg, enter_ogg_size, SOUND_OGG);
		enterSound->SetVolume(GCSettings.SFXVolume);
		exitSound = new GuiSound(exit_ogg, exit_ogg_size, SOUND_OGG);
		exitSound->SetVolume(GCSettings.SFXVolume);
	}

	if(currentMenu == MENU_GAMESELECTION)
		bgMusic->Play(); // startup music
	#endif

	if(firstRun) {
		LoadPalettes();
	}

	firstRun = false;

	while(currentMenu != MENU_EXIT || !ROMLoaded)
	{
		switch (currentMenu)
		{
			case MENU_GAMESELECTION:
				currentMenu = MenuGameSelection();
				break;
			case MENU_GAME:
				currentMenu = MenuGame();
				break;
			case MENU_GAME_LOAD:
				currentMenu = MenuGameSaves(0);
				break;
			case MENU_GAME_SAVE:
				currentMenu = MenuGameSaves(1);
				break;
			case MENU_GAME_DELETE:
				currentMenu = MenuGameSaves(2);
				break;	
			case MENU_GAMESETTINGS:
				currentMenu = MenuGameSettings();
				break;
			case MENU_GAMESETTINGS_MAPPINGS:
				currentMenu = MenuSettingsMappings();
				break;
			case MENU_GAMESETTINGS_MAPPINGS_MAP:
				currentMenu = MenuSettingsMappingsMap();
				break;
			case MENU_GAMESETTINGS_VIDEO:
				currentMenu = MenuSettingsVideo();
				break;
			case MENU_GAMESETTINGS_PALETTE:
				currentMenu = MenuPalette();
				break;
			case MENU_SETTINGS:
				currentMenu = MenuSettings();
				break;
			case MENU_SETTINGS_FILE:
				currentMenu = MenuSettingsFile();
				break;
			case MENU_SETTINGS_MENU:
				currentMenu = MenuSettingsMenu();
				break;
			case MENU_SETTINGS_NETWORK:
				currentMenu = MenuSettingsNetwork();
				break;
			case MENU_SETTINGS_EMULATION:
				currentMenu = MenuSettingsEmulation();
				break;
			default: // unrecognized menu
				currentMenu = MenuGameSelection();
				break;
		}
		lastMenu = currentMenu;
		usleep(THREAD_SLEEP);
	}

	#ifdef HW_RVL
	ShutoffRumble();
	#endif

	CancelAction();
	HaltGui();

	delete btnLogo;
	delete gameScreenImg;
	delete bgTopImg;
	delete bgBottomImg;
	delete mainWindow;

	mainWindow = NULL;

	if(gameScreen)
		delete gameScreen;

	ClearScreenshot();

	// wait for keys to be depressed
	while(MenuRequested())
	{
		UpdatePads();
		usleep(THREAD_SLEEP);
	}
}
