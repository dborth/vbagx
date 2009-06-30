/****************************************************************************
 * Visual Boy Advance GX
 *
 * Carl Kenner February 2009
 *
 * wiiusbsupport.cpp
 *
 * Wii USB Keyboard and USB Mouse support
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <ogc/ipc.h>
#include <wiiuse/wpad.h>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>

#include "wiiusbsupport.h"

#ifdef HW_RVL

#define USB_CLASS_HID				0x03
#define USB_SUBCLASS_BOOT			0x01
#define USB_PROTOCOL_KEYBOARD		0x01
#define USB_PROTOCOL_MOUSE			0x02

static int mouse_vid = 0;
static int mouse_pid = 0;
#define DEVLIST_MAXSIZE				0x08

#define USB_REQ_GETPROTOCOL			0x03
#define USB_REQ_SETPROTOCOL			0x0B
#define USB_REQ_GETREPORT			0x01
#define USB_REQ_SETREPORT			0x09
#define USB_REPTYPE_INPUT			0x01
#define USB_REPTYPE_OUTPUT			0x02
#define USB_REPTYPE_FEATURE			0x03

#define USB_REQTYPE_GET				0xA1
#define USB_REQTYPE_SET				0x21

static u8 mouseconfiguration = 0;
static u32 mouseinterface = 0;
static u32 mousealtInterface = 0;

u8 DownUsbKeys[256];
u8 DownUsbShiftKeys = 0;

s32 MouseDirectInputX = 0;
s32 MouseDirectInputY = 0;

static bool KeyboardStarted=false, MouseStarted=false;
static s32 KeyboardHandle=0, MouseHandle=0;

typedef struct
{
	u32 message;
	u32 id; // direction
	u8 modifiers;
	u8 unknown;
	u8 keys[6];
	u8 pad[16];
}TKeyData;

static TKeyData KeyData ATTRIBUTE_ALIGN(32);
static signed char *MouseData = NULL;

static u8 OldKeys[6];
static u8 OldShiftKeys;

static bool StopKeyboard = true;

void KeyPress(u8 key)
{
	DownUsbKeys[key] = 1;
}
void KeyRelease(u8 key)
{
	DownUsbKeys[key] = 0;
}

bool AnyKeyDown()
{
	int i;
	for (i=4; i<=231;i++)
	{
		if (DownUsbKeys[i]) return true;
	}
	return false;
}

s32 KeyboardCallback(int ret,void * none)
{
	if (KeyboardHandle<0 || KeyData.message==0x7fffffff)
	return 0;
	if(KeyData.message==0)
	{
		// keyboard connected!
	}
	else if(KeyData.message==1)
	{
		// keyboard disconnected!
	}
	else if(KeyData.message==2)
	{
		// key event
		DownUsbShiftKeys = KeyData.modifiers;
		u8 p = DownUsbShiftKeys & (~OldShiftKeys);
		if (p & 0x01) KeyPress(KB_LCTRL);
		if (p & 0x02) KeyPress(KB_LSHIFT);
		if (p & 0x04) KeyPress(KB_LALT);
		if (p & 0x08) KeyPress(KB_LWIN);
		if (p & 0x10) KeyPress(KB_RCTRL);
		if (p & 0x20) KeyPress(KB_RSHIFT);
		if (p & 0x40) KeyPress(KB_RALT);
		if (p & 0x80) KeyPress(KB_RWIN);
		p = OldShiftKeys & (~DownUsbShiftKeys);
		if (p & 0x01) KeyRelease(KB_LCTRL);
		if (p & 0x02) KeyRelease(KB_LSHIFT);
		if (p & 0x04) KeyRelease(KB_LALT);
		if (p & 0x08) KeyRelease(KB_LWIN);
		if (p & 0x10) KeyRelease(KB_RCTRL);
		if (p & 0x20) KeyRelease(KB_RSHIFT);
		if (p & 0x40) KeyRelease(KB_RALT);
		if (p & 0x80) KeyRelease(KB_RWIN);
		// check each key to see if is in the list of old keys, if not it was pressed
		for (int i=0; i<6; i++)
		{
			if (KeyData.keys[i]==1)
			{
				break; // too many keys held down at once, so no key data except modifiers
			}
			else if (KeyData.keys[i])
			{
				bool found = false;
				for (int old=0; old<6; old++)
				{
					if (OldKeys[old]==KeyData.keys[i])
					{
						found = true;
						break;
					}
				}
				if (!found) KeyPress(KeyData.keys[i]);
			}
		}
		// check each old key to see if is in the list of keys, if not it was released
		for (int old=0; old<6; old++)
		{
			if (OldKeys[old])
			{
				bool found = false;
				for (int i=0; i<6; i++)
				{
					if (OldKeys[old]==KeyData.keys[i])
					{
						found = true;
						break;
					}
				}
				if (!found) KeyRelease(OldKeys[old]);
			}
		}
		// Update old keys, unless too many keys were held down
		if (KeyData.keys[0]!=1)
		memcpy(OldKeys, KeyData.keys, 6);
		OldShiftKeys = DownUsbShiftKeys;
	}

	// no keyboard message
	KeyData.message=0x7fffffff;
	// Request another keyboard message when one is ready
	if (!StopKeyboard)
	IOS_IoctlAsync(KeyboardHandle,1,(void *) &KeyData, 16,(void *) &KeyData, 16, KeyboardCallback, NULL);
	return 0;
}

void StartWiiKeyboardMouse()
{
	memset(DownUsbKeys, 0, sizeof(DownUsbKeys));

	if (!KeyboardStarted)
	{
		USB_Initialize();
		StartWiiMouse();
		KeyboardHandle=IOS_Open("/dev/usb/kbd", 1);
		if (KeyboardHandle<0)
		{
			// Error!
		}
		//sleep(2);
		KeyData.message=0x7fffffff;
		StopKeyboard = false;
		if(KeyboardHandle>=0)
		IOS_IoctlAsync(KeyboardHandle,1,(void *) &KeyData, 16,(void *) &KeyData, 16, KeyboardCallback, NULL);
		KeyboardStarted = true;
	}
	else
	{
		if(KeyboardHandle>=0)
		IOS_IoctlAsync(KeyboardHandle,1,(void *) &KeyData, 16,(void *) &KeyData, 16, KeyboardCallback, NULL);
	}
}

void StopWiiKeyboard()
{
	StopKeyboard = true;
	IOS_Close(KeyboardHandle);
}

s32 MouseCallback(s32 result, void *usrdata)
{

	if (result>0)
	{
		u8 button = MouseData[0];
		int deltax = (s8)MouseData[1];
		int deltay = (s8)MouseData[2];
		MouseDirectInputX += deltax;
		MouseDirectInputY += deltay;
		DownUsbKeys[KB_MOUSEL] = (button & 1);
		DownUsbKeys[KB_MOUSER] = (button & 2);
		DownUsbKeys[KB_MOUSEM] = (button & 4);

		USB_ReadIntrMsgAsync(MouseHandle, 0x81, 4, MouseData, MouseCallback, 0);
	}
	else
	{
		MouseStarted=0;
		MouseHandle=0;
	}
	return 0;
}

static int wii_find_mouse()
{
	s32 fd=0;
	static u8 *buffer = 0;

	if (!buffer)
	{
		buffer = (u8*)memalign(32, DEVLIST_MAXSIZE << 3);
	}
	if(buffer == NULL)
	{
		return -1;
	}
	memset(buffer, 0, DEVLIST_MAXSIZE << 3);

	u8 dummy;
	u16 vid,pid;

	if (USB_GetDeviceList("/dev/usb/oh0", buffer, DEVLIST_MAXSIZE, 0, &dummy) < 0)
	{

		free(buffer);
		buffer =0;
		return -2;
	}

	u8 mouseep;
	u32 mouseep_size;

	int i;

	for(i = 0; i < DEVLIST_MAXSIZE; i++)
	{
		memcpy(&vid, (buffer + (i << 3) + 4), 2);
		memcpy(&pid, (buffer + (i << 3) + 6), 2);

		if ((vid==0)&&(pid==0))
		continue;
		fd =0;

		int err = USB_OpenDevice("oh0",vid,pid,&fd);
		if (err<0)
		{
			continue;
		}
		else
		{
			// error!
		}

		u32 iConf, iInterface;
		usb_devdesc udd;
		usb_configurationdesc *ucd;
		usb_interfacedesc *uid;
		usb_endpointdesc *ued;

		USB_GetDescriptors(fd, &udd);

		for(iConf = 0; iConf < udd.bNumConfigurations; iConf++)
		{
			ucd = &udd.configurations[iConf];
			for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
			{
				uid = &ucd->interfaces[iInterface];

				if ( (uid->bInterfaceClass == USB_CLASS_HID) && (uid->bInterfaceSubClass == USB_SUBCLASS_BOOT) &&
						(uid->bInterfaceProtocol== USB_PROTOCOL_MOUSE))
				{
					int iEp;
					for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
					{
						ued = &uid->endpoints[iEp];
						mouse_vid = vid;
						mouse_pid = pid;

						mouseep = ued->bEndpointAddress;
						mouseep_size = ued->wMaxPacketSize;
						mouseconfiguration = ucd->bConfigurationValue;
						mouseinterface = uid->bInterfaceNumber;
						mousealtInterface = uid->bAlternateSetting;

					}
					break;
				}

			}
		}
		USB_FreeDescriptors(&udd);
		USB_CloseDevice(&fd);

	}
	if (mouse_pid!=0 || mouse_vid!=0) return 0;
	return -1;

}

void StartWiiMouse()
{
	if (!MouseStarted)
	{

		if (wii_find_mouse()!=0) return;

		if (USB_OpenDevice("oh0", mouse_vid, mouse_pid, &MouseHandle)<0)
		{
			return;
		}
		if (!MouseData)
		{
			MouseData = (signed char*)memalign(32, 20);
		}

		//set boot protocol
		USB_WriteCtrlMsg(MouseHandle,USB_REQTYPE_SET,USB_REQ_SETPROTOCOL,0,0,0,0);
		USB_ReadIntrMsgAsync(MouseHandle, 0x81, 4, MouseData, MouseCallback, 0);

		MouseStarted=true;
	}
}

#else // GameCube stub

u8 DownUsbKeys[256];

bool AnyKeyDown()
{
	return false;
}
void StartWiiKeyboardMouse()
{
	memset(DownUsbKeys, 0, sizeof(DownUsbKeys));
}

#endif
