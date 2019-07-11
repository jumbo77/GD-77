/*
 * Copyright (C)2019 Roger Clark. VK3KYY / G4KYF
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "menu/menuSystem.h"
#include "fw_settings.h"


static void updateScreen();
static void handleEvent(int buttons, int keys, int events);

int menuTxScreen(int buttons, int keys, int events, bool isFirstRun)
{
	if (isFirstRun)
	{
		gMenusCurrentItemIndex=180000;
		updateScreen();
	    GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
	    GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 1);
	    trxSetFrequency(currentChannelData->txFreq);
	    txstartdelay=0;
	    txstopdelay=0;
		trxIsTransmitting=true;
	    trx_setTX();
		trxSetCTCSS(currentChannelData->txTone);// This seems to need to be set after the radio is set into transmit mode.
	}
	else
	{
		if ((gMenusCurrentItemIndex%1000)==0)
		{
			updateScreen();
		}
		gMenusCurrentItemIndex--;
		if (gMenusCurrentItemIndex<0)
		{
			gMenusCurrentItemIndex=0;
		}

		handleEvent(buttons, keys, events);
	}
	return 0;
}

static void updateScreen()
{
	char buf[8];
	UC1701_clearBuf();
	sprintf(buf,"%d",gMenusCurrentItemIndex/1000);
	UC1701_printCentered(20, buf,UC1701_FONT_16x32);

	UC1701_render();
	displayLightOverrideTimeout(-1);
}


static void handleEvent(int buttons, int keys, int events)
{
	if ((buttons & BUTTON_PTT)==0)
	{
		trxIsTransmitting=false;
		if (txstopdelay>0)
		{
			txstopdelay--;
		}
		else
		{
			GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 0);
			trx_setRX();
			menuSystemPopPreviousMenu();
		}
	}
}

