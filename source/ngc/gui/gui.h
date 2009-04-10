/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui.h
 *
 * GUI class definitions
 ***************************************************************************/

#ifndef GUI_H
#define GUI_H

#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <math.h>
#include <wiiuse/wpad.h>
#include <mp3player.h>
#include "pngu/pngu.h"
#include "FreeTypeGX.h"
#include "vba.h"
#include "video.h"
#include "filelist.h"
#include "fileop.h"
#include "menu.h"
#include "input.h"

#define PI 3.14159265f
#define PADCAL 50
#define SCROLL_INITIAL_DELAY 20
#define SCROLL_LOOP_DELAY 3

#define SAVELISTSIZE 6

typedef void (*UpdateCallback)(void * e);

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

enum
{
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTRE,
	ALIGN_TOP,
	ALIGN_BOTTOM,
	ALIGN_MIDDLE
};

enum
{
	STATE_DEFAULT,
	STATE_SELECTED,
	STATE_CLICKED,
	STATE_DISABLED
};

enum
{
	TRIGGER_SIMPLE,
	TRIGGER_BUTTON_ONLY,
	TRIGGER_BUTTON_ONLY_IN_FOCUS
};

#define EFFECT_SLIDE_TOP			1
#define EFFECT_SLIDE_BOTTOM			2
#define EFFECT_SLIDE_RIGHT			4
#define EFFECT_SLIDE_LEFT			8
#define EFFECT_SLIDE_IN				16
#define EFFECT_SLIDE_OUT			32
#define EFFECT_FADE					64
#define EFFECT_SCALE				128
#define EFFECT_COLOR_TRANSITION		256

enum
{
	ON_CLICK,
	ON_OVER
};

class GuiSound
{
	public:
		GuiSound(const u8 * s, int l);
		~GuiSound();
		void Play();
	protected:
		const u8 * sound;
		s32 length;
};

class GuiTrigger
{
	public:
		GuiTrigger();
		~GuiTrigger();
		void SetSimpleTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
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

class GuiElement
{
	public:
		GuiElement();
		~GuiElement();
		void SetParent(GuiElement * e);
		int GetLeft();
		int GetTop();
		int GetWidth();
		int GetHeight();
		void SetSize(int w, int h);
		bool IsVisible();
		bool IsSelectable();
		bool IsClickable();
		void SetSelectable(bool s);
		void SetClickable(bool c);
		int GetState();
		void SetVisible(bool v);
		void SetAlpha(int a);
		int GetAlpha();
		void SetScale(float s);
		float GetScale();
		void SetTrigger(GuiTrigger * t);
		void SetTrigger(u8 i, GuiTrigger * t);
		void SetEffect(int e, int a, int t=0);
		void SetEffectOnOver(int e, int a, int t=0);
		void SetEffectGrow();
		int GetEffect();
		bool IsInside(int x, int y);
		void SetPosition(int x, int y);
		void UpdateEffects();
		void SetUpdateCallback(UpdateCallback u);
		int IsFocused();
		virtual void SetFocus(int f);
		virtual void SetState(int s);
		virtual void ResetState();
		virtual int GetSelected();
		virtual void SetAlignment(int hor, int vert);
		virtual void Update(GuiTrigger * t);
		virtual void Draw();
	protected:
		bool visible;
		int focus; // -1 = cannot focus, 0 = not focused, 1 = focused
		int width;
		int height;
		int xoffset;
		int yoffset;
		int xoffsetDyn;
		int yoffsetDyn;
		int alpha;
		f32 scale;
		int alphaDyn;
		f32 scaleDyn;
		int effects;
		int effectAmount;
		int effectTarget;
		int effectsOver;
		int effectAmountOver;
		int effectTargetOver;
		int alignmentHor; // LEFT, RIGHT, CENTRE
		int alignmentVert; // TOP, BOTTOM, MIDDLE
		int state; // DEFAULT, SELECTED, CLICKED, DISABLED
		bool selectable; // is SELECTED a valid state?
		bool clickable; // is CLICKED a valid state?
		GuiTrigger * trigger[2];
		GuiElement * parentElement;
		UpdateCallback updateCB;
};

class GuiImageData
{
	public:
		GuiImageData(const u8 * i);
		~GuiImageData();
		u8 * GetImage();
		int GetWidth();
		int GetHeight();
	protected:
		u8 * data;
		int height;
		int width;
};

class GuiImage : public GuiElement
{
	public:
		GuiImage(GuiImageData * img);
		GuiImage(u8 * img, int w, int h);
		GuiImage(int w, int h, GXColor c);
		~GuiImage();
		void SetAngle(float a);
		void SetTile(int t);
		void Draw();
		u8 * GetImage();
		void SetImage(GuiImageData * img);
		void SetImage(u8 * img, int w, int h);
		GXColor GetPixel(int x, int y);
		void SetPixel(int x, int y, GXColor color);
		void Stripe(int s);
	protected:
		u8 * image;
		f32 imageangle;
		int tile;
};

#endif
