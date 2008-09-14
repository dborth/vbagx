/****************************************************************************
* VisualBoyAdvance 1.7.2
*
* Nintendo GameCube Joypad Wrapper
****************************************************************************/

#include <gccore.h>
#include <stdio.h>


#define VBA_BUTTON_A		1
#define VBA_BUTTON_B		2
#define VBA_BUTTON_SELECT	4
#define VBA_BUTTON_START	8
#define VBA_RIGHT		16
#define VBA_LEFT		32
#define VBA_UP			64
#define VBA_DOWN		128
#define VBA_BUTTON_R		256
#define VBA_BUTTON_L		512
#define VBA_SPEED		1024
#define VBA_CAPTURE		2048

#ifdef WII_BUILD
#include <math.h>
#include <wiiuse/wpad.h>
int isClassicAvailable = 0;
int isWiimoteAvailable = 0;
/*
#ifndef PI
#define PI 3.14159f
#endif

enum { STICK_X, STICK_Y };
static int getStickValue(joystick_t* j, int axis, int maxAbsValue){
	double angle = PI * j->ang/180.0f;
	double magnitude = (j->mag > 1.0f) ? 1.0f :
	                    (j->mag < -1.0f) ? -1.0f : j->mag;
	double value;
	if(axis == STICK_X)
		value = magnitude * sin( angle );
	else
		value = magnitude * cos( angle );
	return (int)(value * maxAbsValue);
}*/

