#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <auris.h>
#include <svc.h>
#include <svc_sec.h>

#ifdef __VMAC
#	include "eeslapi.h"
#	include "logsys.h"
#	include "devman.h"
#	include "vmac.h"
#endif

#include "alloc.h"
#include "display.h"
#include "input.h"
#include "printer.h"

#include "as2805.h"
#include "as2805.h"
#include "security.h"
#include "utility.h"
#include "iris.h"
#include "irisfunc.h"

#include "comms.h"
int old_ticks = 0;

int debug = 0;

#ifdef _DEBUG
#define STDIN 1
write_at(char * tempBuf, int length, int q, int w) {}
void DispInit(void){}
#endif

int conHandle = -1;
bool animationOK;
int getObjectAgain = 0;

// We must define these as they are expected by iRIS to define the scope of object and string resolution
char * currentObject = NULL;
char * currentObjectData = NULL;
uint currentObjectLength;
char * currentObjectVersion = NULL;
char * currentObjectGroup = NULL;
char * currentEvent = NULL;
char * currentEventValue = NULL;
char * newEventValue = NULL;
char * errorObjectGroup = NULL;

char * prevObject = NULL;
char * nextObject = NULL;

// Global variables mainly used during callbacks
bool callbackMode = false;
char * forceNextObject = NULL;

unsigned char lastKey;

#define	MAX_DATA	40
struct
{
	char name[50];
	char value[100];
} data[MAX_DATA];

typedef struct
{
	T_KEYBITMAP keyBitmap;
	T_EVTBITMAP evtBitmap;
	char * action;
	char * object;
} T_MAP;

T_MAP map[30];
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
	{EVT_SERIAL2_DATA,	"SER2_DATA"},
	{EVT_INIT0,			"INIT0"},
	{EVT_INIT,			"INIT"},
	{EVT_INIT2,			"INIT2"},
	{EVT_LAST,			"LAST"},
	{EVT_LAST2,			"LAST2"},
	{EVT_NONE,			NULL}
};

static const struct
{
	uchar key;
	T_KEYBITMAP keyBitmap;
	char * event;
} key_string[] =
{
	{KEY_ALL,		KEY_ALL_BITS,			"KEY_ALL"},
	{KEY_NUM,		KEY_NUM_BITS,			"KEY_NUM"},
	{KEY_NO_PIN,	KEY_NO_PIN_BIT,			"KEY_NO_PIN"},
	{KEY_0,			KEY_0_BIT,				"KEY_0"},
	{KEY_1,			KEY_1_BIT,				"KEY_1"},
	{KEY_2,			KEY_2_BIT,				"KEY_2"},
	{KEY_3,			KEY_3_BIT,				"KEY_3"},
	{KEY_4,			KEY_4_BIT,				"KEY_4"},
	{KEY_5,			KEY_5_BIT,				"KEY_5"},
	{KEY_6,			KEY_6_BIT,				"KEY_6"},
	{KEY_7,			KEY_7_BIT,				"KEY_7"},
	{KEY_8,			KEY_8_BIT,				"KEY_8"},
	{KEY_9,			KEY_9_BIT,				"KEY_9"},
	{KEY_SK1,		KEY_SK1_BIT,			"KEY_SK1"},
	{KEY_SK2,		KEY_SK2_BIT,			"KEY_SK2"},
	{KEY_SK3,		KEY_SK3_BIT,			"KEY_SK3"},
	{KEY_SK4,		KEY_SK4_BIT,			"KEY_SK4"},
	{KEY_F0,		KEY_F0_BIT,				"KEY_F0"},
	{KEY_F1,		KEY_F1_BIT,				"KEY_F1"},
	{KEY_F2,		KEY_F2_BIT,				"KEY_F2"},
	{KEY_F3,		KEY_F3_BIT,				"KEY_F3"},
	{KEY_F4,		KEY_F4_BIT,				"KEY_F4"},
	{KEY_F5,		KEY_F5_BIT,				"KEY_F5"},
	{KEY_CNCL,		KEY_CNCL_BIT,			"CANCEL"},
	{KEY_CLR,		KEY_CLR_BIT,			"KEY_CLR"},
	{KEY_LCLR,		KEY_LCLR_BIT,			"KEY_LCLR"},
	{KEY_OK,		KEY_OK_BIT,				"KEY_OK"},
	{KEY_ASTERISK,	KEY_ASTERISK_BIT,		"KEY_ASTERISK"},
	{KEY_FUNC,		KEY_FUNC_BIT,			"KEY_FUNC"},
	{KEY_NONE,		0,						NULL}
};

ulong displayTimeout;
int displayTimeoutMultiplier;
int displayTimeoutMultiplierFull;
bool vmacRefresh;

static uchar iris_ktk1[17] = "\x43\x2B\x37\xD4\x64\xD7\xBF\x5C\x5D\xD2\x5D\xC5\xC2\x1D\x9B\xB8";

