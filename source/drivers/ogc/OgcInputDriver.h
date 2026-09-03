/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcInputDriver.h
 ***************************************************************************/
#pragma once
#include "../InputDriver.h"

class OgcInputDriver : public InputDriver
{
	public:
		OgcInputDriver();
		~OgcInputDriver() override;

		void init() override;
		void shutdown() override;
		void update() override;
		void setRumble(int channel, bool rumble) override;
		void setGameRumble(int channel, int frames) override;
		void ensureGameRumble(int channel, int frames) override;
		void setContinuousRumble(int channel, bool continuous) override;

	private:
		bool rumbleRequest[4];
		int menuRumbleFrames[4];
		int gameRumbleFrames[4];
		bool continuousRumble[4];
		int continuousRumbleCount[4];
		int silenceFrames[4];
};
