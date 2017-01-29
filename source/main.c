#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <auris.h>
#include <svc.h>
#include <svc_sec.h>

#include "display.h"
#include "input.h"
#include "printer.h"
#include "comms.h"

#include "as2805.h"
#include "security.h"
#include "utility.h"
#include "iris.h"
#include "irisfunc.h"

int conHandle = -1;

T_COMMS comms;

// We must define these as they are expected by RIS to define the scope of object and string resolution
char * currentObject = NULL;
char * currentObjectData = NULL;
uint currentObjectLength;
char * currentObjectVersion = NULL;
char * currentObjectType = NULL;
char * currentObjectGroup = NULL;
char * currentObjectEvent = NULL;
char * currentObjectEventValue = NULL;

char * prevObject = NULL;
char * nextObject = NULL;

#define	MAX_DATA	40
struct
{
	char name[50];
	char value[100];
} data[MAX_DATA];

struct
{
	T_KEYBITMAP keyBitmap;
	T_EVTBITMAP evtBitmap;
	char * action;
	char * object;
} map[30];
int mapIndex = 0;

uchar track1Len;
uchar track2Len;
char track1[90];
char track2[50];

static const struct
{
	T_EVTBITMAP evtBitmap;
	char * event;
} event_string[] = 
{
	{EVT_TIMEOUT,		"TIME"},
	{EVT_MCR,			"MCR"},
	{EVT_SERIAL_DATA,	"SER_DATA"},
	{EVT_NONE,			NULL}
};

static const struct
{
	uchar key;
	T_KEYBITMAP keyBitmap;
	char * event;
} key_string[] =
{
	{KEY_NUM,	KEY_NUM_BITS,	"KEY_NUM"},
	{KEY_0,		KEY_0_BIT,		"KEY_0"},
	{KEY_1,		KEY_1_BIT,		"KEY_1"},
	{KEY_2,		KEY_2_BIT,		"KEY_2"},
	{KEY_3,		KEY_3_BIT,		"KEY_3"},
	{KEY_4,		KEY_4_BIT,		"KEY_4"},
	{KEY_5,		KEY_5_BIT,		"KEY_5"},
	{KEY_6,		KEY_6_BIT,		"KEY_6"},
	{KEY_7,		KEY_7_BIT,		"KEY_7"},
	{KEY_8,		KEY_8_BIT,		"KEY_8"},
	{KEY_9,		KEY_9_BIT,		"KEY_9"},
	{KEY_SK1,	KEY_SK1_BIT,	"KEY_SK1"},
	{KEY_SK2,	KEY_SK2_BIT,	"KEY_SK2"},
	{KEY_SK3,	KEY_SK3_BIT,	"KEY_SK3"},
	{KEY_SK4,	KEY_SK4_BIT,	"KEY_SK4"},
	{KEY_CNCL,	KEY_CNCL_BIT,	"CANCEL"},
	{KEY_CLR,	KEY_CLR_BIT,	"KEY_CLR"},
	{KEY_OK,	KEY_OK_BIT,		"KEY_OK"},
	{KEY_UP,	KEY_UP_BIT,		"KEY_UP"},
	{KEY_NONE,	0,				NULL}
};

ulong displayTimeout;
int displayTimeoutMultiplier;
int displayTimeoutMultiplierFull;


