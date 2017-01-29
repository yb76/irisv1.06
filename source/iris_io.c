//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iris_io.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports I/O functions including
//					- Keyboard input results
//					- Printing
//					- Beeper
//					- Magnetic card reads.
//-----------------------------------------------------------------------------
//

//
//-----------------------------------------------------------------------------
// Include Files
//-----------------------------------------------------------------------------
//

//
// Standard include files.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Project include files.
//
#include <auris.h>

//
// Local include files
//
#include "alloc.h"
#include "input.h"
#include "printer.h"
#include "utility.h"
#include "iris.h"
#include "irisfunc.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//
static const char banner[] = "**** GRAPHICS START ****";
static const char banner2[] = "**** GRAPHICS END   ****";

static const struct
{
	char * search;
	char * replace;
} special[5] = {	{"**** COMMA ****", ","}, {"**** WIGGLY ****", "{"}, {"**** YLGGIW ****", "}"}, {"**** BRACKET ****", "["}, {"**** TEKCARB ****", "]"}	};


//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//
static uchar track1Len;
static uchar track2Len;
static char track1[90];
static char track2[50];


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// I/O FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()INPUT
//
// DESCRIPTION:	Returns the data input via the keyboard as a string
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __input(void)
{
	IRIS_StackPop(1);

	IRIS_StackPush(InpGetString());
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()KEY
//
// DESCRIPTION:	Returns the last key pressed returned from InpGetKeyEvent()
//
// PARAMETERS:	None
//
// RETURNS:		Last key pressed
//-------------------------------------------------------------------------------------------
//
void __key(void)
{
	IRIS_StackPop(1);

	IRIS_StackPush(getLastKeyDesc(lastKey, NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()ERRORBEEP
//
// DESCRIPTION:	Beeps in error
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __errorbeep(void)
{
	IRIS_StackPop(1);

	InpBeep(2, 20, 5);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TRACK2
//
// DESCRIPTION:	Retrieves track 2 data
//
// PARAMETERS:	None
//
// RETURNS:		Track 2 string if available or an empty string
//-------------------------------------------------------------------------------------------
//
void __track2(void)
{
	IRIS_StackPop(1);

	// Read track 2 if not available
	if (track2[0] == '\0' && track2Len == 40)
		InpGetMCRTracks(track1, &track1Len, track2, &track2Len, NULL, NULL);
	IRIS_StackPush(track2Len?track2:NULL);

	// Clear track2
	memset(track2, 0, sizeof(track2));
	track2Len = 40;
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TRACK1
//
// DESCRIPTION:	Retrieves track 1 data
//
// PARAMETERS:	None
//
// RETURNS:		Track 1 string if available or an empty string
//-------------------------------------------------------------------------------------------
//
void __track1(void)
{
	IRIS_StackPop(1);

	// Read track 2 if not available
	if (track1[0] == '\0' && track1Len == 80)
		InpGetMCRTracks(track1, &track1Len, track2, &track2Len, NULL, NULL);
	IRIS_StackPush(track1Len?track1:NULL);

	// Clear track1
	memset(track1, 0, sizeof(track1));
	track1Len = 80;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TRACK_CLEAR
//
// DESCRIPTION:	Clears the track buffer.
//				This Prevents it from being accessed by other applications.
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __track_clear(void)
{
	IRIS_StackPop(1);

	memset(track1, 0, sizeof(track1));
	memset(track2, 0, sizeof(track2));
	track1Len = 80;
	track2Len = 40;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PRINT
//
// DESCRIPTION:	Prints formatted data
//
//
// PARAMETERS:	None
//
// RETURNS:		status of printing is updated but nothing returned
//-------------------------------------------------------------------------------------------
//
static void __add_print_data(char ** dump, char * data, int size)
{
	char * n;

	do
	{
		int i;
		char * banner;
		char * p;

		for (n = NULL, i = 0; i < 5; i++)
		{
			if ((p = strchr(data, special[i].replace[0])) != NULL && (n == NULL || p < n))
				n = p, banner = special[i].search;
		}

		if (n)
		{
			size += (strlen(banner) - 1);	// Comma banner string minus the comma itself
			*dump = my_realloc(*dump, size);

			*n = '\0';
			strcat(*dump, data);
			strcat(*dump, banner);
			data = n + 1;
		}
		else
			strcat(*dump, data);
	} while (n);
}

static char * ____printDump(char * dump, char * data, int graphics)
{
	int size;

	// Calculate the new size
	size = strlen(data) + (graphics?(strlen(banner)+strlen(banner2)):0) + (dump?strlen(dump):0) + 1;

	// Reallocate enough memory
	if (dump)
		dump = my_realloc(dump, size);
	else
		dump = my_calloc(size);

	// Add the header banner if graphics
	if (graphics)
		strcat(dump, banner);

	// Add the print data
	__add_print_data(&dump, data, size);

	// Add the trailer banner if graphics and lose what we have allocated to store the graphics
	if (graphics)
		strcat(dump, banner2);

	// Return the dump
	return dump;
}

void __print_cont(void)
{
	char * data = IRIS_StackGet(0);

	int i, index;
	bool skip = false;
	bool centre = false;
	char leftPart[200];
	char line[200];
	char output[200];
	int maxLine = 24;
	int largeFont = 24;
	bool doubleWidth = false;
	bool doubleHeight = false;
	bool inverse = false;
	int loopMarker = -1;
	char * dump = NULL;
	char * processFirst = NULL;
	uint cpl = 0;

	// Initialisation
	memset(line, 0, sizeof(line));
	memset(leftPart, 0, sizeof(leftPart));

	for (i = 0; data && data[i]; i++)
	{
		do
		{
			// Process any data from iRIS first
			if (processFirst)
			{
				if (data[++i] == '\0')
				{
					UtilStrDup(&processFirst, NULL);
					data = IRIS_StackGet(0);
					i = index;
					break;
				}
			}
				
			// Process the print data
			if (data[i] == '\\')
				skip = !skip;
			else if (skip && data[i] == 'C')
			{
				centre = true;
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'c')
			{
				cpl = (data[i+1] - '0') * 10 + data[i+2] - '0';
				i += 2;
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'R')
			{
				strcpy(leftPart, line);
				memset(line, 0 , sizeof(line));
				centre = skip = false;
				continue;
			}
			else if (skip && data[i] == 'n')
			{
				if (leftPart[0])
					sprintf(output, "%s%s%s%s%s%*s\n",	largeFont == 24? "\033k999":(largeFont == 32?"\033k032":"\033k042"),
														doubleWidth?"\033G":"", doubleHeight?"\033h":"", inverse?"\033i":"", leftPart, maxLine-strlen(leftPart), line);
				else if (centre)
					sprintf(output, "%s%s%s%s%*s\n",		largeFont == 24? "\033k999":(largeFont == 32?"\033k032":"\033k042"),
														doubleWidth?"\033G":"", doubleHeight?"\033h":"", inverse?"\033i":"", (maxLine+strlen(line))/2, line);
				else
					sprintf(output, "%s%s%s%s%s\n",		largeFont == 24? "\033k999":(largeFont == 32?"\033k032":"\033k042"),
														doubleWidth?"\033G":"", doubleHeight?"\033h":"",  inverse?"\033i":"",line);

				memset(line, 0, sizeof(line));
				memset(leftPart, 0, sizeof(leftPart));

				PrtPrintBuffer((uint) strlen(output), (uchar *) output, E_PRINT_APPEND);
				dump = ____printDump(dump, output, 0);

				centre = skip = false;
				cpl = 0;
				continue;
			}
			else if (skip && data[i] == 'f')
			{
				largeFont = maxLine = 24;
				doubleHeight = doubleWidth = inverse = false;
				skip = false;
				cpl = 0;
				continue;
			}
			else if (skip && data[i] == 'F')
			{
				largeFont = 32;
				doubleHeight = inverse = false;
				doubleWidth = true;
				maxLine = 16;
				skip = false;
				cpl = 0;
				continue;
			}
			else if (skip && data[i] == '3')
			{
				largeFont = maxLine = 32;
				doubleHeight = doubleWidth = inverse = false;
				skip = false;
				cpl = 0;
				continue;
			}
			else if (skip && data[i] == '4')
			{
				largeFont = maxLine = 42;
				doubleHeight = doubleWidth = inverse = false;
				skip = false;
				cpl = 0;
				continue;
			}
			else if (skip && data[i] == 'H')
			{
				doubleHeight = true;
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'h')
			{
				doubleHeight = false;
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'i')
			{
				inverse = true;
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'W')
			{
				if (doubleWidth == false)
				{
					doubleWidth = true;
					maxLine /= 2;
				}
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'w')
			{
				if (doubleWidth == true)
				{
					doubleWidth = false;
					maxLine *= 2;
				}
//				maxLine /= 2;		// ****** This needs to be deleted.....wrong....TH.
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'B')
			{
				char barcode[20];
				sprintf(barcode, "\033k901%.12s", &data[i+1]);
				PrtPrintBuffer((uint) strlen(barcode), (uchar *) barcode, E_PRINT_APPEND);
				dump = ____printDump(dump, barcode, 0);
				i += 12;
				skip = false;
				continue;
			}
			else if (skip && data[i] == '*')
			{
				int size = 0;
				char barcode[120];
				char * end = strchr(&data[i+1], '\\');
				if (end) size = end - &data[i+1];
				if (size > 0 && size < 100)
				{
					sprintf(barcode, "\033k902%.*s\033k903", size, &data[i+1]);
					PrtPrintBuffer((uint) strlen(barcode), (uchar *) barcode, E_PRINT_APPEND);
					dump = ____printDump(dump, barcode, 0);
					i += size;
				}
				skip = false;
				continue;
			}
			else if (skip && data[i] == 'g')
			{
				int j;
				char name[50];
				unsigned int length;
				char * objectData;

				for (i++, j = 0; data[i] != '\\'; i++, j++)
					name[j] = data[i];
				name[j] = '\0', i--;

				// Obtain the graphics object locally or go to the host if not available
				if ((objectData = IRIS_GetObjectData(name, &length)) == NULL)
				{
					IRIS_DownloadResourceObject(name);
					objectData = IRIS_GetObjectData(name, &length);
				}

				if (objectData)
				{
					char * image = IRIS_GetStringValue(objectData, length, "IMAGE", false);

					if (image)
					{
						if (image[0] == 0)
						{
							int width, height;
							unsigned char * imageb = my_malloc(strlen(&image[4]) / 2 + 1);

							UtilStringToHex(&image[4], strlen(&image[4]), imageb);

							PrtPrintBuffer(1, "\n", E_PRINT_END);

							width = imageb[0] * 256 + imageb[1];
							height = imageb[2] * 256 + imageb[3];
							PrtPrintGraphics(width, height, &imageb[4], true, (uchar) ((height >= 120 || width > 192)? 1:2));
							dump = ____printDump(dump, (char *) &image[4], 1);

							my_free(imageb);
						}

						IRIS_DeallocateStringValue(image);
					}

					my_free(objectData);
				}

				skip = false;
				continue;
			}

			// Loop start, set the index variables and remember them for counting later
			else if (skip && data[i] == 'L')
			{
				// Remember the beginning of the loop block
				loopMarker = i;
				skip = false;
				continue;
			}
			else if (processFirst == NULL && data[i] == '?' && (data[i+1] == '/' || data[i+1] == '{' || data[i+1] == '~' || data[i+1] == '(' || data[i+1] == '['))
			{
				int j = i+1;
				int myStackIndex = stackIndex;

				// Prepare an evaluation expression by bringing back the commas
				while (data[++i] != '?')
				{
					if (data[i] == ';') data[i] = ',';
					else if (data[i] == '{') data[i] = '[';
					else if (data[i] == '}') data[i] = ']';
				}

				data[i] = '\0';
				IRIS_Eval(&data[j], false);
				data[i] = '?';

				if (myStackIndex != stackIndex)
				{
					// If we are looping, then go ahead
					if (IRIS_StackGet(0))
					{
						if (strcmp(IRIS_StackGet(0), "LOOP") == 0 && loopMarker != -1)
							i = loopMarker;
						else
						{
							UtilStrDup(&processFirst, IRIS_StackGet(0));
							data = processFirst;
							index = i;
							i = -1;
						}
					}
					IRIS_StackPop(stackIndex-myStackIndex);	// This is just to handle extra items pushed on the stack for no reason. i.e. display object programming errors
				}

				continue;
			}
			else
				skip = false;

			// Add the printing character to the current line
			if (skip == false)
			{
				if (data[i] != '\r' && data[i] != '\n')
					line[strlen(line)] = data[i];
				if (data[i] == '\n' || (cpl && (strlen(leftPart) + strlen(line)) >= cpl))
				{
					if (leftPart[0])
						sprintf(output, "%s%s%s%s%s%*s\n",	largeFont == 24? "\033k999":(largeFont == 32?"\033k032":"\033k042"),
															doubleWidth?"\033G":"", doubleHeight?"\033h":"", inverse?"\033i":"", leftPart, maxLine-strlen(leftPart), line);
					else if (centre)
						sprintf(output, "%s%s%s%s%*s\n",		largeFont == 24? "\033k999":(largeFont == 32?"\033k032":"\033k042"),
															doubleWidth?"\033G":"", doubleHeight?"\033h":"", inverse?"\033i":"", (maxLine+strlen(line))/2, line);
					else
						sprintf(output, "%s%s%s%s%s\n",		largeFont == 24? "\033k999":(largeFont == 32?"\033k032":"\033k042"),
															doubleWidth?"\033G":"", doubleHeight?"\033h":"",  inverse?"\033i":"",line);

					memset(line, 0, sizeof(line));
					memset(leftPart, 0, sizeof(leftPart));

					PrtPrintBuffer((uint) strlen(output), (uchar *) output, E_PRINT_APPEND);
					dump = ____printDump(dump, output, 0);

					centre = false;
				}	
			}
		} while (processFirst);
	}

	IRIS_StackPop(2);

	IRIS_StackPush(dump);
	UtilStrDup(&dump, NULL);
}

void __println(void)
{
	char * data = IRIS_StackGet(0);

	if (data)
	{
		PrtPrintBuffer((uint) strlen(data), (uchar *) data, E_PRINT_APPEND);
		PrtPrintBuffer(1, "\n", E_PRINT_END);
	}

	IRIS_StackPop(2);
	IRIS_StackPush(NULL);
}

void __print(void)
{
	__print_cont();

	PrtPrintFormFeed();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PRINT_RAW
//
// DESCRIPTION:	Prints raw data to the printer. Used for duplicate printing for example
//
//
// PARAMETERS:	None
//
// RETURNS:		status of printing is updated but nothing returned
//-------------------------------------------------------------------------------------------
//
static void print_raw(char * data)
{
	char * ptr = data;
	char * prevPtr = data;
	char * graphicsPtr;
	bool ok = true;

	do
	{
		// Look for the graphics banner
		if ((graphicsPtr = strstr(prevPtr, banner)) == NULL)
			graphicsPtr = &prevPtr[strlen(prevPtr)];

		// Look for a new line
		while ((ptr = strstr(ptr, "\n")) != NULL)
		{
			if (ptr < graphicsPtr)
			{
				char * n;

				do
				{
					int i;
					char * banner;
					char * replace;
					char * p;

					for (n = NULL, i = 0; i < 5; i++)
					{
						if ((p = strstr(prevPtr, special[i].search)) != NULL && (n == NULL || p < n) && p < ptr)
							n = p, banner = special[i].search, replace = special[i].replace;
					}

					if (n)
					{
						PrtPrintBuffer(n-prevPtr, (uchar *) prevPtr, E_PRINT_APPEND);
						PrtPrintBuffer(1, (uchar *) replace, E_PRINT_APPEND);
						prevPtr = n + strlen(banner);
					}
				} while(n);

				PrtPrintBuffer(ptr-prevPtr, (uchar *) prevPtr, E_PRINT_APPEND);
				prevPtr = ptr;
				ptr++;
			}
			else
			{
				// Print the graphics image
				if (graphicsPtr[0] == banner[0])
				{
					graphicsPtr += strlen(banner);
					if ((ptr = strstr(graphicsPtr, banner2)) != NULL)
					{
						char temp = *ptr;
						int width, height;
						uchar * buf = my_malloc((ptr-graphicsPtr)/2);

						*ptr = '\0';
						UtilStringToHex(graphicsPtr, strlen(graphicsPtr), buf);
						*ptr = temp;

						PrtPrintBuffer(1, "\n", E_PRINT_END);

						width = buf[0] * 256 + buf[1];
						height = buf[2] * 256 + buf[3];
						PrtPrintGraphics(width, height, &buf[4], true, (uchar) ((height >= 120 || width > 192)? 1:2));

						my_free(buf);

						ptr += strlen(banner2);
						prevPtr = ptr;
					}
				}
				else
					ok = false;

				// Look for another graphics image
				break;
			}
		}
	} while(ok && ptr);
}

void __print_raw(void)
{
	char * data = IRIS_StackGet(0);

	if (data)
	{
		print_raw(data);

		PrtPrintFormFeed();
	}

	IRIS_StackPop(2);

	IRIS_StackPush(data?"TRUE":NULL);
}

void __print_raw_cont(void)
{
	char * data = IRIS_StackGet(0);

	if (data)
		print_raw(data);

	IRIS_StackPop(2);

	IRIS_StackPush(data?"TRUE":NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PRINT_ERR
//
// DESCRIPTION:	Checks the printer and returns the last error
//
// PARAMETERS:	None
//
// RETURNS:		status of printing is updated but nothing returned
//-------------------------------------------------------------------------------------------
//
void __print_err(void)
{
	// Lose the function name
	IRIS_StackPop(1);

	IRIS_StackPush(PrtGetErrorText(PrtStatus(true)));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()REBOOT
//
// DESCRIPTION:	Re-starts the terminal
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __reboot(void)
{
	// Lose the function name
	IRIS_StackPop(1);
#ifdef _DEBUG
	exit(0);
#else
	SVC_RESTART("");
#endif
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SHUTDOWN
//
// DESCRIPTION:	Turns of the terminal. This only works with a Vx670 terminal that is not externally powered.
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __shutdown(void)
{
	if (SVC_SHUTDOWN() == 0)
		SVC_WAIT(10000);	// Make sure we do not start any operation while shuting down.
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()BACKLIGHT
//
// DESCRIPTION:	Turns the backlight ON or OFF.
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __backlight(void)
{
	char * onoff = IRIS_StackGet(0);

	if (onoff)
	{
		if (strcmp(onoff, "ON") == 0)
			set_backlight(1);
		else
			set_backlight(0);
	}

	IRIS_StackPop(2);
	IRIS_StackPush(NULL);
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : ()LOW_POWER
**
** DESCRIPTION: Turns the screen saver ON and puts the device in low power mode
**
** PARAMETERS:	None
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
void __lowPower(void)
{
//	InpTurnOff(true);
//	read_event();	// Clear any pending events...

//	SVC_SLEEP();	// Tell the system we are ready to sleep
//	set_backlight(0);
	wait_event();	// Wait for a wake up event
//	set_backlight(1);

//	InpTurnOn();
}