#endif
int menuCalled = 0;
u32
NGCPad()
{
  u32             res = 0;
  short           p;
  signed char     x,
  y;
  int             padcal = 90;
  float           t;
  
#ifdef WII_BUILD
	//wiimote
	WPADData *wpad;
	WPAD_ScanPads(); 
	wpad = WPAD_Data(0);
	if (isWiimoteAvailable)
	{
		unsigned short b = wpad->btns_h;

		if (b & WPAD_BUTTON_2)
	      res |= VBA_BUTTON_A;
	
	  	if (b & WPAD_BUTTON_1)
	  	  res |= VBA_BUTTON_B;
	  	
	  	if (b & WPAD_BUTTON_MINUS)
	  	  res |= VBA_BUTTON_SELECT;
	  	
	  	if (b & WPAD_BUTTON_PLUS)
	  	  res |= VBA_BUTTON_START;
	  	
	  	if (b & WPAD_BUTTON_RIGHT)
	  	  res |= VBA_UP;
	  	
	  	if (b & WPAD_BUTTON_LEFT)
	  	  res |= VBA_DOWN;
	  	
	  	if (b & WPAD_BUTTON_UP)
	  	  res |= VBA_LEFT;
	  	
	  	if (b & WPAD_BUTTON_DOWN)
	  	  res |= VBA_RIGHT;
	  	
	  	if (b & WPAD_BUTTON_A)
	  	  res |= VBA_BUTTON_L;
	  	
	  	if (b & WPAD_BUTTON_B)
	  	  res |= VBA_BUTTON_R;
	  	  
	    if (b & WPAD_BUTTON_HOME)
  		   menuCalled = 1;
	}
	//classic controller
	if (isClassicAvailable)
	{
		float mag,ang;
		int b = wpad->exp.classic.btns;
	/* Stolen Shamelessly out of Falco's unofficial SNES9x update */
		ang = wpad->exp.classic.ljs.ang;
		mag = wpad->exp.classic.ljs.mag;

		if (mag > 0.4) {
			if (ang > 292.5 | ang <= 67.5)
				res |= VBA_UP;
			if (ang > 22.5 & ang <= 157.5)
				res |= VBA_RIGHT;
			if (ang > 113.5 & ang <= 247.5)
				res |= VBA_DOWN;
			if (ang > 203.5 & ang <= 337.5)
				res |= VBA_LEFT;
	}

		int x_s =  0; //getStickValue(&wpad.exp.classic.ljs, STICK_X, 127);
		int y_s =  0; //getStickValue(&wpad.exp.classic.ljs, STICK_Y, 127);
		if (b & CLASSIC_CTRL_BUTTON_A)
	      res |= VBA_BUTTON_A;
	
	  	if (b & CLASSIC_CTRL_BUTTON_B)
	  	  res |= VBA_BUTTON_B;
	  	
	  	if (b & CLASSIC_CTRL_BUTTON_MINUS)
	  	  res |= VBA_BUTTON_SELECT;
	  	
	  	if (b & CLASSIC_CTRL_BUTTON_PLUS)
	  	  res |= VBA_BUTTON_START;
	  	
	  	if ((b & CLASSIC_CTRL_BUTTON_UP) || (y_s > 0))
	  	  res |= VBA_UP;
	  	
	  	if ((b & CLASSIC_CTRL_BUTTON_DOWN) || (y_s < 0))
	  	  res |= VBA_DOWN;
	  	
	  	if ((b & CLASSIC_CTRL_BUTTON_LEFT) || (x_s < 0))
	  	  res |= VBA_LEFT;
	  	
	  	if ((b & CLASSIC_CTRL_BUTTON_RIGHT) || (x_s > 0))
	  	  res |= VBA_RIGHT;
	  	
	  	if (b & CLASSIC_CTRL_BUTTON_FULL_L)
	  	  res |= VBA_BUTTON_L;
	  	
	  	if (b & CLASSIC_CTRL_BUTTON_FULL_R)
	  	  res |= VBA_BUTTON_R;
	  	  
	    if (b & CLASSIC_CTRL_BUTTON_HOME)
  		   menuCalled = 1;

	}
	//user needs a GC remote
	else
	{
		 p = PAD_ButtonsHeld(0);
	     x = PAD_StickX(0);
	     y = PAD_StickY(0);
		 if (x * x + y * y > padcal * padcal)
	     {
	       	if (x > 0 && y == 0)
	       	  res |= VBA_RIGHT;
	       	if (x < 0 && y == 0)
	       	  res |= VBA_LEFT;
	       	if (x == 0 && y > 0)
	       	  res |= VBA_UP;
	       	if (x == 0 && y < 0)
	       	  res |= VBA_DOWN;
           	
	       	/*** Recalc left / right ***/
	       	t = (float) y / x;
	       	if (t >= -2.41421356237 && t < 2.41421356237)
	       	{
	       		if (x >= 0)
	       		  res |= VBA_RIGHT;
	       		else
	       		  res |= VBA_LEFT;
	       	}
	       	
	       	/*** Recalc up / down ***/
	       	t = (float) x / y;
	       	if (t >= -2.41421356237 && t < 2.41421356237)
	       	{
	       	    if (y >= 0)
	       	      res |= VBA_UP;
	       	    else
	       	      res |= VBA_DOWN;
	       	}
	     }
	     if (p & PAD_BUTTON_A)
	       res |= VBA_BUTTON_A;
	     
	     if (p & PAD_BUTTON_B)
	       res |= VBA_BUTTON_B;
	     
	     if (p & PAD_TRIGGER_Z)
	       res |= VBA_BUTTON_SELECT;
	     
	     if (p & PAD_BUTTON_START)
	       res |= VBA_BUTTON_START;
	     
	     if (p & PAD_BUTTON_UP)
	       res |= VBA_UP;
	     
	     if (p & PAD_BUTTON_DOWN)
	       res |= VBA_DOWN;
	     
	     if (p & PAD_BUTTON_LEFT)
	       res |= VBA_LEFT;
	     
	     if (p & PAD_BUTTON_RIGHT)
	       res |= VBA_RIGHT;
	     
	     if (p & PAD_TRIGGER_L)
	       res |= VBA_BUTTON_L;
	     
	     if (p & PAD_TRIGGER_R)
	       res |= VBA_BUTTON_R;
	       
	     if((p & PAD_BUTTON_X) && (p & PAD_BUTTON_Y))
  		   menuCalled = 1;
	 }
#endif

#ifdef GC_BUILD
  p = PAD_ButtonsHeld(0);
  x = PAD_StickX(0);
  y = PAD_StickY(0);
  if (x * x + y * y > padcal * padcal)
    {
      if (x > 0 && y == 0)
        res |= VBA_RIGHT;
      if (x < 0 && y == 0)
        res |= VBA_LEFT;
      if (x == 0 && y > 0)
        res |= VBA_UP;
      if (x == 0 && y < 0)
        res |= VBA_DOWN;

      /*** Recalc left / right ***/
      t = (float) y / x;
      if (t >= -2.41421356237 && t < 2.41421356237)
        {
          if (x >= 0)
            res |= VBA_RIGHT;
          else
            res |= VBA_LEFT;
        }

      /*** Recalc up / down ***/
      t = (float) x / y;
      if (t >= -2.41421356237 && t < 2.41421356237)
        {
          if (y >= 0)
            res |= VBA_UP;
          else
            res |= VBA_DOWN;
        }
    }
  if (p & PAD_BUTTON_A)
    res |= VBA_BUTTON_A;

  if (p & PAD_BUTTON_B)
    res |= VBA_BUTTON_B;

  if (p & PAD_TRIGGER_Z)
    res |= VBA_BUTTON_SELECT;

  if (p & PAD_BUTTON_START)
    res |= VBA_BUTTON_START;

  if ((p & PAD_BUTTON_UP))
    res |= VBA_UP;

  if ((p & PAD_BUTTON_DOWN))
    res |= VBA_DOWN;

  if ((p & PAD_BUTTON_LEFT))
    res |= VBA_LEFT;

  if ((p & PAD_BUTTON_RIGHT))
    res |= VBA_RIGHT;

  if (p & PAD_TRIGGER_L)
    res |= VBA_BUTTON_L;

  if (p & PAD_TRIGGER_R)
    res |= VBA_BUTTON_R;
    
  if((p & PAD_BUTTON_X) && (p & PAD_BUTTON_Y))
  	menuCalled = 1;
#endif

  if ((res & 48) == 48)
    res &= ~16;
  if ((res & 192) == 192)
    res &= ~128;

  return res;
}