static bool processEvent(T_EVTBITMAP setEvtBitmap, T_KEYBITMAP keepKeyBitmap, T_INP_ENTRY inpEntry, ulong timeout, bool ignoreTimeout, bool largeFont,
						 int timeoutFull, int * displayTimeout)
{
	int i;
	T_KEYBITMAP keyBitmap = KEY_NO_BITS;
	T_EVTBITMAP evtBitmap = EVT_NONE;
	uchar key;
	bool eventOccurred = false;
	char * event;

	// Wait for an event to occur
	evtBitmap = setEvtBitmap;
	key = InpGetKeyEvent(keepKeyBitmap, &evtBitmap, inpEntry, timeout * 10, largeFont);
	if (ignoreTimeout && key == KEY_NONE && evtBitmap == EVT_TIMEOUT)
		return false;

	// Get the non-key event description
	if (key == KEY_NONE)
	{
		for (i = 0; key_string[i].event; i++)
		{
			if (evtBitmap == event_string[i].evtBitmap)
			{
				event = event_string[i].event;
				break;
			}
		}
	}

	// Get the key description
	else
	{
		// Convert the key pressed to a bitmap
		for (i = 0; key_string[i].event; i++)
		{
			if (key == key_string[i].key)
			{
				keyBitmap = key_string[i].keyBitmap;
				event = key_string[i].event;
				break;
			}
		}
	}

	// Check if an event has occurred
	for (i = 0; i < mapIndex; i++)
	{
		if ((keyBitmap & map[i].keyBitmap) || (key == KEY_NONE && evtBitmap == map[i].evtBitmap))
		{
			// Process the action
			IRIS_ProcessActionObject(map[i].action);

			// Determine the next object
			IRIS_ResolveToSingleValue(map[i].object, false);
			if (IRIS_StackGet(0))
			{
				strcpy(nextObject, IRIS_StackGet(0));
				currentObjectEvent = (key == KEY_NONE?"EVENT":"KEY");
				strcpy(currentObjectEventValue, event);
				IRIS_StackPop(1);

				eventOccurred = true;
			}
			break;
		}
	}

	return eventOccurred;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : processPath
//
// DESCRIPTION:	Process the path string
//
// PARAMETERS:	objectName	<=	The name of the object to retrieve
//
// RETURNS:		Track 1 string if available or an empty string
//-------------------------------------------------------------------------------------------
//
static void processPath(char * events, T_KEYBITMAP * keyBitmap, T_EVTBITMAP * keepEvtBitmap)
{
	// Initialisation
	mapIndex = 0;

	// Look for events to process
	if (events && events[0] == 1)
	{
		char ** array1 = (char **) &events[4];
		for(; *array1; array1++)
		{
			if ((*array1)[0] == 1)		// An array
			{
				int index;
				int operation = 0;		// Key operation
				char ** array2 = (char **) &((*array1)[4]);

				for(index = 0; *array2; array2++, index++)
				{
					char * value = &(*array2)[4];

					// Check that we have an element for first (KEY or EVENT) and third (Action Object) elements
					if ((index == 0 || index == 2) && (*array2)[0] != 0)
						break;

					switch(index)
					{
						case 0:
							if (value[0] == '\0' || strcmp(value, "EVENT") == 0)
								operation = 1;
							break;
						case 1:
							// For events....
							if (operation == 1)
							{
								// Not interested in keys....
								map[mapIndex].keyBitmap = KEY_NO_BITS;

								if ((*array2)[0] == 0)
								{
									if (value[0] == '\0')	// If empty, then timeout quickly...No waiting.
									{
										*keepEvtBitmap |= EVT_TIMEOUT, map[mapIndex].evtBitmap = EVT_TIMEOUT;
										displayTimeoutMultiplier = displayTimeoutMultiplierFull = 1;
										displayTimeout = 0;
									}
									else
									{
										if (strcmp(value, "MCR") == 0)
											*keepEvtBitmap |= EVT_MCR, map[mapIndex].evtBitmap = EVT_MCR;
										else if (strcmp(value, "SER_DATA") == 0)
											*keepEvtBitmap |= EVT_SERIAL_DATA, map[mapIndex].evtBitmap = EVT_SERIAL_DATA;
									}
								}
								else
								{
									int index;
									char ** array3 = (char **) &((*array2)[4]);

									for (index = 0; *array3; array3++, index++)
									{
										char * value = &(*array3)[4];

										if ((*array3)[0] != 0)
											break;

										if (index == 0)
										{
											if (strcmp(value, "TIME") == 0)
												*keepEvtBitmap |= EVT_TIMEOUT, map[mapIndex].evtBitmap = EVT_TIMEOUT;
										}
										else if (index == 1)
										{
											displayTimeoutMultiplier = displayTimeoutMultiplierFull = atol(value) / 100;
											displayTimeout = 100;
										}
									}
								}
							}

							// For keys
							else
							{
								int myStackIndex = stackIndex;

								// Not interested in events
								map[mapIndex].evtBitmap = EVT_NONE;

								// Extract single value onto the stack or multiple values onto the stack
								if ((*array2)[0] == 0)
									IRIS_ResolveToSingleValue(*array2, false);
								else
								{
									char ** array3 = (char **) &((*array2)[4]);

									for (; *array3; array3++)
										IRIS_ResolveToSingleValue(*array3, false);
								}

								// Process all values on the stack regardless of order
								while(stackIndex != myStackIndex)
								{
									int i;
									value = IRIS_StackGet(0);		// The order does not mater in this case...We are starting from the last one.

									for (i = 0; key_string[i].event; i++)
									{
										if (strcmp(key_string[i].event, value) == 0)
										{
											*keyBitmap |= key_string[i].keyBitmap;
											map[mapIndex].keyBitmap |= key_string[i].keyBitmap;
											break;
										}
									}

									IRIS_StackPop(1);
								}
							}
							break;
						case 2:
							map[mapIndex].action = value;
							break;
						case 3:
							map[mapIndex++].object = *array2;
							break;
						default:
							break;
					}
				}
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : processDisplayObject2
//
// DESCRIPTION:	Rolls through the display object
//
// PARAMETERS:	objectName	<=	The name of the object to retrieve
//
// RETURNS:		Track 1 string if available or an empty string
//-------------------------------------------------------------------------------------------
//
static void processDisplayObject2(void)
{
	T_KEYBITMAP keyBitmap = KEY_NO_BITS;
	T_EVTBITMAP keepEvtBitmap = EVT_TIMEOUT;
	T_INP_ENTRY inpEntry, loopEntry;
	bool inpLargeFont = false;

	char * animation;
	char * events;
	char * clear;
	bool firstRound;
	bool eventOccurred;

	char * stringValue;

	// Initialisation
	memset(track1, 0, sizeof(track1));
	memset(track2, 0, sizeof(track2));
	track1Len = 80;
	track2Len = 40;
	inpEntry.type = E_INP_NO_ENTRY;
	loopEntry.type = E_INP_NO_ENTRY;
	DispClearScreen();
	displayTimeout = 0;
	displayTimeoutMultiplier = 0;
	displayTimeoutMultiplierFull = 0;

	// Update the current object type
	stringValue = IRIS_GetStringValue(currentObjectData, currentObjectLength, "TYPE", false);
	if (stringValue && stringValue[0] == 0)
		UtilStrDup(&currentObjectData, &stringValue[4]);
	IRIS_DeallocateStringValue(stringValue);

	// The object MUST be a display object. Otherwise, it is not displayable
	if (strcmp(currentObjectType, "DISPLAY"))
	{
		// Set the error line displays
		IRIS_StoreData("/__ERR_DISP/RETURN", prevObject, false);
		IRIS_StoreData("/__ERR_DISP/LINE2", currentObject, false);
		IRIS_StoreData("/__ERR_DISP/LINE3", "IS", false);
		IRIS_StoreData("/__ERR_DISP/LINE4", "NOT A DISPLAY OBJECT", false);

		// Remove the path to the object and link back to the previous display object
		// __ERR_DISP will go there when done.
		UtilStrDup(&currentObject, prevObject);

		// Set the new object to be the error display object
		UtilStrDup(&nextObject, "__ERR_DISP");
		return;
	}

	// Update the current object version
	stringValue = IRIS_GetStringValue(currentObjectData, currentObjectLength, "VERSION", false);
	if (stringValue && stringValue[0] == 0)
		UtilStrDup(&currentObjectVersion, &stringValue[4]);
	IRIS_DeallocateStringValue(stringValue);

	// Update the current object group
	stringValue = IRIS_GetStringValue(currentObjectData, currentObjectLength, "GROUP", false);
	if (stringValue && stringValue[0] == 0)
		UtilStrDup(&currentObjectGroup, &stringValue[4]);
	IRIS_DeallocateStringValue(stringValue);


	// Look for data to clear on startup
	clear = IRIS_GetStringValue(currentObjectData, currentObjectLength, "CLEAR", false);
	if (clear && clear[0] == 1)
	{
		char ** array1 = (char **) &clear[4];

		for (; *array1; array1++)
		{
			char fullName[100];
			char * simpleString = &(*array1)[4];

			// Check that we have an simpleString
			if ((*array1)[0] != 0)
				break;

			// Get simple string full name, then delete it
			IRIS_FullName(simpleString, fullName);
			IRIS_StoreData(fullName, NULL, true);
		}
	}
	IRIS_DeallocateStringValue(clear);

	// Create new objects if requested
	IRIS_StackPush(NULL);	// Dummy - function name
	IRIS_StackPush("NEW_OBJECT");
	IRIS_StackPush(currentObject);
	__new_object();

	// Look for events to process
	events = IRIS_GetStringValue(currentObjectData, currentObjectLength, "PATH", false);
	processPath(events, &keyBitmap, &keepEvtBitmap);

	// Get the value of the animation string from the display object
	animation = IRIS_GetStringValue(currentObjectData, currentObjectLength, "ANIMATION", false);

	// Keep looping waiting for events to occur
	for(firstRound = true, eventOccurred = false; eventOccurred == false; firstRound = false)
	{
		// We expect an array of arrays of strings, so examine this
		if (animation && animation[0] == 1)	// An array
		{
			char ** array1 = (char **) &animation[4];
			for(; eventOccurred == false && *array1; array1++)
			{
				if ((*array1)[0] == 1)		// An array
				{
					int index;
					int	operation = 0;
					int largeFont = false;
					char * search;
					char * referenceName = NULL;
					int row = 0, col = 0;
					bool permanent = true;
					ulong timeDelay = 0;
					int inputMaxLength = 0;
					char ** array2 = (char **) &((*array1)[4]);
					uchar * frequency;
					uchar displayLoopEnd = 0;
					bool displayYes = false;
					bool pinEntry = false;
					int myStackIndex = stackIndex;
					char * value;

					for(index = 0; *array2; array2++, index++)
					{

						switch(index)
						{
							// Pick up the type of the animation GRAPH or TEXT
							case 0:
								IRIS_ResolveToSingleValue(*array2, false);
								value = IRIS_StackGet(0);

								if (value)
								{
									if (strcmp(value, "GRAPH") == 0)
										operation = 1;
									else if (strcmp(value, "TEXT") == 0)
										operation = 0;
									else if (strcmp(value, "LARGE") == 0)
										largeFont = true;
									else if (strcmp(value, "BATTERY") == 0)
										operation = 2;
									else if (strcmp(value, "AMOUNT") == 0)
									{
										inpEntry.type = E_INP_AMOUNT_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "STRING") == 0)
									{
										inpEntry.type = E_INP_STRING_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "LAMOUNT") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_AMOUNT_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LSTRING") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_STRING_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "PIN") == 0)
									{
										inpLargeFont = largeFont = true;
										pinEntry = true;
									}
								}
								break;

							case 1:
								IRIS_ResolveToSingleValue(*array2, false);
								value = IRIS_StackGet(0);

								if (value)
								{
									// If this is an input entry, get the default/current value
									if (inputMaxLength)
									{
										if (inpEntry.type == E_INP_STRING_ENTRY)
											InpSetString(value, false, true);
										else
											InpSetNumber(atol(value), true);
										break;
									}

									// If the table name is not defined, then assign it a default name
									if (value == NULL || value[0] == '\0')
									{
										if (operation == 0)
											value = "DEFAULT_T";
										else
											value = "DEFAULT_G";
									}

									UtilStrDup(&referenceName, value);
								}

								break;
							// Pick up the index within the table to use
							case 2:
								UtilStrDup(&search, value);
								break;
							// Pick up the row. If 255, then centre
							case 3:
								row = atoi(value) - 1;
								if (inputMaxLength)
									inpEntry.row = row;
								break;
							// Pick up the column. If 255, then centre
							case 4:
								col = atoi(value) - 1;
								if (inputMaxLength)
								{
									inpEntry.col = col;
									inpEntry.length = inputMaxLength - col;
								}
								break;
							// Pickup the initial display loop value
							case 5:
								if (value[0] != '\0')
								{
									frequency = (uchar *) &(*array2)[1];
									if (firstRound == true) *frequency = atol(value);
								}
								else frequency = NULL;
								break;
							case 6:
							// Pickup the end display loop value
								if (value[0] != '\0')
									displayLoopEnd = atol(value);

								if (frequency == NULL)
									displayYes = true;
								else if (*frequency >= displayLoopEnd)
								{
									displayYes = true;
									*frequency = 0;
								}
								else (*frequency)++;
								break;
							// Pickup the time delay before displaying the next item. If it starts with '0', then permanent.
							// If not, remove at the end of the time delay
							case 7:
								if (value[0] != '\0')
								{
									if (value[0] != '0')
										permanent = false;
									timeDelay = atol(value);
								}
							default:
								break;

						}
						IRIS_StackPop(stackIndex - myStackIndex);
					}

					// No need to redisplay the permanent items/values after the first round
					if (permanent && firstRound == false)
						break;

					// Only display if the display loop variable reaches the end
					if (displayYes == false)
						break;

					// Input definitions do not have any associated displays
					if (inputMaxLength || pinEntry)
						break;

					// For battery display, no need to retrieve anything
					if (operation != 2)
					{
						// Get the text table or graphics image matching entry
						IRIS_StackPush(operation?"()GRAPHICS_IMAGE":"()TEXT_TABLE");
						IRIS_StackPush(referenceName);
						IRIS_StackPush(search);
						value = IRIS_StackGet(0);

						if (value == NULL)
						{
							value = search;

							// Disable any keyboard entry though...... This is pure text not from the prompt table.
							keyBitmap &= ~KEY_NUM_BITS;
						}
					}

					// Display the text
					if (operation == 0)
						DispText(value, row, col, false /* Clear Line */, largeFont, false /* Inverse */);
					else if (operation == 2)
					{
#ifdef __VX670
						DispUpdateBattery(row, col);
#endif
					}
					else
					{
						int handle = open(value, O_RDWR);

						if (handle >= 0)
						{
							uint length = dir_get_file_sz(value);
							if (length)
							{
								uchar * data = malloc(length);
								read(handle, (char *) data, length);
								DispGraphics(data, row, col);
								free(data);
							}
							close(handle);
						}
					}

					// Free allocated data
					IRIS_StackPop(stackIndex - myStackIndex);
					UtilStrDup(&search, NULL);
					UtilStrDup(&referenceName, NULL);

					// If we have to delay before the next element, wait for events now including this timeout
					// but this timeout should not break the loop
					if (displayTimeout && timeDelay > displayTimeout)
						displayTimeoutMultiplier -= (timeDelay / displayTimeout);
					eventOccurred = processEvent(keepEvtBitmap, keyBitmap, loopEntry, timeDelay, (displayTimeout && displayTimeoutMultiplier <= 1)? false:true, largeFont, displayTimeoutMultiplierFull, &displayTimeoutMultiplier);
				}
			}

			// Display timeout procesing
			if (eventOccurred == false && displayTimeoutMultiplier)
			{
				if (inpEntry.type != E_INP_NO_ENTRY)
				{
					displayTimeout *= displayTimeoutMultiplier;
					displayTimeoutMultiplier = 1;
				}

				eventOccurred = processEvent(keepEvtBitmap, keyBitmap, inpEntry, displayTimeout, displayTimeoutMultiplier > 1? true:false, inpLargeFont, displayTimeoutMultiplierFull, &displayTimeoutMultiplier);
				displayTimeoutMultiplier--;
			}
		}
	}

	// Deallocate the animation object used during this display
	IRIS_DeallocateStringValue(animation);

	// Deallocate the event object
	IRIS_DeallocateStringValue(events);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : processDisplayObject
//
// DESCRIPTION:	Obtains the current object data:
//					If not found, it returns with a failure
//					If found, it authenticates the object using the signing key.
//						If authentication succeeds, it processes the object and returns with a success.
//						If authentication fails, it displays an auth error and awaits a CANCEL
//							 or CLR key and returns with a failure.
//
// PARAMETERS:	objectName	<=	The name of the object to retrieve
//
// RETURNS:		Track 1 string if available or an empty string
//-------------------------------------------------------------------------------------------
//
static void processDisplayObject()
{
	// Get the object
	if ((currentObjectData = IRIS_GetObjectData(currentObject, &currentObjectLength)) == NULL)
		return;

	// Authenticate the object just retrieved

	// Initialise the stack for the new object
	IRIS_StackInit(0);

	// Find and process the animation object
	processDisplayObject2();

	// House keep and finish
	free(currentObjectData);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : Main function
//
// DESCRIPTION:	Sets the current object to boot and then let it all roll!!!
//
//				It also has a list of internal objects to use if they are not externally overriden
//				relating to booting, configuring and contacting the iRIS host.
//
//				The external signed object takes precedence first, then the internal objects. If
//				none are found. The terminal will be get out and back to the previous object. The 
//				user can then select a different action or try again.
//
// PARAMETERS:	None
//
// RETURNS:		Track 1 string if available or an empty string
//-------------------------------------------------------------------------------------------
//
#ifdef _DEBUG
testMain()
#else
main()
#endif
{
	// Set the initial object to __BOOT which is the boot loader
	UtilStrDup(&currentObject, "__BOOT");

	while(1)
	{
		// Process the current object
		processDisplayObject();

		// Switch to the new object
		UtilStrDup(&prevObject, currentObject);

		if (nextObject)
			UtilStrDup(&currentObject, nextObject);
	}
}