static bool processEvent2(T_EVTBITMAP evtBitmap, uchar key, T_KEYBITMAP keyBitmap, char * event)
{
	int i;
	bool eventOccurred = false;
	int myStackIndex = stackIndex;

	// Set some information about terminal activity
	UtilStrDup(&newEventValue, event);

	// Check if an event has occurred
	for (i = 0; i < mapIndex; i++)
	{
		if ((keyBitmap & map[i].keyBitmap) || (key == KEY_NONE && evtBitmap == map[i].evtBitmap))
		{
#ifdef _DEBUG
			printf(">>>>> event = %s\n", event);
#else
//			if (debug)
//			{
//				char keycode;
//				char tempBuf[200];
//				DispInit();
//				sprintf(tempBuf, "Event: %s ", event);
//				write_at(tempBuf, strlen(tempBuf), 1, 2);
//				while(read(STDIN, &keycode, 1) != 1);
//			}
#endif

			// Process the action
			IRIS_ProcessActionObject(map[i].action);

			// Determine the next object
			IRIS_ResolveToSingleValue(map[i].object, false);

			if (IRIS_StackGet(0) && IRIS_StackGet(0)[0])
			{
#ifdef _DEBUG
				printf("<<<<< Next Object: %s\n", IRIS_StackGet(0)? IRIS_StackGet(0):"None = stay");
#else
//				if (strcmp(IRIS_StackGet(0), "iVSTOCK_REMOTE") == 0)
//					debug = 1;
//
//				if (debug)
//				{
//					char keycode;
//					char tempBuf[200];
//					DispInit();
//					sprintf(tempBuf, "Current Object: %s, Event: %s, Next Object: %s     ", currentObject, event, IRIS_StackGet(0)? IRIS_StackGet(0):"None = stay");
//					write_at(tempBuf, strlen(tempBuf), 1, 2);
//					while(read(STDIN, &keycode, 1) != 1);
//				}
#endif

				if (strcmp(IRIS_StackGet(0), "__NO_ANIMATION") == 0)
					animationOK = false;
				else
				{
					UtilStrDup(&nextObject, IRIS_StackGet(0));
					currentEvent = (key == KEY_NONE?"EVENT":"KEY");
					UtilStrDup(&currentEventValue, event);
					IRIS_StackPop(1);
					eventOccurred = true;
				}
			}
			IRIS_StackPop(stackIndex - myStackIndex);

			if (eventOccurred == false && (	map[i].evtBitmap == EVT_INIT0 || map[i].evtBitmap == EVT_INIT || map[i].evtBitmap == EVT_INIT2 ||
											map[i].evtBitmap == EVT_LAST || map[i].evtBitmap == EVT_LAST2 ) )
			{
				// INIT & LAST events run only once and all in one go.
				map[i].evtBitmap = EVT_NONE;
				continue;
			}
			else break;

/*																		{
																			char tempBuf[40];
																			sprintf(tempBuf, "Test 3 ");
																			DispText(tempBuf, 2, 1, false, false, false);
																		}
*/		}
	}

	// Process any last minute cleanups
	if (eventOccurred && evtBitmap != EVT_LAST)
	{
		processEvent2(EVT_LAST, KEY_NONE, KEY_NO_BITS, "LAST");
		if (strcmp(currentObject, nextObject))
			processEvent2(EVT_LAST2, KEY_NONE, KEY_NO_BITS, "LAST2");
	}

	if (!callbackMode && forceNextObject)
	{
		UtilStrDup(&nextObject, forceNextObject);
		UtilStrDup(&forceNextObject, NULL);
		eventOccurred = true;
	}

	return eventOccurred;
}

char * getLastKeyDesc(unsigned char key, T_KEYBITMAP * keyBitmap)
{
	int i;

	// Convert the key pressed to a bitmap
	for (i = 0; key_string[i].event; i++)
	{
		if (key == key_string[i].key)
		{
			if (keyBitmap)
				*keyBitmap = key_string[i].keyBitmap;
			break;
		}
	}

	return key_string[i].event;
}

