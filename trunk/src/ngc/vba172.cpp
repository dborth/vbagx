/****************************************************************************
* VisualBoyAdvance 1.7.2
* Nintendo GameCube Wrapper
****************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug.h>

#ifdef WII_BUILD

extern "C"
  {
#include <ogc/conf.h>
#include <math.h>
#include <wiiuse/wpad.h>
extern s32 CONF_Init(void);
  }
  
extern int isClassicAvailable;
extern int isWiimoteAvailable;

void setup_controllers()
{	/*  Doesn't work, either always returns 
	WiimoteAvailable or switches REALLY fast
	between the two.  WTF.

	WPAD_ScanPads();
	WPADData pad;
	WPAD_ReadEvent(0, &pad);
	// User can use just wiimote
	if(pad.exp.type == WPAD_EXP_NONE)
	{
		isClassicAvailable = 0;
		isWiimoteAvailable = 1;
		return;
	}
	
	// User can use a Classic controller	
	else if(pad.exp.type == WPAD_EXP_CLASSIC)
	{
		isClassicAvailable = 1;
		WPAD_SetDataFormat(0, WPAD_FMT_BTNS);
		return;
	}
	// User will have to use a GC controller
	else	
	{
		isClassicAvailable = 0;
		isWiimoteAvailable = 0;
		return;
	}
	*/
	isClassicAvailable = 1;
	isWiimoteAvailable = 1;
	return;
}
#endif

#include "mixer.h"
#include "gcpad.h"
#include "vmmem.h"
#include "pal60.h"


extern "C"
  {
#include "gx_supp.h"
#include "tbtime.h"
  }

/** VBA **/
#include "GBA.h"
#include "agbprint.h"
#include "Flash.h"
#include "Port.h"
#include "RTC.h"
#include "Sound.h"
#include "Text.h"
#include "unzip.h"
#include "Util.h"
#include "gb/GB.h"
#include "gb/gbGlobals.h"

/**
 * Globals
 */
int RGB_LOW_BITS_MASK=0x821;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
u16 systemGbPalette[24];
int systemDebug = 0;
int emulating = 0;
int systemFrameSkip = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
int sensorX = 2047;
int sensorY = 2047;
u16 systemColorMap16[0x10000];
//u32 systemColorMap32[0x10000];
u32 *systemColorMap32 = (u32 *)&systemColorMap16;
bool systemSoundOn = false;
int systemVerbose = 0;
int cartridgeType = 0;
int srcWidth = 0;
int srcHeight = 0;
int destWidth = 0;
int destHeight = 0;
int srcPitch = 0;
extern int menuCalled;
#define WITHGX	1
#define DEBUGON 0

#if DEBUGON
const char *dbg_local_ip = "192.168.1.32";
const char *dbg_netmask = "255.255.255.0";
const char *dbg_gw = "192.168.1.100";
#endif

/*** 2D Video Globals ***/
GXRModeObj *vmode;		/*** Graphics Mode Object ***/
u32 *xfb[2] = { NULL, NULL };	/*** Framebuffers ***/
int whichfb = 0;		/*** Frame buffer toggle ***/

void debuggerOutput(char *, u32)
{}

void (*dbgOutput)(char *, u32) = debuggerOutput;

/**
 * Locals
 */
extern "C"
  {
    void SDInit( void );
    int gen_getdir( char *whichdir );
  };
extern char *direntries[1000];
extern char *MENU_GetLoadFile( char *whichdir );
extern void MENU_DrawString( int x, int y, char *msg, int style );

struct EmulatedSystem emulator =
    {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      false,
      0
    };

static tb_t start, now;
static u8 soundbuffer[3200] ATTRIBUTE_ALIGN(32);

u32 loadtimeradjust;

void doScan(u32 blah)
{
	PAD_ScanPads();
}

