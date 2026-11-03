/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * InputDriver.h
 *
 * Platform input backend GuiElements delegate to.
 * Exactly one driver implements this and assigns the single global instance.
 ***************************************************************************/
#pragma once

enum {
	WIIMOTE_ORIENTATION_VERTICAL = 0,
	WIIMOTE_ORIENTATION_HORIZONTAL,
	WIIMOTE_ORIENTATION_LENGTH
};

class InputDriver
{
	public:
		virtual ~InputDriver() = default;
		
		virtual void init() = 0;
		virtual void shutdown() = 0;
		
		//! Polls the hardware and dispatches InputPadData payloads
		virtual void update() = 0;
		
		//! Requests a rumble event on the specified controller channel
		virtual void setRumble(int channel, bool rumble) = 0;
		virtual void setGameRumble(int channel, int frames) = 0;
		virtual void ensureGameRumble(int channel, int frames) = 0;
		virtual void setContinuousRumble(int channel, bool continuous) = 0;
		
		void setRumbleEnabled(bool enabled) { rumbleEnabled = enabled; }
		bool isRumbleEnabled() const { return rumbleEnabled && !rumbleHalted; }
		void haltRumble() { rumbleHalted = true; stopAllRumble(); }
		void setWiimoteOrientation(int orientation) { wiimoteOrientation = orientation; }
		int getWiimoteOrientation() const { return wiimoteOrientation; }
	
	protected:
		//! Stops every motor on every channel and clears all pending rumble state
		virtual void stopAllRumble() = 0;

		bool rumbleEnabled = true;
		bool rumbleHalted = false;
		int wiimoteOrientation = WIIMOTE_ORIENTATION_VERTICAL;
};