static bool processEvent(T_EVTBITMAP setEvtBitmap, T_KEYBITMAP keepKeyBitmap, T_INP_ENTRY inpEntry, ulong timeout, bool ignoreTimeout, bool largeFont, bool * flush)
{
	int i;
	T_KEYBITMAP keyBitmap = KEY_NO_BITS;
	T_EVTBITMAP evtBitmap = EVT_NONE;
	uchar key;
	char * event;
	bool keyPress = false;

	// Wait for an event to occur
	evtBitmap = setEvtBitmap;
	lastKey = key = InpGetKeyEvent(keepKeyBitmap, &evtBitmap, inpEntry, timeout * 10, largeFont, *flush, &keyPress);
	*flush = false;

	// Rearm the timer to the fullest if we detected a key press while waiting for a fraction of the time.
	if (ignoreTimeout && keyPress)
		displayTimeoutMultiplier = displayTimeoutMultiplierFull;

	// Do not generate a timeout event if we are ignoring timeouts...This is if we are updating elements within the screen
	if (ignoreTimeout && key == KEY_NONE && evtBitmap == EVT_TIMEOUT)
		return false;

	// Get the non-key event description
	if (key == KEY_NONE)
	{
		for (i = 0; event_string[i].event; i++)
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
		event = getLastKeyDesc(key, &keyBitmap);

	// Check if an event has occurred
	return processEvent2(evtBitmap, key, keyBitmap, event);
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

					// Check that we have an element for first (KEY or EVENT or blank) and third (Action Object) elements
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
									if (strcmp(value, "MCR") == 0)
										*keepEvtBitmap |= EVT_MCR, map[mapIndex].evtBitmap = EVT_MCR;
									else if (strcmp(value, "SER_DATA") == 0)
										*keepEvtBitmap |= EVT_SERIAL_DATA, map[mapIndex].evtBitmap = EVT_SERIAL_DATA;
									else if (strcmp(value, "SER2_DATA") == 0)
										*keepEvtBitmap |= EVT_SERIAL2_DATA, map[mapIndex].evtBitmap = EVT_SERIAL2_DATA;
									else if (strcmp(value, "INIT0") == 0)
										map[mapIndex].evtBitmap = EVT_INIT0;
									else if (strcmp(value, "INIT") == 0)
										map[mapIndex].evtBitmap = EVT_INIT;
									else if (strcmp(value, "INIT2") == 0)
										map[mapIndex].evtBitmap = EVT_INIT2;
									else if (strcmp(value, "LAST") == 0)
										map[mapIndex].evtBitmap = EVT_LAST;
									else if (strcmp(value, "LAST2") == 0)
										map[mapIndex].evtBitmap = EVT_LAST2;
									else
									{
										*keepEvtBitmap |= EVT_TIMEOUT, map[mapIndex].evtBitmap = EVT_TIMEOUT;
										displayTimeoutMultiplier = displayTimeoutMultiplierFull = 1;
									}
								}
								else
								{
									int index;
									char ** array3 = (char **) &((*array2)[4]);

									for (index = 0; *array3; array3++, index++)
									{
										char * value;
										int myStackIndex = stackIndex;

										IRIS_ResolveToSingleValue(*array3, false);
										value = IRIS_StackGet(0);

										if (index == 0)
										{
											if (strcmp(value, "TIME") == 0)
												*keepEvtBitmap |= EVT_TIMEOUT, map[mapIndex].evtBitmap = EVT_TIMEOUT;
										}
										else if (index == 1)
										{
											if (value)
												displayTimeoutMultiplier = displayTimeoutMultiplierFull = atol(value) / 100;
											else
												displayTimeoutMultiplier = displayTimeoutMultiplierFull = 1;
											displayTimeout = 100;
										}

										IRIS_StackPop(stackIndex - myStackIndex);
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
	T_EVTBITMAP keepEvtBitmap = EVT_NONE;
	T_INP_ENTRY inpEntry, loopEntry;
	bool inpLargeFont = false;
	bool inpDisabled = false;

	char * animation;
	char * events;
	char * clear;
	bool firstRound;
	bool eventOccurred = false;
	bool init2 = true;

	char * stringValue;
	bool flush = true;
	bool nonPermanent = false;
	bool dispCleared = false;
	bool reDisplay[50];

	// After lots os trial and error, I found the Verifone terminal to benefit significanty in speed if I
	// clear the latest text table prior to displaying the IDLE screen. Visiting another VAA will also do,
	// but this is a manual process. At the end, I think we need our own malloc to speed things up instead
	// of using the system heap...
//	if (strcmp(currentObject, "IDLE") == 0)
//	{
//		UtilStrDup(&currentTableName, NULL);
//		UtilStrDup(&currentTableData, NULL);
//		currentTableValid = false;
//		if (currentTablePromptValue)
//		{
//			IRIS_DeallocateStringValue(currentTablePromptValue);
//			currentTablePromptValue = NULL;
//		}
//	}

	// Initialisation
	animationOK = true;
	memset(reDisplay, 0, sizeof(reDisplay));
	memset(track1, 0, sizeof(track1));
	memset(track2, 0, sizeof(track2));
	track1Len = 80;
	track2Len = 40;
	memset(&inpEntry, 0, sizeof(inpEntry));
	memset(&loopEntry, 0, sizeof(loopEntry));
	displayTimeout = 0;
	displayTimeoutMultiplier = 0;
	displayTimeoutMultiplierFull = 0;

	// Get the current object type
	stringValue = IRIS_GetStringValue(currentObjectData, currentObjectLength, "TYPE", false);

	// The object MUST be a display object. Otherwise, it is not displayable
	if (!stringValue || stringValue[0] || (strcmp(&stringValue[4], "DISPLAY") && strcmp(&stringValue[4], "PROFILE")))
	{
		// Set the error line displays
		IRIS_StoreData("/__ERRMSG/RETURN", prevObject, false);
		IRIS_StoreData("/__ERRMSG/L1F", "i-RIS", false);
		IRIS_StoreData("/__ERRMSG/L4", currentObject, false);
		IRIS_StoreData("/__ERRMSG/L5", "IS", false);
		IRIS_StoreData("/__ERRMSG/L6", "NOT A DISPLAY OBJECT", false);

		// Remove the path to the object and link back to the previous display object
		// __ERR_DISP will go there when done.
		UtilStrDup(&currentObject, prevObject);

		// Set the new object to be the error display object
		UtilStrDup(&nextObject, "__ERRMSG");
		return;
	}
	IRIS_DeallocateStringValue(stringValue);

	// Update the current object version
	stringValue = IRIS_GetStringValue(currentObjectData, currentObjectLength, "VERSION", false);
	UtilStrDup(&currentObjectVersion, (stringValue && stringValue[0] == 0)? &stringValue[4]:"");
	IRIS_DeallocateStringValue(stringValue);

	// Update the current object group
	stringValue = IRIS_GetStringValue(currentObjectData, currentObjectLength, "GROUP", false);
	UtilStrDup(&currentObjectGroup, (stringValue && stringValue[0] == 0)? &stringValue[4]:"");
	IRIS_DeallocateStringValue(stringValue);

#ifdef _DEBUG
	printf("**********************************************************************************************************************************************************\n");
	printf("**************** NEW OBJECT: %s (%s) %s \n", currentObject, currentObjectGroup, currentObjectVersion);
	printf("**********************************************************************************************************************************************************\n");
#else
//	if (debug)
//	{
//		char keycode;
//		char tempBuf[200];
//		DispInit();
//		sprintf(tempBuf, "New Object: %s (%s) %s ", currentObject, currentObjectGroup, currentObjectVersion);
//		write_at(tempBuf, strlen(tempBuf), 1, 2);
//		while(read(STDIN, &keycode, 1) != 1);
//	}
#endif

	// Look for data to clear on startup
	clear = IRIS_GetStringValue(currentObjectData, currentObjectLength, "CLEAR", false);
	if (clear && clear[0] == 1)
	{
		char ** array1 = (char **) &clear[4];

		for (; *array1; array1++)
		{
			char fullName[100];
			char * simpleString = &(*array1)[4];

			// Check that we have a simple string
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
	IRIS_StackPush(currentObject);
	IRIS_StackPush("NEW_OBJECT");
	__new_object();
	IRIS_StackPop(1);

	// Look for events to process
	events = IRIS_GetStringValue(currentObjectData, currentObjectLength, "PATH", false);
	processPath(events, &keyBitmap, &keepEvtBitmap);

	// Check for INIT0 events now and perform any initial actions now
	if (strcmp(prevObject,currentObject))
		eventOccurred = processEvent2(EVT_INIT0, KEY_NONE, KEY_NO_BITS, "INIT0");

	// Check for INIT events now and perform any initial actions now
	if (eventOccurred == false)
		eventOccurred = processEvent2(EVT_INIT, KEY_NONE, KEY_NO_BITS, "INIT");

	// Get the value of the animation string from the display object
	if (animationOK)
		animation = IRIS_GetStringValue(currentObjectData, currentObjectLength, "ANIMATION", false);
	else
	{
		flush = false;
		animation = NULL;
	}

/*																		{
																			char tempBuf[40];
																			sprintf(tempBuf, "Object Start 2 ");
																			write_at(tempBuf, strlen(tempBuf), 1, 2);
																		}
*/
	// Keep looping waiting for events to occur
	for(firstRound = true, vmacRefresh = false; eventOccurred == false && vmacRefresh == false; firstRound = false)
	{
		// We expect an array of arrays of strings, so examine this
		if (animation && animation[0] == 1)	// An array
		{
			int i;
			char ** array1 = (char **) &animation[4];

			for(i = 0; eventOccurred == false && *array1; array1++, i++)
			{
				// No need to redisplay permanent 
				if (!firstRound && !reDisplay[i]) continue;

				if ((*array1)[0] == 1)		// An array
				{
					int index;
					int	operation = 0;
					bool inverse = false;
					bool largeFont = false;
					char * search = NULL;
					char * referenceName = NULL;
					int row = 0, col = 0;
					ulong timeDelay = 0;
					char * erase = NULL;
					int inputMaxLength = 0;
					char ** array2 = (char **) &((*array1)[4]);
					uchar * frequency;
					uchar displayLoopEnd = 0;
					bool displayYes = false;
					int myStackIndex = stackIndex;
					char * value;
					bool skipDisplay = false;
					bool hidden = false;

					for(index = 0; *array2; array2++, index++)
					{

						IRIS_ResolveToSingleValue(*array2, false);
						value = IRIS_StackGet(0);

						switch(index)
						{
							// Pick up the type of the animation GRAPH or TEXT
							case 0:
								if (value)
								{
									if (strcmp(value, "GRAPH") == 0)
										operation = 1;
									else if (strcmp(value, "TEXT") == 0)
										operation = 0;
									else if (strcmp(value, "INV") == 0)
										inverse = true;
									else if (strcmp(value, "LARGE") == 0)
										largeFont = true;
									else if (strcmp(value, "BATTERY") == 0)
										operation = 2;
									else if (strcmp(value, "SIGNAL") == 0)
										operation = 3;
									else if (strcmp(value, "LINV") == 0)
									{
										largeFont = true;
										inverse = true;
									}
									else if (strcmp(value, "AMOUNT") == 0)
									{
										inpEntry.type = E_INP_AMOUNT_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "PERCENT") == 0)
									{
										inpEntry.type = E_INP_PERCENT_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "DECIMAL") == 0)
									{
										inpEntry.type = E_INP_DECIMAL_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "DATE") == 0)
									{
										inpEntry.type = E_INP_DATE_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "MMYY") == 0)
									{
										inpEntry.type = E_INP_MMYY_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "TIME") == 0)
									{
										inpEntry.type = E_INP_TIME_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "NUMBER") == 0 || strcmp(value, "HIDDEN") == 0)
									{
										if (strcmp(value, "HIDDEN") == 0) hidden = true;
										inpEntry.type = E_INP_STRING_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "STRING") == 0)
									{
										inpEntry.type = E_INP_STRING_ALPHANUMERIC_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "HEX") == 0)
									{
										inpEntry.type = E_INP_STRING_HEX_ENTRY;
										inputMaxLength = 21;
									}
									else if (strcmp(value, "LAMOUNT") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_AMOUNT_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LPERCENT") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_PERCENT_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LDECIMAL") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_DECIMAL_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LDATE") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_DATE_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LMMYY") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_MMYY_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LTIME") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_TIME_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LNUMBER") == 0 || strcmp(value, "LHIDDEN") == 0)
									{
										if (strcmp(value, "LHIDDEN") == 0) hidden = true;
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_STRING_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LSTRING") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_STRING_ALPHANUMERIC_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "LHEX") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_STRING_HEX_ENTRY;
										inputMaxLength = 16;
									}
									else if (strcmp(value, "PIN") == 0)
									{
										inpLargeFont = largeFont = true;
										inpEntry.type = E_INP_PIN;
										inputMaxLength = 16;
									}
								}
								break;

							case 1:
								// If this is an input entry, get the default/current value
								if (inputMaxLength)
								{
									if (inpEntry.type == E_INP_AMOUNT_ENTRY || inpEntry.type == E_INP_PERCENT_ENTRY || inpEntry.type == E_INP_DECIMAL_ENTRY)
										InpSetNumber(value?atol(value):0, true);
									else
										InpSetString(value?value:"", (bool) (hidden?true:false), true);
									break;
								}

								// If the table name is not defined, then assign it a default name
								if (!value || value[0] == '\0')
									value = "DEFAULT_T";

								UtilStrDup(&referenceName, value);

								break;
							// Pick up the index within the table to use
							case 2:
								if (inputMaxLength && value && value[0] == '1')
									InpSetOverride(false);
								
								if (value)
									UtilStrDup(&search, value);
								break;
							// Pick up the row. If 255, then centre
							case 3:
								if (value)
								{
									if (value[0] == 's')
									{
										#ifdef __VX670
											#if 0
											if (largeFont)
											{
												row = atoi(&value[1]);
												if (row >= 4)
													row += 2;
												else if (row >= 2)
													row += 1;
											}
											else
											#else
											largeFont = 0;
											#endif
												row = atoi(&value[1]) * 3;
										#else
										#ifdef __VX810
											if (largeFont)
											{
												row = atoi(&value[1]);
												if (row == 1)
													row = 0;
												else if (row == 2)
													row = 2;
												else if (row == 3)
													row = 5;
												else if (row == 4)
													row = 7;
												else row = 20;
											}
											else
											{
												row = atoi(&value[1]);
												if (row == 1)
													row = 1;
												else if (row == 2)
													row = 5;
												else if (row == 3)
													row = 10;
												else if (row == 4)
													row = 14;
												else row = 20;
											}
										#else
											row = atoi(&value[1]) - 1;
											if (!largeFont)
												row = (atoi(&value[1]) - 1) * 2;
										#endif
										#endif
									}
									else if (value[0] == 'B')
									{
										#if defined(__VX670) || defined(__VX810)
											row = 15;
										#else
											row = 7;
										#endif
									}
									else
									{
										row = atoi(value) - 1;
										#if defined(__VX670) || defined(__VX810)
											if (largeFont)
												row += 2;
											else
												row += 4;
										#endif
									}
								}
								if (inputMaxLength)
									inpEntry.row = row;
								break;
							// Pick up the column. If 255, then centre
							case 4:
								if (value)
								{
									if (value[0] == 'C')
										col = 255;
									else if (value[0] == 'R')
										col = 254;
									else
										col = atoi(value) - 1;
								}

								if (inputMaxLength)
								{
									inpEntry.col = col;
									inpEntry.length = inputMaxLength - col;
								}
								break;
							// Pickup the initial display loop value
							case 5:
								if (value && value[0] != '\0')
								{
									if (inputMaxLength)
										inpEntry.length = atoi(value);
									else
									{
										frequency = (uchar *) &(*array2)[1];
										if (firstRound == true) *frequency = atoi(value);
									}
								}
								else frequency = NULL;
								break;
							case 6:
							// Pickup the end display loop value
								if (value && value[0] != '\0')
								{
									if (inputMaxLength)
									{
										inpEntry.minLength = atol(value);
										break;
									}
									else
										displayLoopEnd = atoi(value);
								}

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
								if (value)
								{
									if (value[0] != '0')
									{
										if (inputMaxLength)
										{
											inpEntry.decimalPoint = atoi(value);
											break;
										}

										reDisplay[i] = true;
										nonPermanent = true;
									}
									timeDelay = atol(value);
								}
							case 8:
								if (value && value[0] == 'Y')
								{
									erase = my_malloc(22);
									erase[0] = '\0';
								}
							default:
								break;
						}
						IRIS_StackPop(stackIndex - myStackIndex);
					}

					// If we have not reached the end of the loop for non-permanent displays, do not display yet
					if (reDisplay[i] && displayYes == false)
						skipDisplay = true;

					// Input definitions do not have any associated displays
					if (inputMaxLength)
						skipDisplay = true;

					if (skipDisplay == false)
					{
						// Initialisation
						value = NULL;

						// Get the text table or graphics image matching entry
						if (search)
						{
							if (strcmp(referenceName, "THIS") && operation == 0)
							{
								IRIS_StackPushFunc("()TEXT_TABLE");
								IRIS_StackPush(referenceName);
								IRIS_StackPush(search);
								value = IRIS_StackGet(0);
							}
							else
							{
								int t;

								value = search;

								// Disable any keyboard entry...... This is pure text not from the prompt table
								// but allow amount displays such as spaces, $, and numbers...NO alpha characters
								for (t = 0; operation == 0 && search[t] != '\0'; t++)
								{
									if ((search[t] >= 'a' && search[t] <= 'z') || (search[t] >= 'A' && search[t] <= 'Z'))
									{
										inpDisabled = true;
										break;
									}
								}
							}
						}

						// Display the text
						if (value)
						{
							if (operation == 0)
							{
								// Clear the screen for the first round
								if (dispCleared == false)
								{
									DispClearScreen();
									dispCleared = true;
								}

								DispText(value, row, col, false /* Clear Line */, largeFont, inverse);
								if (erase)
								{
									memset(erase, ' ', strlen(value));
									erase[strlen(value)] = '\0';
								}
							}
							else if (operation == 2)
							{
#ifdef __VX670
								DispUpdateBattery(row, col);
#endif
							}
							else if (operation == 3)
							{
#ifdef __VX670
								DispSignal(row, col);
#endif
							}
							else
							{
								unsigned int length;
								char * objectData;

								// Obtain the graphics object locally or go to the host if not available
								if ((objectData = IRIS_GetObjectData(value, &length)) == NULL)
								{
									IRIS_DownloadResourceObject(value);
									objectData = IRIS_GetObjectData(value, &length);
								}

								if (objectData)
								{
									char * type = IRIS_GetStringValue(objectData, length, "TYPE", false);

									if (type)
									{
										if (strcmp(&type[4], "IMAGE") == 0)
										{
											char * image = IRIS_GetStringValue(objectData, length, "IMAGE", false);

											if (image)
											{
												if (image[0] == 0)
												{
													unsigned char * imageb = my_malloc(strlen(&image[4]) / 2 + 1);
													UtilStringToHex(&image[4], strlen(&image[4]), imageb);

													// Clear the screen for the first round
													if (dispCleared == false)
													{
														DispClearScreen();
														dispCleared = true;
													}

													DispGraphics(imageb, row, col);
													my_free(imageb);
												}

												IRIS_DeallocateStringValue(image);
											}
										}

										IRIS_DeallocateStringValue(type);
									}

									my_free(objectData);
								}
							}
						}
					}

					// my_free allocated data
					IRIS_StackPop(stackIndex - myStackIndex);
					UtilStrDup(&search, NULL);
					UtilStrDup(&referenceName, NULL);

					// If we have to delay before the next element, wait for events now including this timeout
					// but this timeout should not break the loop
					if (displayTimeout && timeDelay > displayTimeout && displayTimeoutMultiplier)
					{
						displayTimeoutMultiplier -= (timeDelay / displayTimeout);
						if (displayTimeoutMultiplier < 0) displayTimeoutMultiplier = 0;
					}
					if (eventOccurred == false && (timeDelay || firstRound == false))
						eventOccurred = processEvent(keepEvtBitmap | EVT_TIMEOUT, keyBitmap, loopEntry, timeDelay, (bool) ((displayTimeout && displayTimeoutMultiplier <= 1)? false:true), largeFont, &flush);
					if (erase)
					{
						if (value && operation == 0)
							DispText(erase, row, col, false /* Clear Line */, largeFont, false /* Inverse */);
						UtilStrDup(&erase, NULL);
					}
				}
			}
		}

		// Disable numbers if input disabled
		if (inpDisabled == true)
		{
			if (inpEntry.type != E_INP_PIN)
				inpEntry.type = E_INP_NO_ENTRY;
			keyBitmap &= ~KEY_NUM_BITS;
		}

		// Clear the screen if not already cleared
		if (dispCleared == false && inpEntry.type != E_INP_NO_ENTRY)
		{
			DispClearScreen();
			dispCleared = true;
		}

		// Initialisation (after display) processing
		if (eventOccurred == false && init2)
		{
			eventOccurred = processEvent2(EVT_INIT2, KEY_NONE, KEY_NO_BITS, "INIT2");
			init2 = false;
		}

		// Display timeout procesing
		if (eventOccurred == false)
		{
			if (inpEntry.type != E_INP_NO_ENTRY)
			{
				displayTimeout *= displayTimeoutMultiplier;
				displayTimeoutMultiplier = 1;
			}

			eventOccurred = processEvent(keepEvtBitmap | (nonPermanent?EVT_TIMEOUT:EVT_NONE), keyBitmap, inpEntry, displayTimeout, (bool) (displayTimeoutMultiplier > 1? true:false), inpLargeFont, &flush);
			if (displayTimeoutMultiplier) displayTimeoutMultiplier--;
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
	{
		if (++getObjectAgain >= 2)
		{
			char temp[30];

			// Set the error line displays
			IRIS_StoreData("/__ERRMSG/RETURN", currentObject, false);
			IRIS_StoreData("/__ERRMSG/L1F", "i-RIS", false);
			IRIS_StoreData("/__ERRMSG/L3", currentObject, false);
			if (currentObjectGroup && currentObjectGroup[0])
			{
				sprintf(temp, "From: (%s)", currentObjectGroup);
				UtilStrDup(&errorObjectGroup, temp);
			}
			IRIS_StoreData("/__ERRMSG/L4", errorObjectGroup, false);
			IRIS_StoreData("/__ERRMSG/L5", "IS MISSING /INVALID", false);
			IRIS_StoreData("/__ERRMSG/L7", "PRESS OK TO TRY AGAIN", false);

			// Set the new object to be the error display object
			UtilStrDup(&nextObject, "__ERRMSG");
		}

		return;
	}
	else if (strcmp(currentObject, "__ERRMSG"))
		getObjectAgain = 0;

	// Initialise the stack for the new object, but we can't do this in callback mode.
	// This was a precaution anyways to recover from badly formatted objects that leave something in the stack.
	// Since it is expected that the callback objects will have a short life, this will most likely not be required... To be verified later...
	if (!callbackMode)
		IRIS_StackInit(0);

	// Find and process the animation object
	processDisplayObject2();

	// House keep and finish
	UtilStrDup(&currentObjectData, NULL);
}