/****************************************************************************
* Initialise Video
*
* Before doing anything in libogc, it's recommended to configure a video
* output.
****************************************************************************/
static void
Initialise (void)
{
  VIDEO_Init ();		/*** ALWAYS CALL FIRST IN ANY LIBOGC PROJECT!
  				     Not only does it initialise the video 
  				     subsystem, but also sets up the ogc os
  				***/
#if DEBUGON
  DEBUG_Init(2424);
  _break();
#endif

  PAD_Init ();			/*** Initialise pads for input ***/
#ifdef WII_BUILD
  CONF_Init();
  WPAD_Init();
#endif
  vmode = VIDEO_GetPreferredMode(NULL);

  /*** Now configure the framebuffer.
       Really a framebuffer is just a chunk of memory
       to hold the display line by line.
  ***/

  xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
  /*** I prefer also to have a second buffer for double-buffering.
       This is not needed for the console demo.
  ***/
  xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

  /*** Define a console ***/
  console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight,
                vmode->fbWidth * 2);

  /*** Let libogc configure the mode ***/
  VIDEO_Configure (vmode);

  /*** Clear framebuffer to black ***/
  VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);

  /*** Set the framebuffer to be displayed at next VBlank ***/
  VIDEO_SetNextFramebuffer (xfb[0]);

  /*** Get the PAD status updated by libogc ***/
  VIDEO_SetPreRetraceCallback (doScan);
  VIDEO_SetBlack (0);

  int i;
  u32 *vreg = (u32 *)0xCC002000;
  for ( i = 0; i < 64; i++ )
	vreg[i] = vpal60[i];

  /*** Update the video for next vblank ***/
  VIDEO_Flush ();

  VIDEO_WaitVSync ();		/*** Wait for VBL ***/
  if (vmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync ();

}

void systemMessage(int num, const char *msg, ...)
{
  /*** For now ... do nothing ***/
}
void systemFrame()
{}

void systemScreenCapture(int a)
{}

void systemShowSpeed(int speed)
{}
static u32 autoFrameSkipLastTime = 0;
static int frameskipadjust = 0;
void system1Frame(int rate)
{
/*	  if ( cartridgeType == 1 )
	    return;
	
	  u32 time = systemGetClock();
	  u32 diff = time - autoFrameSkipLastTime;
	  int speed = 100;
	
	  if(diff)
	    speed = (1000000/rate)/diff;
	  char temp[512];
	  sprintf(temp,"Speed: %i",speed);
	  MENU_DrawString( -1, 450,temp , 1 );
	  	
	  if(speed >= 98)
	    {
	      frameskipadjust++;
	
	      if(frameskipadjust >= 3)
	        {
	          frameskipadjust=0;
	          if(systemFrameSkip > 0)
	            systemFrameSkip--;
	        }
	    }
	  else
	    {
	      if(speed  < 80)
	        frameskipadjust -= (90 - speed)/5;
	      else if(systemFrameSkip < 9)
	        frameskipadjust--;
	
	      if(frameskipadjust <= -2)
	        {
	          frameskipadjust += 2;
	          if(systemFrameSkip < 9)
	            systemFrameSkip++;
	        }
	    }
	  autoFrameSkipLastTime = time;
*/
}

void system10Frames(int rate)
{
	  if ( cartridgeType == 1 )
	    return;

	  u32 time = systemGetClock();
	  u32 diff = time - autoFrameSkipLastTime;
	  int speed = 100;
	
	  if(diff)
	    speed = (1000000/rate)/diff;
/*	  char temp[512];
	  sprintf(temp,"Speed: %i",speed);
	  MENU_DrawString( -1, 450,temp , 1 );
*/	  	
	  if(speed >= 98)
	    {
	      frameskipadjust++;
	
	      if(frameskipadjust >= 3)
	        {
	          frameskipadjust=0;
	          if(systemFrameSkip > 0)
	            systemFrameSkip--;
	        }
	    }
	  else
	    {
	      if(speed  < 80)
	        frameskipadjust -= (90 - speed)/5;
	      else if(systemFrameSkip < 9)
	        frameskipadjust--;
	
	      if(frameskipadjust <= -2)
	        {
	          frameskipadjust += 2;
	          if(systemFrameSkip < 9)
	            systemFrameSkip++;
	        }
	    }
	  autoFrameSkipLastTime = time;

}

void systemUpdateMotionSensor()
{}

void systemGbBorderOn()
{}

void systemWriteDataToSoundBuffer()
{
  MIXER_AddSamples((u8 *)soundFinalWave, (cartridgeType == 1));
}

void systemSoundPause()
{}

