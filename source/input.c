/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       input.c
**
** DATE CREATED:    10 July 2007
**
** AUTHOR:			Tareq Hafez    
**
** DESCRIPTION:     This module handles smartcard, magstripe and keyboard entry
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Include Files
**-----------------------------------------------------------------------------
*/

/*
** Standard include files.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** Project include files.
*/
#include <svc.h>
#include <svc_sec.h>
#include <errno.h>

#ifdef __VMAC
#	include "eeslapi.h"
#	include "logsys.h"
#	include "devman.h"
#	include "vmac.h"
#	include "version.h"
#endif

/*
** Local include files
*/
#include "auris.h"
#include "input.h"
#include "display.h"
#include "timer.h"
#include "irisfunc.h"
#include "iris.h"
#include "comms.h"
#include "security.h"
#include "alloc.h"

/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/
const struct
{
	T_KEYBITMAP	tKeyBitmap;
	uchar bKeyCode;
} sKey[] = 
{   {KEY_0_BIT, KEY_0}, {KEY_1_BIT, KEY_1}, {KEY_2_BIT, KEY_2}, 
	{KEY_3_BIT, KEY_3}, {KEY_4_BIT, KEY_4}, {KEY_5_BIT, KEY_5}, 
	{KEY_6_BIT, KEY_6}, {KEY_7_BIT, KEY_7}, {KEY_8_BIT, KEY_8}, 
	{KEY_9_BIT, KEY_9},
	{KEY_FUNC_BIT, KEY_FUNC}, {KEY_CNCL_BIT, KEY_CNCL}, {KEY_ASTERISK_BIT, KEY_ASTERISK},
	{KEY_OK_BIT, KEY_OK}, {KEY_CLR_BIT, KEY_CLR},  {KEY_LCLR_BIT, KEY_LCLR},
	{KEY_SK1_BIT, KEY_SK1}, {KEY_SK2_BIT, KEY_SK2}, {KEY_SK3_BIT, KEY_SK3},  {KEY_SK4_BIT, KEY_SK4},
	{KEY_F0_BIT, KEY_F0}, {KEY_F1_BIT, KEY_F1}, {KEY_F2_BIT, KEY_F2},  {KEY_F3_BIT, KEY_F3}, {KEY_F4_BIT, KEY_F4},  {KEY_F5_BIT, KEY_F5},
	{KEY_NO_BITS, KEY_NONE}
};

const char HiddenString[] = "********************";

/*
**-----------------------------------------------------------------------------
** Module variable definitions and initialisations.
**-----------------------------------------------------------------------------
*/
static bool Override;
static ulong Number;
static char String[MAX_COL*3+1];
static bool HiddenAttribute;

extern int prtHandle;
extern int conHandle;
extern int cryptoHandle;
int mcrHandle = 0;
bool numberEntry = false;

#ifdef __VMAC

	int timerID = -1;
	int wait_10003 = -1;
	int active = -1;
	int firstInit = -1;

	typedef struct
	{
		char logicalName[17];
		char lowPAN[20];
		char highPAN[20];
	} typeRedirectPAN;
	
	typeRedirectPAN * redirectPAN = NULL;

	int maxRedirectPAN = 0;

	int hotKey = -1;
	char hotKeyApp[30];
	int hotKeyEvent;

#else
	#define	LOG_PRINTF(...)
#endif

int ser_line0;
int ser_line1;

//
//-----------------------------------------------------------------------------
// FUNCTION   : InpSetOverride
//
// DESCRIPTION:	Sets the initial override condition of an input
//
// PARAMETERS:	override	<=	boolean.
//
// RETURNS:		NONE
//	
//-----------------------------------------------------------------------------
//
void InpSetOverride(bool override)
{
	Override = override;
}


