/****************************************************************************
* File Selection Menu
*
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include "sdfileio.h"

extern GXRModeObj *vmode;	/*** Graphics Mode Object ***/
extern u32 *xfb[2];		/*** Framebuffers ***/
extern int whichfb;		/*** Frame buffer toggle ***/

/** Bits from SD lib **/

#define PAGE_SIZE 14

/*** Use OGC built in font ***/
extern u8 console_font_8x16[];

static u32 forecolour = COLOR_WHITE;
static u32 backcolour = COLOR_BLACK;

/****************************************************************************
* MENU_DrawChar
****************************************************************************/
void MENU_DrawChar( int x, int y, char c, int style )
{
	u8 bits;
	int offset;
	int h,w;
	u32 colour[2];
	u32 scroffs;

	offset = c << 4;
	scroffs = ( y * 320 ) + ( x >> 1 );

	for( h = 0; h < 16; h++ )
	{
		bits = console_font_8x16[ offset++ ];

		if ( style )
		{
			for( w = 0; w < 8; w++ )
			{
				xfb[whichfb][scroffs + w] = ( bits & 0x80 ) ? forecolour : backcolour;
				bits <<= 1;
			}
		}
		else
		{
			for( w = 0; w < 4; w++ )
			{
				colour[0] = ( bits & 0x80 ) ? forecolour : backcolour;
				colour[1] = ( bits & 0x40 ) ? forecolour : backcolour;

				xfb[whichfb][scroffs + w] = ( colour[0] & 0xFFFF00FF ) | ( colour[1] & 0x0000FF00 );
				bits <<= 2;
			}
		}

		scroffs += 320;
	}
}

/****************************************************************************
* MENU_DrawString
****************************************************************************/
void MENU_DrawString( int x, int y, char *msg, int style )
{
	int i,len;
	
	/* Centred ? */	
	if ( x == -1 )
	{
		if ( style )
			x = strlen(msg) << 4;
		else
			x = strlen(msg) << 3;	

		x = ( 640 - x ) >> 1;
	}
	if((int)strlen(msg) < 36)
		len = (int)strlen(msg);
	else
		len = 36;
	for ( i = 0; i < len; i++ )
	{
		MENU_DrawChar(x,y,msg[i],style);
		x += ( style ? 16 : 8 );
	}
}

/****************************************************************************
* MENU_Draw
****************************************************************************/
int MENU_Draw( int max, int current, int offset )
{
	int i;
	int ypos = 30;
	int xpos = 30;

	for ( i = offset; i < max && ( ( i - offset ) < PAGE_SIZE ); i++ )
	{
		if ( i == current )
		{
			forecolour = COLOR_BLACK;
			backcolour = COLOR_WHITE;
		}
		else
		{
			forecolour = COLOR_WHITE;
			backcolour = COLOR_BLACK;
		}
		MENU_DrawString( xpos, ypos, direntries[i], 1);
		ypos += 16;		
	}
}
#ifdef WII_BUILD
#include <math.h>
#include <wiiuse/wpad.h>
extern int isClassicAvailable;
extern int isWiimoteAvailable;
extern void setup_controllers();
#endif

/****************************************************************************
* MENU_GetLoadFile
*
* Returns the filename of the selected file
***************************************************************************/
char *MENU_GetLoadFile( char *whichdir )
{
	int count;
	char *p = NULL;
	int quit = 0;
	int redraw = 1;
	int current = 0;
	int offset = 0;
	u16 buttons;
	int do_DOWN = 0;
	int do_UP = 0;
	int do_A = 0;
	
	count = gen_getdir( whichdir );

	
	if ( count == 0 )
	{
		printf("No ROM files in %s\n", whichdir);
		while(1);
	}
#ifdef WII_BUILD
	setup_controllers();
#endif
	if ( count == 1 )
		return (char*)direntries[0];

	/* Do menu */
	while ( !quit )
	{
		if ( redraw )
		{
			whichfb ^= 1;
			VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
			MENU_Draw( count, current, offset );
			VIDEO_SetNextFramebuffer(xfb[whichfb]);
			VIDEO_Flush();
			VIDEO_WaitVSync();
			redraw = 0;
		}
#ifdef WII_BUILD
		WPAD_ScanPads();
		WPADData *wpad;
		wpad = WPAD_Data(0);
		int b = wpad->exp.classic.btns;
		unsigned short b1 = wpad->btns_d;
		if (isClassicAvailable){
			if (b & CLASSIC_CTRL_BUTTON_DOWN){
				do_DOWN = 1;
				do{WPAD_ScanPads(); wpad = WPAD_Data(0);}
				while (WPAD_ButtonsHeld(0));	
			}
			else if (b & CLASSIC_CTRL_BUTTON_UP){
				do_UP = 1; 
				do{WPAD_ScanPads(); wpad = WPAD_Data(0);}
				while (WPAD_ButtonsHeld(0));	
			}
			else if (b & CLASSIC_CTRL_BUTTON_A){
				do_A = 1;
				do{WPAD_ScanPads(); wpad = WPAD_Data(0);}
				while (WPAD_ButtonsHeld(0));	
			}
		}
		if (isWiimoteAvailable){
			if (b1 & WPAD_BUTTON_LEFT){
				do_DOWN = 1;
				do{WPAD_ScanPads(); wpad = WPAD_Data(0);}
				while (WPAD_ButtonsHeld(0));	
			}
			else if (b1 & WPAD_BUTTON_RIGHT){
				do_UP = 1;			
				do{WPAD_ScanPads(); wpad = WPAD_Data(0);}
				while (WPAD_ButtonsHeld(0));	
			}
			else if (b1 & WPAD_BUTTON_2){
				do_A = 1;			
				do{WPAD_ScanPads(); wpad = WPAD_Data(0);}
				while (WPAD_ButtonsHeld(0));	
			}
		}
#endif	
		buttons = PAD_ButtonsDown(0);
		if(buttons & PAD_BUTTON_DOWN)
			do_DOWN = 1;
		else if(buttons & PAD_BUTTON_UP)
			do_UP = 1;
		else if(buttons & PAD_BUTTON_A)
			do_A = 1;
		if (do_DOWN)
		{
			do_DOWN=0;
			current++;
			if ( current == count )
				current = 0;

			if ( ( current % PAGE_SIZE ) == 0 )
				offset = current;

			redraw = 1;
		}
		else
		{
			if ( do_UP )
			{
				do_UP=0;
				current--;
				if ( current < 0 ) current = count - 1;
				offset = ( current / PAGE_SIZE ) * PAGE_SIZE;
				redraw = 1;
			}
			else
			{
				if ( do_A )
				{
					do_A=0;
					quit = 1;
					p = (char*)direntries[current];
				}
			}
		}
	}

	whichfb ^= 1;
	VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_BLACK);
	forecolour = COLOR_WHITE;
	backcolour = COLOR_BLACK;
	MENU_DrawString(-1, 240, "Loading ... Wait", 1);
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	return p;		
}