void systemSoundResume()
{}

void systemSoundReset()
{}

void systemGbPrint(u8 *data,int pages,int feed,int palette, int contrast)
{}

void systemSoundShutdown()
{}

static void AudioPlayer( void )
{
  AUDIO_StopDMA();
  MIXER_GetSamples(soundbuffer, 3200);
  DCFlushRange(soundbuffer,3200);
  AUDIO_InitDMA((u32)soundbuffer,3200);
  AUDIO_StartDMA();
}

bool systemSoundInit()
{
  AUDIO_Init(NULL);
  AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
  AUDIO_RegisterDMACallback(AudioPlayer);
  memset(soundbuffer, 0, 3200);

  //printf("Audio Inited\n");
  return true;
}

static void AudioDeInit()
{
	AUDIO_StopDMA();
}

bool systemPauseOnFrame()
{
  return false;
}

int systemGetSensorX()
{
  return sensorX;
}

int systemGetSensorY()
{
  return sensorY;
}

bool systemCanChangeSoundQuality()
{
  return true;
}

/****************************************************************************
* systemReadJoypads
****************************************************************************/
bool systemReadJoypads()
{
  return true;
}

u32 systemReadJoypad(int which)
{
  return NGCPad();
}

/****************************************************************************
* systemGetClock
*
* Returns number of milliseconds since program start
****************************************************************************/
u32 systemGetClock( void )
{
  mftb(&now);
  return tb_diff_msec(&now, &start) - loadtimeradjust;
}

/****************************************************************************
* systemDrawScreen
****************************************************************************/
void systemDrawScreen()
{
  /** GB / GBC Have oodles of time - so sync on VSync **/
#if WITHGX
  GX_Render( srcWidth, srcHeight, pix, srcPitch );
#endif
#ifdef WII_BUILD
	VIDEO_WaitVSync ();
#endif
#ifdef GC_BUILD
  if ( cartridgeType == 1 )
    {
      VIDEO_WaitVSync();
    }
#endif
}

/****************************************************************************
* Showdir
****************************************************************************/
int ShowDir( char *whichdir )
{
	int count;
	int i;
	count = gen_getdir( whichdir );
	printf("Found %d files\n",count);

	for ( i = 0; i < count; i++ )
		printf("%s\n", direntries[i]);
	while(1);
}

char filename[1024];	//rom file name
char batfilename[1024]; //battery save file name


/*
	Asks user to select a ROM
*/
void chooseRom() {
	//char *fname = "Sonic Advance 3.GBA";
	char *fname = NULL;
	memset(filename,0,1024);
	memset(batfilename,0,1024);
	fname = MENU_GetLoadFile( "/VBA/ROMS" );
	sprintf(filename, "/VBA/ROMS/%s", fname);

  	// construct battery save file name
  	strcpy(batfilename, "/VBA/SAVES/");
  	strncat(batfilename,fname,strlen(fname)-4);
  	strcat(batfilename,".SAV");

}

void askSound() {
  soundOffFlag = false;
  soundLowPass =  true;
}

/*
	Checks to see if a previous SRAM/Flash save exists
	If it does, it prompts the user to load it or not
*/
void checkSave() {
	
}
void GC_Sleep(u32 dwMiliseconds)
{
	int nVBlanks = (dwMiliseconds / 16);
	while (nVBlanks-- > 0)
	{
		VIDEO_WaitVSync();
	}
}
void write_save()
{
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
   	MENU_DrawString( -1, 250,"Attempting to Save data ..." , 1 );
    whichfb^=1;
	VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
    if(emulator.emuWriteBattery(batfilename) == true) 
		MENU_DrawString( -1, 250,"Save OK!" , 1 );
	else
		MENU_DrawString( -1, 250,"Either no data to save yet or error!" , 1 );
	VIDEO_WaitVSync ();
    VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	GC_Sleep(2000);
	
}