//
//-----------------------------------------------------------------------------
// FUNCTION   : InpSetString
//
// DESCRIPTION:
//	Sets the entry string to a value. This can be used to
//	initialise the string or set it to a default value.
//
// PARAMETERS:
//	value	<=  The value to set the string to.
//	hidden	<=  Indicates if the string entry should be hidden
//
// RETURNS:
//	NONE
//-----------------------------------------------------------------------------
//
void InpSetString(char * value, bool hidden, bool override)
{
	numberEntry = false;

	memset(String, 0, sizeof(String));
	strcpy(String, value);

	HiddenAttribute = hidden;

	Override = override;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : InpGetString
//
// DESCRIPTION:
//	Returns the last string entered using a String Entry field.
//
// PARAMETERS:
//	NONE
//
// RETURNS:
//	(char *) The string entered
//-----------------------------------------------------------------------------
//
char * InpGetString(void)
{
	if (numberEntry)
		return ltoa(Number, String, 10);
	else
		return String;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : InpSetNumber
**
** DESCRIPTION:
**	Sets the entry number to a value. This can be used to
**	initialise the number or set it to a default value.
**
** PARAMETERS:
**	value		<=  The value to set the number to.
**	override	<=	Whether the first numeric entry should override the number (true)
**					or just append to it (false)
**
** RETURNS:
**	NONE
**-----------------------------------------------------------------------------
*/
void InpSetNumber(ulong value, bool override)
{
	numberEntry = true;
	Number = value;
	HiddenAttribute = false;
	Override = override;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : InpGetNumber
**
** DESCRIPTION:
**	Returns the last number entered using an Number Entry field.
**
** PARAMETERS:
**	NONE
**
** RETURNS:
**	(ulong) The number entered
**-----------------------------------------------------------------------------
*/
ulong InpGetNumber(void)
{
    return Number;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : InpTurnOn
**
** DESCRIPTION:	Turns on all open resources
**
**
** PARAMETERS:	None
**
**
** RETURNS:		
**
**-----------------------------------------------------------------------------
*/
void InpTurnOn(void)
{
	DispInit();

	if (ser_line0)
		__ser_reconnect(0);

	if (ser_line1)
		__ser_reconnect(1);

	ser_line0 = ser_line1 = -1;

	if (cryptoHandle == -2)
		SecurityInit();
	else if (cryptoHandle == -1)
		cryptoHandle = open("/dev/crypto", 0);
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : InpTurnOff
**
** DESCRIPTION:	Turns off all open resources
**
**
** PARAMETERS:	None
**
**
** RETURNS:		
**
**-----------------------------------------------------------------------------
*/
int InpTurnOff(bool serial0)
{
	int released = -1;

	// Just in case it was left connected
	__pstn_disconnect();

	// Disconnect TCP and ethernet ports if open
	__tcp_disconnect_completely();

	if (conHandle != -1)
	{
		close(conHandle);
		LOG_PRINTF(("Releasing console..."));
		conHandle = -1;
		released = 1;
	}

	if (mcrHandle)
	{
		close(mcrHandle);
		LOG_PRINTF(("Releasing mag reader..."));
		mcrHandle = 0;
		released = 1;
	}

	if (ser_line0 == -1)
	{
		if (serial0)
		{
			ser_line0 = ____ser_disconnect(0);
			if (ser_line0)
			{
				LOG_PRINTF(("Releasing com1..."));
				released = 1;
			}
		}
		else ser_line0 = 0;
	}

	if (ser_line1 == -1)
	{
		ser_line1 = ____ser_disconnect(1);
		if (ser_line1)
		{
			LOG_PRINTF(("Releasing com2..."));
			released = 1;
		}
	}

	if (prtHandle != -1)
	{
		close(prtHandle);
		LOG_PRINTF(("Releasing printer (COM4)..."));
		prtHandle = -1;
		released = 1;
	}

	if (cryptoHandle > -1)
	{
		close(cryptoHandle);
		LOG_PRINTF(("Releasing crypto (CRYPTO)..."));
		cryptoHandle = -1;
		released = 1;
	}

#ifndef __VX670
	CommsReInitPSTN();
#endif

	return released;
}



#ifdef __VMAC
/*
**-----------------------------------------------------------------------------
** FUNCTION   : VMACInactive
**
** DESCRIPTION:	Waits for an event in a VMAC loop until activated
**
** PARAMETERS:	NONE
**
** RETURNS:		Activation status
**
**-----------------------------------------------------------------------------
*/
void VMACHotKey(char * appName, int event)
{
	strcpy(hotKeyApp, appName);
	hotKeyEvent = event;
	hotKey = 1;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : VMACInactive
**
** DESCRIPTION:	Waits for an event in a VMAC loop until activated
**
** PARAMETERS:	NONE
**
** RETURNS:		Activation status
**
**-----------------------------------------------------------------------------
*/
void VMACInactive(void)
{
	if (timerID >= 0) clr_timer(timerID);

	LOG_PRINTF(("VMAC IRIS SET TO INACTIVE MODE"));

	active = -1;
	wait_10003 = 1;
	timerID = set_timer(10000, EVT_TIMER);
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : VMACDeactivate
**
** DESCRIPTION:	Waits for an event in a VMAC loop until activated
**
** PARAMETERS:	NONE
**
** RETURNS:		Activation status
**
**-----------------------------------------------------------------------------
*/
void VMACDeactivate(void)
{
	unsigned short event = 15001;

	if (timerID >= 0) clr_timer(timerID);

	LOG_PRINTF(("Handling DE_ACT EVENT (deactivate)"));

	if (InpTurnOff(false) == 1)
		EESL_send_event("DEVMAN", RES_COMPLETE_EVENT, (unsigned char*) &event, sizeof(short));

	wait_10003 = -1;
	active = -1;
	timerID = set_timer(5000, EVT_TIMER);
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : VMACLoop
**
** DESCRIPTION:	Waits for an event in a VMAC loop until activated
**
** PARAMETERS:	NONE
**
** RETURNS:		Activation status
**
**-----------------------------------------------------------------------------
*/
int VMACLoop(void)
{
	int status = 0;
	long event;
	char iris_vmac[30];

	iris_vmac[0] = '1';
	get_env("IRIS_VMAC", iris_vmac, sizeof(iris_vmac));

	// Indicate that the application is ready to receive events
//	EESL_appl_ready();

	if ((strcmp(currentObject, "IDLE") == 0 || strcmp(currentObject, "iPAY") == 0) && firstInit == -1 && active == 1 && iris_vmac[0] != '0')
	{
		unsigned short event = 15001;
		firstInit = 1;
		LOG_PRINTF(("Handling DE_ACT EVENT (first)"));

		InpTurnOff(true);

		EESL_send_event("DEVMAN", RES_COMPLETE_EVENT, (unsigned char*) &event, sizeof(short));
//		timerID = set_timer(2000, EVT_TIMER);				
		timerID = set_timer(5000, EVT_TIMER);				
		active = -1;
	}

	if (hotKey != -1 && iris_vmac[0] != '0')
	{
		EESL_send_event(hotKeyApp, hotKeyEvent, NULL, 0);
		VMACDeactivate();
	}
	hotKey = -1;

	do
	{
		if (active == -1)
			event = wait_event();
		else
			event = read_event();

		if (strcmp(currentObject, "IDLE") == 0 && iris_vmac[0] != '0')
			enable_hot_key();
		else
			disable_hot_key();

//		LOG_PRINTF(("*** ACTIVE *** %d %s %d", active, currentObject, event));

		if (event & EVT_TIMER)
		{
			// Check if 10003 timer
			if (wait_10003 == 1)
			{
				wait_10003 = -1;
				if (timerID >= 0) clr_timer(timerID);
				timerID = -1;
				active = 1;
			}
			else
			{
				// Self activate periodically
				LOG_PRINTF(("Self Activate attempt"));
				EESL_send_devman_event("IRIS", 15001, NULL, 0);
				if (timerID >= 0) clr_timer(timerID);
				timerID = -1;
			}
		}

		if ((event & EVT_DEACTIVATE) && active == 1 && iris_vmac[0] != '0')
			VMACDeactivate();

		if (event & EVT_PIPE || EESL_queue_count())
		{
			unsigned short evtID;
			unsigned char data2[300];
			unsigned short dataSize;
			char senderName[21];

			LOG_PRINTF(("IRIS PIPE EVENT %d",event));

			do
			{
				memset(senderName, 0, sizeof(senderName));
				evtID = EESL_read_cust_evt(data2, sizeof(data2), &dataSize, senderName);

				if (evtID == APPLICATION_INIT_EVENT)
				{
					LOG_PRINTF(("VMAC IRIS INIT using %s",GetVMACLibVersion()));
					EESL_send_devman_event("IRIS", 15001, NULL, 0);
				}
				else if (evtID == CONSOLE_AVAILABLE_EVENT)
				{
					LOG_PRINTF(("Handling Console Available EVENT"));
				}
				else if (evtID == APP_RES_SET_UNAVAILABLE)
				{
					if (timerID >= 0) clr_timer(timerID);
					LOG_PRINTF(("Handling RES SET UNAVAILABLE EVENT"));
					timerID = set_timer(10000, EVT_TIMER);								
				}
/*				else if (	strcmp(currentObject, "IDLE") &&
							(evtID == CONSOLE_REQUEST_EVENT || evtID == MAG_READER_REQUEST_EVENT || evtID == BEEPER_REQUEST_EVENT || evtID == CLOCK_REQUEST_EVENT ||
							evtID == COMM_1_REQUEST_EVENT || evtID == COMM_2_REQUEST_EVENT || evtID == COMM_3_REQUEST_EVENT || evtID == COMM_4_REQUEST_EVENT || evtID == ETH_1_REQUEST_EVENT)
						)
				{
					EESL_send_event("DEVMAN", RES_REJECT_EVENT, (unsigned char*) &event, sizeof(short));
				}
				else if (evtID == CONSOLE_REQUEST_EVENT)
				{
					unsigned short event = 15001;

					if (timerID >= 0) clr_timer(timerID);

					LOG_PRINTF(("Handling DE_ACT EVENT - console request event"));
					if (conHandle != -1)
					{
						close(conHandle);
						EESL_send_event("DEVMAN", CONSOLE_RELEASED_EVENT, (unsigned char*) &event, sizeof(short));
						conHandle = -1;
					}

					wait_10003 = -1;
					active = -1;
					timerID = set_timer(5000, EVT_TIMER);
				}
				else if (evtID == MAG_READER_REQUEST_EVENT)
				{
					unsigned short event = 15001;

					if (timerID >= 0) clr_timer(timerID);

					LOG_PRINTF(("Handling DE_ACT EVENT - mag reader request release"));
					if (mcrHandle)
					{
						close(mcrHandle);
						EESL_send_event("DEVMAN", MAG_READER_RELEASED_EVENT, (unsigned char*) &event, sizeof(short));
						mcrHandle = 0;
					}

					wait_10003 = -1;
					active = -1;
					timerID = set_timer(5000, EVT_TIMER);
				}
				else if (evtID == COMM_1_REQUEST_EVENT)
				{
					unsigned short event = 15001;

					if (timerID >= 0) clr_timer(timerID);

					LOG_PRINTF(("Handling DE_ACT EVENT - comm 1 request release"));
					ser_line0 = ____ser_disconnect(0);
					if (ser_line0)
						EESL_send_event("DEVMAN", COMM_1_RELEASED_EVENT, (unsigned char*) &event, sizeof(short));

					wait_10003 = -1;
					active = -1;
					timerID = set_timer(5000, EVT_TIMER);
				}
				else if (evtID == COMM_2_REQUEST_EVENT)
				{
					unsigned short event = 15001;

					if (timerID >= 0) clr_timer(timerID);

					LOG_PRINTF(("Handling DE_ACT EVENT - comm 1 request release"));
					ser_line1 = ____ser_disconnect(1);
					if (ser_line1)
						EESL_send_event("DEVMAN", COMM_2_RELEASED_EVENT, (unsigned char*) &event, sizeof(short));

					wait_10003 = -1;
					active = -1;
					timerID = set_timer(5000, EVT_TIMER);
				}
*/				
				else if (evtID == CONSOLE_REQUEST_EVENT || evtID == MAG_READER_REQUEST_EVENT || evtID == BEEPER_REQUEST_EVENT || evtID == CLOCK_REQUEST_EVENT ||
						evtID == COMM_1_REQUEST_EVENT || evtID == COMM_2_REQUEST_EVENT || evtID == COMM_3_REQUEST_EVENT || evtID == COMM_4_REQUEST_EVENT || evtID == ETH_1_REQUEST_EVENT)
				{
					unsigned short event = 15001;

					if (iris_vmac[0] == '0')
						EESL_appl_busy();

					if (iris_vmac[0] != '0' && (wait_10003 == 1 || active == -1 || strcmp(currentObject, "IDLE") == 0))
					{
						if (timerID >= 0) clr_timer(timerID);

						LOG_PRINTF(("Handling DE_ACT EVENT"));

						if (InpTurnOff(false) == 1)
							EESL_send_event("DEVMAN", RES_COMPLETE_EVENT, (unsigned char*) &event, sizeof(short));

						wait_10003 = -1;
						active = -1;
						timerID = set_timer(5000, EVT_TIMER);
					}
					else
					{
						LOG_PRINTF(("NOT IDLING - REJECTING %s %d", currentObject, evtID));
						EESL_send_event("DEVMAN", RES_REJECT_EVENT, (unsigned char*) &evtID, sizeof(short));
					}
				}
				else if (evtID == 10001)
				{
					int i,j, k;
					char temp[300];

					LOG_PRINTF(("VMAC IRIS PAN RANGE using %s",GetVMACLibVersion()));
					sprintf(temp, "%*s", dataSize, data2);
					temp[dataSize] = '\0';
					LOG_PRINTF(("VMAC IRIS PAN REGISTRATION: %s", temp));

					// Store the logical name
					for (i = 0; i < dataSize && data2[i] && data2[i] != ','; i++)
						temp[i] = data2[i];
					temp[i++] = '\0';

					// Process the data
					for(;i < dataSize;)
					{
						// Find an empty pan range slot. Stop if none found
						for (j = 0; j < maxRedirectPAN; j++)
						{
							if (redirectPAN[j].logicalName[0] == '\0')
								break;
						}
						if (j == maxRedirectPAN)
						{
							maxRedirectPAN++;
							if (redirectPAN == NULL)
								redirectPAN = my_malloc(sizeof(typeRedirectPAN));
							else
								redirectPAN = my_realloc(redirectPAN, maxRedirectPAN * sizeof(typeRedirectPAN));
						}
						strcpy(redirectPAN[j].logicalName, temp);
						LOG_PRINTF(("VMAC IRIS PAN STORED: %d", j));

						// Now store the low pan range
						for (k = 0; k < 20 && data2[i] != '-' && i < dataSize; k++, i++)
							redirectPAN[j].lowPAN[k]  = data2[i];
						redirectPAN[j].lowPAN[k] = '\0';
						LOG_PRINTF(("VMAC IRIS LOW PAN RANGE (%d): %s", j, redirectPAN[j].lowPAN));
						if (data2[i] == '-') i++;
						if (i == dataSize) break;

						// Now store the high pan range
						for (k = 0; k < 20 && data2[i] != ',' && i < dataSize; k++, i++)
							redirectPAN[j].highPAN[k]  = data2[i];
						redirectPAN[j].highPAN[k] = '\0';
						LOG_PRINTF(("VMAC IRIS HIGH PAN RANGE (%d): %s", j, redirectPAN[j].highPAN));
						if (data2[i] == ',') i++;
					}
				}
				else if (evtID == 10007)
				{
					int i,j;
					char temp[300];

					LOG_PRINTF(("VMAC IRIS PAN RANGE DELETION using %s",GetVMACLibVersion()));
					sprintf(temp, "%*s", dataSize, data2);
					temp[dataSize] = '\0';
					LOG_PRINTF(("VMAC IRIS PAN DELETION: %s", temp));

					// Store the logical name
					for (i = 0; i < dataSize && data2[i] && data2[i] != ','; i++)
						temp[i] = data2[i];
					temp[i++] = '\0';

					// Now erase any existing pan ranges for this logical name
					for (j = 0; j < maxRedirectPAN; j++)
					{
						if (strcmp(redirectPAN[j].logicalName, temp) == 0)
						{
							redirectPAN[j].logicalName[0] = '\0';
							LOG_PRINTF(("VMAC IRIS PAN CLEANUP: %d", j));
						}
					}
				}
				else if (evtID == 15001)
				{
					if (timerID >= 0) clr_timer(timerID);

					LOG_PRINTF(("VMAC IRIS ACTIVATE using %s", GetVMACLibVersion()));

					InpTurnOn();

					active = 1;

					status = 1;
				}
				else if (evtID == 10003 && iris_vmac[0] != '0')
				{
					if (timerID >= 0) clr_timer(timerID);

					LOG_PRINTF(("VMAC IRIS MCR ACK [%d] using %s",data2[0],GetVMACLibVersion()));

					if (data2[0] == 0)
					{
						active = -1;
						wait_10003 = 1;
						timerID = set_timer(10000, EVT_TIMER);
					}
					else active = 1;
				}
				else if (evtID == 10005)
				{
					char message[100];
					int myStackIndex;

					if (IRIS_StackInitialised() == false)
						IRIS_StackInit(0);
					myStackIndex = stackIndex;

					LOG_PRINTF(("VMAC IRIS TID/MID REQUEST using %s", GetVMACLibVersion()));

					message[0] = '\0';

					// Get the TID
					IRIS_Eval("/iPAY_CFG/TID", false);
					if (myStackIndex != stackIndex && IRIS_StackGet(0))
					{
						strcat(message, IRIS_StackGet(0));
						IRIS_StackPop(stackIndex-myStackIndex);
					}
					else
					{
						IRIS_Eval("/iPAY_CFG0/TID", false);
						if (myStackIndex != stackIndex && IRIS_StackGet(0))
						{
							strcat(message, IRIS_StackGet(0));
							IRIS_StackPop(stackIndex-myStackIndex);
						}
					}

					strcat(message, ",");

					// Get the MID
					IRIS_Eval("/iPAY_CFG/MID", false);
					if (myStackIndex != stackIndex && IRIS_StackGet(0))
					{
						strcat(message, IRIS_StackGet(0));
						IRIS_StackPop(stackIndex-myStackIndex);
					}
					else
					{
						IRIS_Eval("/iPAY_CFG0/MID", false);
						if (myStackIndex != stackIndex && IRIS_StackGet(0))
						{
							strcat(message, IRIS_StackGet(0));
							IRIS_StackPop(stackIndex-myStackIndex);
						}
					}

					LOG_PRINTF(("Sending event 10006 to %s with DATA=\"%s\"", senderName, message));
					EESL_send_event(senderName, 10006, (unsigned char *) message, strlen(message));
				}
			} while (evtID != 0);
		}
	} while (active == -1);

	// Indicate that the application is NOT ready to receive events
//	EESL_appl_busy();

	return status;
}
#endif

/*
**-----------------------------------------------------------------------------
** FUNCTION   : InpGetKeyEvent
**
** DESCRIPTION:
**	Returns the next key or event as requested.
**
** PARAMETERS:	NONE
**
** RETURNS:
**		NOERROR
**-----------------------------------------------------------------------------
*/
uchar InpGetKeyEvent(T_KEYBITMAP keyBitmap, T_EVTBITMAP * evtBitmap, T_INP_ENTRY inpEntry, ulong timeout, bool largeFont, bool flush, bool * keyPress)
{
	TIMER_TYPE myTimer;
	uchar keyCode;
	char entry[MAX_COL*3+1];
	char inpStr[MAX_COL*3+1];
	T_EVTBITMAP myEvtBitmap = EVT_NONE;
	int lastPinStatus = 0;
	char numPinPress = 0;
//	int activeStatus;
	char curr_symbol[20];
	unsigned int curr_decimal;

    uchar index;
    bool displayEntry = true;
    bool override = Override;

    uchar lastKeyIndex = 0;
    uchar lastKey = 12;	    /* Dummy out-of-range value */
    const struct
    {
		uchar sequence[7];
    } keys[12] = {  {{'0','-',' ','+','_','!','@'}},
					{{'1','Q','Z','.','q','z','/'}},
					{{'2','A','B','C','a','b','c'}},
					{{'3','D','E','F','d','e','f'}},
					{{'4','G','H','I','g','h','i'}},
					{{'5','J','K','L','j','k','l'}},
					{{'6','M','N','O','m','n','o'}},
					{{'7','P','R','S','p','r','s'}},
					{{'8','T','U','V','t','u','v'}},
					{{'9','W','X','Y','w','x','y'}},
					{{'*',',','\'','"','`','$','%'}},
					{{'#','^','&','(',')','[',']'}}};

#ifdef __VMAC
	if (VMACLoop() == 1 && evtBitmap)
	{
		vmacRefresh = true;
		*evtBitmap = EVT_TIMEOUT;
		return KEY_NONE;
	}
//	activeStatus = VMACLoop();
#endif

    /* If we did not request any keys or events, go back */
    if (keyBitmap == KEY_NO_BITS && (evtBitmap == NULL || (evtBitmap && *evtBitmap == EVT_NONE)))
		return KEY_NONE;

	/* Adjust the last key and last key index to reflect the last character in the string if the string not empty */
	if (strlen(String))
	{
		for (lastKey = 0; lastKey < 12; lastKey++)
		{
			for (lastKeyIndex = 0; lastKeyIndex < 7; lastKeyIndex++)
			{
				if (String[strlen(String)-1] == keys[lastKey].sequence[lastKeyIndex])
					break;
			}
			if (lastKeyIndex < 7) break;
		}
	}

	if (flush)
	{
		// Flush the current keyboard entries
		while(read(conHandle, (char *) &keyCode, 1) == 1);

		/* Enable or disable any MCR tracks */
		if (evtBitmap && *evtBitmap & EVT_MCR)
		{
			char temp[6];
			if (mcrHandle == 0) mcrHandle = open(DEV_CARD, 0);
			read(mcrHandle, temp, sizeof(temp));
		}
		else
		{
			if (mcrHandle)
			{
				close(mcrHandle);
				mcrHandle = 0;
			}
		}
	}

    // If timeout is requested, arm the default timer
    if (evtBitmap && *evtBitmap & EVT_TIMEOUT)
		TimerArm(&myTimer, timeout);

	if (inpEntry.type == E_INP_PIN)
	{
		PINPARAMETER param;
		uchar data[20];

		// Set the next printing location
#if defined(__VX670)
//		DispText("KEY PIN", 3, 9, false, true, false);
		DispText("KEY PIN", 4, 9, false, true, false);
#elif defined(__VX810)
		DispText("KEY PIN", 4, 9, false, true, false);
#else
		DispText("KEY PIN", 2, 9, false, true, false);
#endif

		memset(&param, 0, sizeof(param));
#if defined(__VX670) || defined(__VX810)
		param.ucMin = inpEntry.row - 1;
#else
		param.ucMin = inpEntry.row + 1;
#endif
		param.ucMax = inpEntry.col + 1;
		param.ucEchoChar = '*';
		param.ucDefChar = ' ';
		if (keyBitmap & KEY_CLR_BIT)
			param.ucOption |= 0x10;
		if (inpEntry.length == 0)
			param.ucOption |= 0x02;

		iPS_SetPINParameter(&param);
		iPS_SelectPINAlgo(0x0B);
		iPS_RequestPINEntry(0, data);
	}

	if (inpEntry.type == E_INP_AMOUNT_ENTRY)
	{
		char result[30];
		result[0] = '\0';

		strcpy(&result[4], "/IRIS_CURRENCY/SYMBOL");
		IRIS_ResolveToSingleValue(result, false);
		if (IRIS_StackGet(0))
			strcpy(curr_symbol, IRIS_StackGet(0));
		else
			strcpy(curr_symbol, "$");
		IRIS_StackPop(1);

		strcpy(&result[4], "/IRIS_CURRENCY/DECIMAL");
		IRIS_ResolveToSingleValue(result, false);
		if (IRIS_StackGet(0))
			curr_decimal = atoi(IRIS_StackGet(0));
		else curr_decimal = 2;
		IRIS_StackPop(1);
	}

    do {

#ifdef __VMAC
		if (VMACLoop() == 1 && evtBitmap)
		{
			vmacRefresh = true;
			*evtBitmap = EVT_TIMEOUT;
			return KEY_NONE;
		}
#endif

		if (inpEntry.type == E_INP_PIN)
		{
			int status;
			PINRESULT result;

			iPS_GetPINResponse(&status, &result);
			if (status == 0)
				return KEY_OK;
			if (status == 6)
				return KEY_NO_PIN;
			else if (status == 5)
				return KEY_CNCL;
			else if (status == 0x0A)
				return KEY_CLR;
			else
			{
				if (status != lastPinStatus || result.encPinBlock[0] != numPinPress)
				{
					lastPinStatus = status;
					numPinPress = result.encPinBlock[0];
					if (evtBitmap && *evtBitmap & EVT_TIMEOUT)
						TimerArm(&myTimer, timeout);
				}
			}
		}
		else
		{
			/* If a number or amount display is requested, display it */
			if (displayEntry == true && inpEntry.type != E_INP_NO_ENTRY)
			{
				uchar entryOffset = 0;

				memset(entry, ' ', inpEntry.length);
				if (inpEntry.type == E_INP_AMOUNT_ENTRY)
				{
					int i;
					int multiplier = 1;
					for (i = 0; i < curr_decimal; i++) multiplier *= 10;
					sprintf(inpStr, "%s%ld", curr_symbol, Number/multiplier);
					if (curr_decimal)
						sprintf(&inpStr[strlen(inpStr)], "%s%0*ld", ".", curr_decimal, Number%multiplier);
				}
				else if (inpEntry.type == E_INP_PERCENT_ENTRY)
					sprintf(inpStr, "%ld.%02ld%%", Number/100, Number%100);
				else if (inpEntry.type == E_INP_DECIMAL_ENTRY)
				{
					int i;
					unsigned long divider = 1;

					for (i = 0; i < inpEntry.decimalPoint; i++)
						divider *= 10;

					sprintf(inpStr, "%ld.%0*ld", Number/divider, inpEntry.decimalPoint, Number%divider);
				}
				else if (inpEntry.type == E_INP_DATE_ENTRY)
					sprintf(inpStr, "%c%c/%c%c/%c%c",	String[0]?String[0]:'D', String[1]?String[1]:'D',
														String[2]?String[2]:'M', String[3]?String[3]:'M',
														String[4]?String[4]:'Y', String[5]?String[5]:'Y');
				else if (inpEntry.type == E_INP_MMYY_ENTRY)
					sprintf(inpStr, "%c%c/%c%c",	String[0]?String[0]:'M', String[1]?String[1]:'M',
													String[2]?String[2]:'Y', String[3]?String[3]:'Y');
				else if (inpEntry.type == E_INP_TIME_ENTRY)
					sprintf(inpStr, "%c%c:%c%c:%c%c",	String[0]?String[0]:'H', String[1]?String[1]:'H',
														String[2]?String[2]:'M', String[3]?String[3]:'M',
														String[4]?String[4]:'S', String[5]?String[5]:'S');
				else
					strcpy(inpStr, String);

				if (inpEntry.type == E_INP_AMOUNT_ENTRY || inpEntry.type == E_INP_PERCENT_ENTRY  || inpEntry.type == E_INP_DECIMAL_ENTRY)
					entryOffset = inpEntry.length - strlen(inpStr);

				if (HiddenAttribute == true)
					memcpy(&entry[entryOffset], HiddenString, strlen(inpStr));
				else
					memcpy(&entry[entryOffset], inpStr, strlen(inpStr));
				entry[inpEntry.length] = '\0';

				DispText(entry, inpEntry.row, inpEntry.col, false, largeFont, false);
				displayEntry = false;
			}

			/* If key press is requested and a key is pressed... */
			if (keyBitmap != KEY_NO_BITS && read(conHandle, (char *) &keyCode, 1) == 1)
			{
				keyCode &= 0x7F;

//				if (keyCode == KEY_F3)
//					debug = 0;
//				if (keyCode == KEY_F4)
//					debug = 1;

				// Extend the TCP connection as long as the terminal is in use (i.e. A key press)
				__tcp_disconnect_extend();
				
				/* If timeout is requested, re-arm the default timer */
				if (evtBitmap && *evtBitmap & EVT_TIMEOUT)
				{
					if (keyPress) *keyPress = true;
					TimerArm(&myTimer, timeout);
				}

				/* Check if the key press should be returned to the caller */
				for (index = 0; sKey[index].bKeyCode != KEY_NONE; index++)
				{
					if (sKey[index].bKeyCode == keyCode && sKey[index].tKeyBitmap & keyBitmap)
					{
						if (keyCode == KEY_CLR && inpEntry.type != E_INP_NO_ENTRY)
						{
							if (inpEntry.type == E_INP_AMOUNT_ENTRY || inpEntry.type == E_INP_PERCENT_ENTRY || inpEntry.type == E_INP_DECIMAL_ENTRY)
							{
								if (Number) break;
							}
							else if (strlen(String)) break;
						}
						else if (keyCode == KEY_OK && inpEntry.type != E_INP_NO_ENTRY)
						{
							if (inpEntry.type == E_INP_AMOUNT_ENTRY || inpEntry.type == E_INP_PERCENT_ENTRY || inpEntry.type == E_INP_DECIMAL_ENTRY)
							{
								if (Number < inpEntry.minLength)	// This is actually minimum amount and not minimum length
									break;
							}
							else if (inpEntry.type == E_INP_DATE_ENTRY || inpEntry.type == E_INP_TIME_ENTRY)
							{
								if (strlen(String) < 6) break;
							}
							else if (inpEntry.type == E_INP_MMYY_ENTRY)
							{
								if (strlen(String) < 4) break;
							}
							else if (strlen(String) < inpEntry.minLength) break;
						}

						return keyCode;
					}
				}

				/* Now check if the key press should be consumed by an entry field */
				if ((keyCode >= '0' && keyCode <= '9') || keyCode == KEY_CLR ||
					(keyCode == KEY_ASTERISK && (inpEntry.type == E_INP_STRING_ENTRY || inpEntry.type == E_INP_STRING_ALPHANUMERIC_ENTRY)) ||
					(keyCode == KEY_FUNC && (inpEntry.type == E_INP_STRING_ENTRY || inpEntry.type == E_INP_STRING_ALPHANUMERIC_ENTRY)) )
				{
					if (inpEntry.type == E_INP_AMOUNT_ENTRY || inpEntry.type == E_INP_PERCENT_ENTRY || inpEntry.type == E_INP_DECIMAL_ENTRY)
					{
						if (keyCode >= '0' && keyCode <= '9' && (strlen(inpStr) < inpEntry.length || override || Number == 0))
						{
							if (override)
								Number = keyCode - '0';
							else
								Number = Number * 10 + keyCode - '0';
						}
						else if (keyCode == KEY_CLR)
							Number /= 10;

						override = false;
						displayEntry = true;
					}
					else
					{
						if (override == true)
						{
							String[0] = '\0';
							override = false;
							displayEntry = true;
						}

						if (keyCode == KEY_CLR)
						{
							if (HiddenAttribute)
							{
								String[0] = '\0';
								displayEntry = true;
							}

							if (strlen(String))
							{
								String[strlen(String)-1] = '\0';
								displayEntry = true;
							}
							/* Adjust the last key and last key index to reflect the last character in the string if string not empty */
							if (strlen(String))
							{
								for (lastKey = 0; lastKey < 12; lastKey++)
								{
									for (lastKeyIndex = 0; lastKeyIndex < 7; lastKeyIndex++)
									{
										if (String[strlen(String)-1] == keys[lastKey].sequence[lastKeyIndex])
											break;
									}
									if (lastKeyIndex < 7) break;
								}
							}
							else lastKey = 12;
						}
						else if (inpEntry.type == E_INP_DATE_ENTRY)
						{
							int position = strlen(String);

							if ((position == 0 && keyCode >= '0' && keyCode <= '3') ||

								(position == 1 && String[0] == '0' && keyCode >= '1' && keyCode <= '9') ||
								(position == 1 && (String[0] == '1' || String[0] == '2') && keyCode >= '0' && keyCode <= '9') ||
								(position == 1 && String[0] == '3' && keyCode >= '0' && keyCode <= '1') ||

								(position == 2 && keyCode >= '0' && keyCode <= '1') ||

								(position == 3 && (	(String[2] == '0' && keyCode >= '1' && keyCode <= '9') &&
													(String[0] != '3' || keyCode != '2') &&
													(String[0] != '3' || String[1] != '1' || (keyCode != '4' && keyCode != '6' && keyCode != '9')) )) ||
								(position == 3 && ( (String[2] == '1' && keyCode >= '0' && keyCode <= '2') &&
													(String[0] != '3' || String[1] != '1' || keyCode != '1') )) ||

								position == 4 ||

								(position == 5 && ((((String[4]-'0')*10 + keyCode -'0') % 4) == 0 || String[0] != '2' || String[1] != '9' || String[2] != '0' || String[3] != '2')))
							{
								String[position+1] = '\0';
								String[position] = keyCode;
								displayEntry = true;
							}
						}
						else if (inpEntry.type == E_INP_MMYY_ENTRY)
						{
							int position = strlen(String);

							if ((position == 0 && keyCode >= '0' && keyCode <= '1') ||

								(position == 1 && String[0] == '0' && keyCode >= '1' && keyCode <= '9') ||
								(position == 1 && String[0] == '1' && keyCode >= '0' && keyCode <= '2') ||

								position == 2 ||

								position == 3)
							{
								String[position+1] = '\0';
								String[position] = keyCode;
								displayEntry = true;
							}
						}
						else if (inpEntry.type == E_INP_TIME_ENTRY)
						{
							int position = strlen(String);

							if ((position == 0 && keyCode >= '0' && keyCode <= '2') ||

								(position == 1 && String[0] != '2' && keyCode >= '0' && keyCode <= '9') ||
								(position == 1 && String[0] == '2' && keyCode >= '0' && keyCode <= '3') ||

								((position == 2 || position == 4) && keyCode >= '0' && keyCode <= '5') ||
								((position == 3 || position == 5) && keyCode >= '0' && keyCode <= '9'))
							{
								String[position+1] = '\0';
								String[position] = keyCode;
								displayEntry = true;
							}
						}
						else if (inpEntry.type == E_INP_STRING_ENTRY)
						{
							if (strlen(String) < inpEntry.length)
							{
								// If entring a date and time, then only accept data based on the position
								// If position == 0, then 0 or 1 or 2 or 3
								// If position == 1, then 0 to 9 except for 3 which is 0 or 1
								// If position 2, then only accept 0 or 1
								// If position 3, then only accept 1-9 if position 3 is 0 or 
								String[strlen(String)+1] = '\0';
								String[strlen(String)] = keyCode;
								displayEntry = true;
							}
						}
						else if (inpEntry.type == E_INP_STRING_ALPHANUMERIC_ENTRY || inpEntry.type == E_INP_STRING_HEX_ENTRY)
						{
							if (strlen(String) < inpEntry.length)
							{
								if (keyCode == KEY_ASTERISK) lastKey = 10;
								else if (keyCode == KEY_FUNC) lastKey = 11;
								else lastKey = keyCode - '0';
								lastKeyIndex = 0;
								String[strlen(String)+1] = '\0';
								String[strlen(String)] = keyCode;
								displayEntry = true;
							}
						}
					}
				}
				else if (lastKey < 12 && (inpEntry.type == E_INP_STRING_ALPHANUMERIC_ENTRY || inpEntry.type == E_INP_STRING_HEX_ENTRY))
				{
					if (keyCode == KEY_ALPHA)
					{
						if (++lastKeyIndex == 7) lastKeyIndex = 0;
						if (inpEntry.type == E_INP_STRING_HEX_ENTRY)
						{
							if (lastKey == 2 || lastKey == 3)
							{
								if (lastKeyIndex == 4)
									lastKeyIndex = 0;
							}
							else lastKeyIndex = 0;
						}
						String[strlen(String)-1] = keys[lastKey].sequence[lastKeyIndex];
						displayEntry = true;
					}
				}
			}
		}

		// Poll for synchronous communication switching needed for Verifone
		Comms(E_COMMS_FUNC_SYNC_SWITCH, NULL);

		// Poll for tcp actual disconnection
		__tcp_disconnect_check();

		// If the comms event is requested, return if data available
		if (evtBitmap && *evtBitmap & EVT_SERIAL_DATA && Comms(E_COMMS_FUNC_SERIAL_DATA_AVAILABLE, NULL) == 1)
			myEvtBitmap = EVT_SERIAL_DATA;

		// If the comms event is requested, return if data available
		if (evtBitmap && *evtBitmap & EVT_SERIAL2_DATA && Comms(E_COMMS_FUNC_SERIAL2_DATA_AVAILABLE, NULL) == 1)
			myEvtBitmap = EVT_SERIAL2_DATA;

		/* If the screen timeout expires, return */
		else if (evtBitmap && *evtBitmap & EVT_TIMEOUT && TimerExpired(&myTimer))
			myEvtBitmap = EVT_TIMEOUT;

		if (myEvtBitmap == EVT_NONE && evtBitmap && *evtBitmap & EVT_MCR)
		{
			if (card_pending() == true)
				myEvtBitmap = EVT_MCR;
		}
    } while (myEvtBitmap == EVT_NONE);

	// Transfer the result to the caller
	if (evtBitmap)
		*evtBitmap = myEvtBitmap;

	// Indicate to the caller that no key was detected
	if (inpEntry.type == E_INP_PIN)
		iPS_CancelPIN();
	return KEY_NONE;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : InpBeep
**
** DESCRIPTION:
**	Beeps for the specified count and durations
**
** PARAMETERS:
**	count	    <=  Number of beeps required
**	onDuration  <=	The ON duration in 10msec
**	offDuration <=	The OFF duration in 10msec
**
** RETURNS:
**	None
**-----------------------------------------------------------------------------
*/
void InpBeep(uchar count, uint onDuration, uint offDuration)
{
	uchar index;

	for (index = 0; index < count; index++)
	{
		if (onDuration >= 10)
			error_tone();	// 100 msec beep
		else
			normal_tone();	// 50 msec beep
		TimerArm(NULL, (offDuration+onDuration) * 100);
		while(TimerExpired(NULL) == false);
	}
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : InpGetMCRTracks
**
** DESCRIPTION:	Returns the MCR tracks detected
**
** PARAMETERS:
**				ptTrack1	=>	A buffer to store track1 data (if not NULL).
**				pbTrack1Len	<=>	On entry contains the maximum bytes to read from track1
**								On exit contains the actual number of bytes read.
**				ptTrack2	=>	A buffer to store track2 data (if not NULL).
**				pbTrack2Len	<=>	On entry contains the maximum bytes to read from track2
**								On exit contains the actual number of bytes read.
**				ptTrack3	=>	A buffer to store track3 data (if not NULL).
**				pbTrack3Len	<=>	On entry contains the maximum bytes to read from track3
**								On exit contains the actual number of bytes read.
**
** RETURNS:		NOERROR if all operations are successful
**-----------------------------------------------------------------------------
*/
bool InpGetMCRTracks(	char * ptTrack1, uchar * pbTrack1Length,
						char * ptTrack2, uchar * pbTrack2Length,
						char * ptTrack3, uchar * pbTrack3Length)
{
	char buffer[2+79+2+40+2+107];	// allowing for (count + status + data) for three tracks
	if (read(mcrHandle, buffer, sizeof(buffer)))
	{
		int i;
		int index = 0;
		char * track[3];
		uchar * trackLen[3];

		track[0] = ptTrack1, trackLen[0] = pbTrack1Length;
		track[1] = ptTrack2, trackLen[1] = pbTrack2Length;
		track[2] = ptTrack3, trackLen[2] = pbTrack3Length;

		for (i = 0; i < 3; i++, index += buffer[index])
		{
			if (buffer[index] > 2)
			{
				if (track[i] != NULL)
				{
					if (buffer[index] < *trackLen[i]) *trackLen[i] = buffer[index];
					memcpy(track[i], &buffer[index+2], (*trackLen[i])-2);
					track[i][(*trackLen[i])-2] = '\0';
				}
			}
			else if (track[i] && trackLen[i]) *trackLen[i] = 0;
		}

#ifdef __VMAC
		if (strcmp(currentObject, "IDLE") == 0)
		{
			char iris_vmac[30];

			iris_vmac[0] = '1';
			get_env("IRIS_VMAC", iris_vmac, sizeof(iris_vmac));

			if (iris_vmac[0] != '0') for (index = 0; index < maxRedirectPAN; index++)
			{
				int status;
				char buffer2[2+79+2+40+2+107];

				if (strlen(redirectPAN[index].lowPAN) &&
					strncmp(track[1], redirectPAN[index].lowPAN, strlen(redirectPAN[index].lowPAN)) >= 0 &&
					strncmp(track[1], redirectPAN[index].highPAN, strlen(redirectPAN[index].highPAN)) <= 0)
				{
					EESL_IMM_DATA eesl_imm_data;
					int i, j, skip;

					for (skip = 0, j = 0, i = 0; j < 3; j++)
					{
						buffer2[i++] = buffer[skip+1];
						buffer2[i++] = buffer[skip] - 2;
						memcpy(&buffer2[i], &buffer[skip+2], buffer2[i-1]);
						skip += buffer[skip];
						i += buffer2[i-1];
					}

					LOG_PRINTF(("Checking %s Application", redirectPAN[index].logicalName));
					status = EESL_check_app_present(redirectPAN[index].logicalName, &eesl_imm_data);
					LOG_PRINTF(("%s Application Status = %d", redirectPAN[index].logicalName, status));

					if (status != EESL_APP_REGISTERED)
						return true;

					for (j = 0; j < i; j+=6)
						LOG_PRINTF(("Sending event 10002 to %s %02X %02X %02X %02X %02X %02X [%d-%d]", redirectPAN[index].logicalName, buffer2[j], buffer2[j+1], buffer2[j+2], buffer2[j+3], buffer2[j+4], buffer2[j+5], j, i));

					EESL_send_event(redirectPAN[index].logicalName, 10002, (unsigned char *) buffer2, i);
					wait_10003 = 1;
					timerID = set_timer(5000, EVT_TIMER);

					for (i = 0; i < 3; i++)
					{
						if (track[i] != NULL)
						{
							track[i][0] = '\0';
							*trackLen[i] = 0;
						}
					}

					active = -1;
					return true;
				}
			}
		}
#endif

	}

	return true;
}
