/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * input.h
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

#include <gccore.h>
#include <wiiuse/wpad.h>

#define PI 				3.14159265f
#define PADCAL			50
#define MAXJP 			10 // # of mappable controller buttons

#define VBA_BUTTON_A		1
#define VBA_BUTTON_B		2
#define VBA_BUTTON_SELECT	4
#define VBA_BUTTON_START	8
#define VBA_RIGHT			16
#define VBA_LEFT			32
#define VBA_UP				64
#define VBA_DOWN			128
#define VBA_BUTTON_R		256
#define VBA_BUTTON_L		512
#define VBA_SPEED			1024
#define VBA_CAPTURE			2048

enum
{
	TRIGGER_SIMPLE,
	TRIGGER_HELD,
	TRIGGER_BUTTON_ONLY,
	TRIGGER_BUTTON_ONLY_IN_FOCUS
};

typedef struct _paddata {
	u16 btns_d;
	u16 btns_u;
	u16 btns_h;
	s8 stickX;
	s8 stickY;
	s8 substickX;
	s8 substickY;
	u8 triggerL;
	u8 triggerR;
} PADData;

class GuiTrigger
{
	public:
		GuiTrigger();
		~GuiTrigger();
		void SetSimpleTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		void SetHeldTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		void SetButtonOnlyTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		void SetButtonOnlyInFocusTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		s8 WPAD_Stick(u8 right, int axis);
		bool Left();
		bool Right();
		bool Up();
		bool Down();

		u8 type;
		s32 chan;
		WPADData wpad;
		PADData pad;
};

extern GuiTrigger userInput[4];
extern int rumbleRequest[4];
extern u32 btnmap[5][10];

void ResetControls();
void ShutoffRumble();
void DoRumble(int i);
void systemGameRumble(int RumbleForFrames);
void systemGameRumbleOnlyFor(int OnlyRumbleForFrames);
void updateRumbleFrame();
s8 WPAD_Stick(u8 chan,u8 right, int axis);
u32 GetJoy(int which);

#endif