int ingameMenu() {
	tb_t start,end;
	mftb(&start);
	char temp[512];
	u16 buttons;
	  AudioDeInit();
	  whichfb^=1;
	  VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
      MENU_DrawString( 140, 50,"VisualBoyAdvance 1.7.2" , 1 );

#ifdef GC_BUILD
      MENU_DrawString( 140, 80,"Nintendo Gamecube Port" , 1 );
      MENU_DrawString( -1, 170,"(B)       - Resume play" , 1 );
	  MENU_DrawString( -1, 200,"(L)       - Write save" , 1 );
	  MENU_DrawString( -1, 230,"(R)       - Reset game" , 1 );
	  MENU_DrawString( -1, 320,"(Z)       - Return to loader" , 1 );
#endif
#ifdef WII_BUILD
      MENU_DrawString( 140, 80,"Nintendo Wii Port" , 1 );
      MENU_DrawString( -1, 170,"(B) or (Home)    - Resume play" , 1 );
	  MENU_DrawString( -1, 200,"(L) or  (+)      - Write save" , 1 );
	  MENU_DrawString( -1, 230,"(R) or  (-)      - Reset game" , 1 );
	  MENU_DrawString( -1, 320,"(Z) or (A+B)     - Return to loader" , 1 );
#endif	  

      sprintf(temp,"Frameskip: Auto (Currently %i)",systemFrameSkip);
	  MENU_DrawString( -1, 450,temp , 1 );

      VIDEO_WaitVSync ();
      VIDEO_SetNextFramebuffer(xfb[whichfb]);
	  VIDEO_Flush();
	  VIDEO_WaitVSync();
	 
	  //wait for user to let go of menu calling button
	  do{buttons = PAD_ButtonsHeld(0);}while((buttons & PAD_BUTTON_A)||(buttons & PAD_BUTTON_B));
	  //wait for user to let go of home button
#ifdef WII_BUILD
		WPADData *wpad;
		int btn;
		if(isWiimoteAvailable)  
			do{WPAD_ScanPads(); wpad = WPAD_Data(0); btn = wpad->btns_h;}while(btn & WPAD_BUTTON_HOME);
		if(isClassicAvailable)  
			do{WPAD_ScanPads(); wpad = WPAD_Data(0); btn = wpad->exp.classic.btns;}while(btn & CLASSIC_CTRL_BUTTON_HOME);
#endif

	  while(1){
		
#ifdef WII_BUILD
		WPADData *wpad;
		WPAD_ScanPads();
		wpad = WPAD_Data(0);
		if (isWiimoteAvailable)
		{
			unsigned short b = wpad->btns_h;
			if(b & WPAD_BUTTON_MINUS){ //Reset game
		    	emulator.emuReset();
		    	break;
	    	}
		    if(b & WPAD_BUTTON_PLUS) { //Write save
		    	write_save();
			    break;
	    	}
	    	if((b & WPAD_BUTTON_A) && (b & WPAD_BUTTON_B)) { //Return to loader
				void (*reload)() = (void(*)())0x80001800;
				reload();    		
	    	}
	    	if(b & WPAD_BUTTON_HOME) {	//Resume play
	    	do{WPAD_ScanPads(); wpad = WPAD_Data(0); b = wpad->btns_h;}while(b & WPAD_BUTTON_HOME);  //wait for home
				break;
	    	}
    	}
    	if (isClassicAvailable)
    	{
	    	int b = wpad->exp.classic.btns;
			if(b & CLASSIC_CTRL_BUTTON_MINUS){ //Reset game
		    	emulator.emuReset();
		    	break;
	    	}
		    if(b & CLASSIC_CTRL_BUTTON_PLUS) { //Write save
		    	write_save();
			    break;
	    	}
	    	if((b & CLASSIC_CTRL_BUTTON_A) && (b & CLASSIC_CTRL_BUTTON_B)) { //Return to loader
				void (*reload)() = (void(*)())0x80001800;
				reload();    		
	    	}
	    	if(b & CLASSIC_CTRL_BUTTON_HOME) {	//Resume play
	    		do{WPAD_ScanPads(); wpad = WPAD_Data(0); b = wpad->exp.classic.btns;}
			while(b & CLASSIC_CTRL_BUTTON_HOME); //wait for home button
				break;
	    	}
    	}
#endif

		u16 buttons = PAD_ButtonsHeld(0); //grab pad buttons

		if(buttons & PAD_TRIGGER_R){ //Reset game
	    	emulator.emuReset();
	    	break;
    	}
	    if(buttons & PAD_TRIGGER_L) { //Write save
	    	write_save();
		    break;
    	}
    	if(buttons & PAD_TRIGGER_Z) { //Return to loader
			void (*reload)() = (void(*)())0x80001800;
			reload();    		
    	}
    	if(buttons & PAD_BUTTON_B) {	//Resume play
			break;
    	}
	}
	AudioPlayer();
	mftb(&end);
    loadtimeradjust += tb_diff_msec(&end, &start);
	return 0;
		  
}