static void processObjectLoop()
{
	while(1)
	{
		// Initialisation
		mapIndex = 0;
		memset(map, 0, sizeof(map));

		// Process the current object
		processDisplayObject();

		// If we are in "callback mode" and the next object terminate, then end the loop
		if (callbackMode && strcmp(nextObject, "__ENDCALLBACK__") == 0)
			return;

		// Switch to the new object
		UtilStrDup(&prevObject, currentObject);

		if (nextObject)
			UtilStrDup(&currentObject, nextObject);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : executeCallbackObjects
//
// DESCRIPTION:	Starts a new Child Application "machine" without affecting the current
//				object, its context or stack state.
//
//				When done, the new object executing during the callback is cleared completely
//				and all memory allocated is cleared. The current object context and stack are then restored
//				and execution continues as normal.
//
//				This is mainly called from within the 'C' program to execute an object
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void executeCallbackObjects(char * callbackObject)
{
	// Preserve the current object context
	char *	preserve__currentObject = currentObject;
	char *	preserve__currentObjectData = currentObjectData;
	uint	preserve__currentObjectLength = currentObjectLength;
	char *	preserve__currentObjectVersion = currentObjectVersion;
	char *	preserve__currentObjectGroup = currentObjectGroup;
	char *	preserve__currentEvent = currentEvent;
	char *	preserve__currentEventValue = currentEventValue;
	char *	preserve__NewEventValue = newEventValue;
	char *	preserve__errorObjectGroup = errorObjectGroup;

	char *	preserve__prevObject = prevObject;
	char *	preserve_nextObject = nextObject;

	ulong preserve_displayTimeout = displayTimeout;
	int preserve_displayTimeoutMultiplier = displayTimeoutMultiplier;
	int preserve_displayTimeoutMultiplierFull = displayTimeoutMultiplierFull;
	bool preserve_vmacRefresh = vmacRefresh;

	T_MAP preserve_map[30];
	int preserve_mapIndex;
	memcpy(preserve_map, map, sizeof(map));
	preserve_mapIndex = mapIndex;

	// Callback function cannot be executed when already in callback mode
	if (callbackMode)
		return;

	// Switch to 'Callback mode' and initialise
	callbackMode = true;
	currentObject = prevObject = nextObject =
	currentObjectData = currentObjectGroup = currentObjectVersion =
	currentEvent = currentEventValue = newEventValue = errorObjectGroup = NULL;	

	// Set the initial object to callbackObject which is the boot loader
	UtilStrDup(&currentObject, callbackObject);
	UtilStrDup(&prevObject, callbackObject);
	UtilStrDup(&nextObject, callbackObject);

	// Execute the callback object loop
	stackLevel++;
	processObjectLoop();
	stackLevel--;

	// Restore the object context
	currentObject = preserve__currentObject;
	currentObjectData = preserve__currentObjectData;
	currentObjectLength = preserve__currentObjectLength;
	currentObjectVersion = preserve__currentObjectVersion;
	currentObjectGroup = preserve__currentObjectGroup;
	currentEvent = preserve__currentEvent;
	currentEventValue = preserve__currentEventValue;
	newEventValue = preserve__NewEventValue;
	errorObjectGroup = preserve__errorObjectGroup;

	prevObject = preserve__prevObject;
	nextObject = preserve_nextObject;

	mapIndex = preserve_mapIndex;
	memcpy(map, preserve_map, sizeof(map));

	displayTimeout = preserve_displayTimeout;
	displayTimeoutMultiplier = preserve_displayTimeoutMultiplier;
	displayTimeoutMultiplierFull = preserve_displayTimeoutMultiplierFull;
	vmacRefresh = preserve_vmacRefresh;

	// House keeping
	callbackMode = false;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : Main function
//
// DESCRIPTION:	Sets the current object to boot and then let's it all roll!!!
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
main(int argc, char * argv[])
#endif
{
	int i;
	uchar ktk[16];
#ifdef __VMAC
	unsigned char gEeslData[EESL_DATA_SIZE_EX];
#endif

	// Initialisation
#ifndef __VMAC
	SecurityInit();
#endif
	__tcp_init();
	__pstn_init();
	__ser_init();
	UtilStrDup(&currentObjectGroup, irisGroup);

	// Transform the KTK components....
	for (i = 0; i < 16; i+=2)
	{
		iris_ktk1[i] = iris_ktk1[i] ^ 'U';
		iris_ktk1[i+1] = iris_ktk1[i+1] ^ ('S' | 0x80);

		iris_ktk2[i] = iris_ktk2[i] ^ 'G';
		iris_ktk2[i+1] = iris_ktk2[i+1] ^ ('R' | 0x80);

		iris_ktk3[i] = iris_ktk3[i] ^ 'I';
		iris_ktk3[i+1] = iris_ktk3[i+1] ^ ('S' | 0x80);
	}

	// Create and store the KTK
	for (i = 0; i < 16; i++)
		ktk[i] = iris_ktk1[i] ^ iris_ktk2[i] ^ iris_ktk3[i];
	SecurityWriteKey(irisGroup, 253, 16, ktk);


	// Set the initial object to __BOOT which is the boot loader
	UtilStrDup(&currentObject, "__BOOT");
	UtilStrDup(&prevObject, "__BOOT");
	UtilStrDup(&nextObject, "__BOOT");


#ifdef __VMAC
	// Log init
	LOG_INIT("IRIS", LOGSYS_COMM,LOGSYS_PRINTF_FILTER|EESL_DEBUG_GLOBAL|EESL_DEBUG_SEND_SUCCESS|EESL_DEBUG_RECEIVE_SUCC);
	
	EESL_InitialiseEx(argv, argc, "IRIS", gEeslData,EESL_DATA_SIZE_EX);

    LOG_PRINTF(("EESL_Initialise"));

	// Wait for the task to activate first
	VMACLoop();
#endif

	// Perform an initial object check to guard against tamper...
	IRIS_StackInit(0);

	IRIS_StackPushFunc("()PPID");			// Make sure we have the keys injected already...
	if (IRIS_StackGet(0))
	{
		IRIS_StackPushFunc("()OBJECTS_CHECK");	// Called here instead of an object since all objects are not authentic yet..so we have to use the program
		IRIS_StackPush("51");					// The Secure Key location..Hardcoded for security reasons against tamper of "iRIS" object
		IRIS_StackPush(NULL);					// Check all objects
	}

	processObjectLoop();
}