/****************************************************************************
* main
*
* Program entry
****************************************************************************/
int main()
{
    int i;
    int vAspect = 0;
    int hAspect = 0;

    Initialise();
    
    printf("\n\n\nVisualBoyAdvance 1.7.2\n");
    printf("Nintendo Wii Port\n");

    
    /** Kick off SD Lib **/
    SDInit();     
    
        /** Build GBPalette **/
	  for( i = 0; i < 24; )
	    {
	      systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
	      systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
	      systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
	      systemGbPalette[i++] = 0;
	    }
	 /** Set palette etc - Fixed to RGB565 **/
	  systemColorDepth = 16;
	  systemRedShift = 11;
	  systemGreenShift = 6;
	  systemBlueShift = 0;
	  for(i = 0; i < 0x10000; i++)
	    {
	       systemColorMap16[i] = ((i & 0x1f) << systemRedShift) |
	                             (((i & 0x3e0) >> 5) << systemGreenShift) |
	                             (((i & 0x7c00) >> 10) << systemBlueShift);
	    }
    
#ifdef WII_BUILD
	  setup_controllers();
#endif	    
	//Main loop
	while(1) {
		cartridgeType = 0;
		srcWidth = 0;
		srcHeight = 0;
		destWidth = 0;
		destHeight = 0;
		srcPitch = 0;
	
		chooseRom();
		//checkSave();

	  IMAGE_TYPE type = utilFindType(filename);
	
	  switch( type )
	    {
	    case IMAGE_GBA:
//	      printf("GameBoy Advance Image\n");
	      cartridgeType = 2;
	      emulator = GBASystem;
	      srcWidth = 240;
	      srcHeight = 160;
	      VMCPULoadROM(filename);
	      /* Actual Visual Aspect is 1.57 */
	      hAspect = 70;
	      vAspect = 46;
	      srcPitch = 484;
	      soundQuality = 2;
	      soundBufferLen = 1470;
	      cpuSaveType = 0;
	      break;
	
	    case IMAGE_GB:
//	      printf("GameBoy Image\n");
	      cartridgeType = 1;
	      emulator = GBSystem;
	      srcWidth = 160;
	      srcHeight = 144;
	      gbLoadRom(filename);
	      /* Actual physical aspect is 1.0 */
	      hAspect = 60;
	      vAspect = 46;
	      srcPitch = 324;
	      soundQuality = 1;
	      soundBufferLen = 1470 * 2;
	      break;
	
	    default:
//	      printf("Unknown Image\n");
	      while(1);
	      break;
	
	    }
	
	  /** Set defaults **/
	  flashSetSize(0x10000);
	  rtcEnable(true);
	  agbPrintEnable(false);
	  askSound();

	#if WITHGX
	  /** Set GX **/
	  GX_Start( srcWidth, srcHeight, hAspect, vAspect );
	#endif
	
	  if ( cartridgeType == 1 )
	    {
	      gbSoundReset();
	      gbSoundSetQuality(soundQuality);
	    }
	  else
	    {
	      soundSetQuality(soundQuality);
	      CPUInit("/VBA/BIOS/BIOS.GBA", 1);
	      CPUReset();
	    }

	  soundVolume = 0;
	  systemSoundOn = true;
	
	  soundInit();
	  AudioPlayer();

	  emulating = 1;
	  /** Start system clock **/
	  mftb(&start);
	  
	  emulator.emuReadBattery(batfilename);
      while ( emulating )
	  {
	    emulator.emuMain(emulator.emuCount);
	    if(menuCalled) {
	      if(ingameMenu())
	      	break;
	      menuCalled = 0;
  	    }
	  }

	}

  /** Never **/
  while(1);
  return 0;

}

