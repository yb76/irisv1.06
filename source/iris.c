//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iris.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports iris object resolutions
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
#include <svc.h>

//
// Local include files
//
#include "alloc.h"
#include "utility.h"
#include "irisfunc.h"
#include "security.h"
#include "iris.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//
#define	C_MAX_STACK			100
#define C_MAX_TEMP_DATA		500

#define	C_CONVERT_NO		0
#define	C_CONVERT_TO_ASCII	1
#define	C_CONVERT_TO_HEX	2

//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//
typedef struct
{
	uchar level;
	uchar func;
	char * value;
} T_STACK;

const char indexTrailer[] = "/INDEX";
const char countTrailer[] = "/COUNT";
char irisGroup[] = "iRIS";


static T_TEMPDATA tempData[C_MAX_TEMP_DATA];
static T_STACK * stack = NULL;
int stackIndex = -1;
uchar stackLevel = 0;

static bool arrayOfArraysFlag = false;
static int max_temp_data = 0;
static char * upload = NULL;

#ifdef _DEBUG
int dir = 0;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// IRIS FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_AppendToUpload
//
// DESCRIPTION:	Add an upload message to the next remote session
//
// PARAMETERS:	addition	<=	New data to add
//
// RETURNS:		None
//
//-----------------------------------------------------------------------------
//
void IRIS_AppendToUpload(char * addition)
{
	if (upload)
	{
		upload = my_realloc(upload, strlen(upload) + strlen(addition) + 1);
		strcat(upload, addition);
	}
	else
	{
		upload = my_malloc(strlen(addition) + 1);
		strcpy(upload, addition);
	}
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_PutObjectData
//
// DESCRIPTION:	Store the object data in the specified file
//
// PARAMETERS:	objectData	<=	The object data to store
//
// RETURNS:		None
//
//-----------------------------------------------------------------------------
//
void IRIS_PutNamedObjectData(char * objectData, uint length, char * name)
{
	FILE_HANDLE handle;

	handle = open(name, FH_NEW);
	write(handle, objectData, length);
	close(handle);
}

void IRIS_PutObjectData(char * objectData, uint length)
{
	char * name;

	name = IRIS_GetStringValue(objectData, length, "NAME", false);

	// Create the object within the terminal
	if (name && name[0] == 0)
		IRIS_PutNamedObjectData(objectData, length, &name[4]);

	// Deallocate the object value used
	IRIS_DeallocateStringValue(name);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_TemporaryObjectStringValue
//
// DESCRIPTION:	Manages temporary objects accessed so that multiple accesses to the same object
//				do not require a reload of data from the file system if it reference the
//				same object
//
// PARAMETERS:	stringName		<=	A fully qualified string name in the form /object/string
//
// RETURNS:		Temporary object data
//-------------------------------------------------------------------------------------------
//
static char * IRIS_TemporaryObjectStringValue(char * stringName, bool partial)
{
	static char * tempObject = NULL;
	static char * tempObjectData;
	static uint tempObjectLength;
	char * group;
	char * value;

	int j;
	int offset = 0;
	char objectName[50];

	// If this is a house cleaning operation, clear everything and exist
	if (stringName == NULL)
	{
		if (tempObject)
		{
			my_free(tempObject);
			my_free(tempObjectData);
			tempObject = NULL;
		}
		return NULL;
	}

	// Extract the object name on its own
	if (stringName[0] == '/' && stringName[1] == '/') offset = 1;
	for (j = offset; stringName[j+1] != '/' && stringName[j+1]; j++)
		objectName[j-offset] = stringName[j+1];
	objectName[j-offset] = '\0';

	// Release the temporary object if it is already allocated and it is not what we want now
	if (tempObject)
	{
		if (strcmp(objectName, tempObject))
		{
			my_free(tempObject), my_free(tempObjectData);
			tempObject = NULL;
		}
	}

	// If the temporary object is not allocated, allocate it to reference the object we want
	if (tempObject == NULL)
	{
		// Get the object
		if ((tempObjectData = IRIS_GetObjectData(objectName, &tempObjectLength)) == NULL)
			return NULL;
		UtilStrDup(&tempObject, objectName);
	}

	// Get the goup name of the object
	group = IRIS_GetStringValue(tempObjectData, tempObjectLength, "GROUP", false);

	// Do not allow access if the group string exists, and it is not a simple value or the group does not match the current group
	if (group && (group[0] != 0 || (group[4] != '\0' && strcmp(&group[4], currentObjectGroup) && strcmp(currentObjectGroup, irisGroup)) ))
		value = NULL;

	// Obtain the string value otherwise
	else
		value = IRIS_GetStringValue(tempObjectData, tempObjectLength, &stringName[j+2], partial);

	// Clean up
	IRIS_DeallocateStringValue(group);

	// Return the string value (if access granted)
	return value;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_GetExternalObjectData
//
// DESCRIPTION:	Look for an object in the internal table of objects
//				If found, returns the object data into a newly allocated memory
//
// PARAMETERS:	objectName	<=	The name of the file (object)
//				length		=>	Updated with the object length on exit
//
// RETURNS:		Pointer to an allocated memory structure representing file
//				object data in memory
//
//-----------------------------------------------------------------------------
//
void IRIS_GetExternalObjectData(char * objectName)
{
#ifdef _DEBUG
	FILE * handle;
#else
	int handle;
#endif
	char * ptr;
	char serialNo[20];
	char manufacturer[20];
	char model[20];

	char tx[1000];
	char * dataString;
	char * data;
	char iv[20];
	char proof[40];

	char comms_type[20];
	static int serial_connected = 0;
	int retry;
	char * myCurrentObjectGroup = currentObjectGroup;
	char temp[50];

	temp[0] = '\0';
	data = my_malloc(2000);
	currentObjectGroup = irisGroup;

	// Find out the connection type
	strcpy(&temp[4], "/IRIS_CFG/COMMS_TYPE");
	IRIS_ResolveToSingleValue(temp, false);
	if (IRIS_StackGet(0))
		strcpy(comms_type, IRIS_StackGet(0));
	else
		strcpy(comms_type, "IP");
	IRIS_StackPop(1);

	if (strcmp(comms_type, "SERIAL") == 0)
	{
		if (serial_connected == 0)
		{
			// IRIS_StackPush(NULL);	// Instead of the function call...
			IRIS_StackPush("1");		// Length header..Instead of the function call.
			strcpy(&temp[4], "/IRIS_CFG/SER_PORT");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/TIMEOUT");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/ITIMEOUT");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/SER_BAUD");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/SER_DATA");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/SER_PARITY");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/SER_STOP");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/BUFLEN");
			IRIS_ResolveToSingleValue(temp, false);

			__ser_connect();
			IRIS_StackPop(1);

			serial_connected = 1;
		}
	}
	else
	{
		// Establish a TCP connection
		for (retry = 0; retry < 2; retry++)
		{
			IRIS_StackPush(NULL);
	//		IRIS_StackPush("3");				// SSL + Length header
			IRIS_StackPush("1");				// Length header
			
			strcpy(&temp[4], "/IRIS_CFG/OIP");
			IRIS_ResolveToSingleValue(temp, false);
#ifdef __VX670
			IRIS_StackPush("R");
#else
			strcpy(&temp[4], "/IRIS_CFG/GW");
			IRIS_ResolveToSingleValue(temp, false);
#endif
			strcpy(&temp[4], "/IRIS_CFG/PDNS");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/SDNS");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/HIP");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/PORT");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/TIMEOUT");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/ITIMEOUT");
			IRIS_ResolveToSingleValue(temp, false);
			strcpy(&temp[4], "/IRIS_CFG/BUFLEN");
			IRIS_ResolveToSingleValue(temp, false);

			__tcp_connect();
			if (strcmp(IRIS_StackGet(0), "NOERROR") == 0)
			{
				IRIS_StackPop(1);
				break;
			}
			SVC_WAIT(1000);
			IRIS_StackPop(1);
		}

		if (retry == 2)
		{
			if (upload) UtilStrDup(&upload, NULL);
			currentObjectGroup = myCurrentObjectGroup;
			my_free(data);
			return;
		}
	}

restart:
	// Get the terminal configuration
	IRIS_StackPush(NULL);
	__serial_no();
	strcpy(serialNo, IRIS_StackGet(0));
	IRIS_StackPop(1);

	IRIS_StackPush(NULL);
	__manufacturer();
	strcpy(manufacturer, IRIS_StackGet(0));
	IRIS_StackPop(1);

	IRIS_StackPush(NULL);
	__model();
	strcpy(model, IRIS_StackGet(0));
	IRIS_StackPop(1);

	// Generate a random iv via a random key
	SecurityGenerateKey(irisGroup, 0, 8);
	SecuritySetIV(NULL);
	IRIS_StackPushFunc("()ENC");
	IRIS_StackPush("0");
	IRIS_StackPush("8");
	IRIS_StackPush("0123456789ABCDEF");
	strcpy(iv, IRIS_StackGet(0));
	IRIS_StackPop(1);

	// Encrypt the proof which is IV | signature
	SecuritySetIV(NULL);
	IRIS_StackPushFunc("()ENC");
	IRIS_StackPush("103");
	IRIS_StackPush("");
	sprintf(proof, "%s%s", iv, "CAFEBABEDEAFF001");
	IRIS_StackPush(proof);

	// Send KVC of KEK as well
	IRIS_StackPush(NULL);
	IRIS_StackPush("6");
	IRIS_StackPush("");
	__kvc();

	// Add the authentication object
	sprintf(tx, "{TYPE:IDENTITY,SERIALNUMBER:%s,MANUFACTURER:%s,MODEL:%s}{TYPE:AUTH,PROOF:%s,KVC:%s}", serialNo, manufacturer, model, IRIS_StackGet(1), IRIS_StackGet(0));
	IRIS_StackPop(2);

	// Add the event object and GETOBJECTs
	if (objectName)
		sprintf(data, "{TYPE:DISPLAY,NAME:%s,VERSION:%s,EVENT:%s,VALUE:%s,OBJECT:%s}", prevObject, currentObjectVersion, currentEvent, currentEventValue, objectName);
	else
		data[0] = '\0';

	handle = open("DEFAULT_T", FH_RDONLY);
	if (FH_ERR(handle))
		strcat(data, "{TYPE:GETOBJECT,NAME:DEFAULT_T}");
	else close(handle);

	handle = open("__MENU", FH_RDONLY);
	if (FH_ERR(handle))
		strcat(data, "{TYPE:GETOBJECT,NAME:__MENU}");
	else close(handle);

	// Add objects required for upload
	if (upload)
	{
		data = my_realloc(data, strlen(data) + strlen(upload) + strlen(tx));
		strcat(data, upload);
	}

	// Set IV
	IRIS_StackPush(NULL);
	IRIS_StackPush(iv);
	__iv_set();
	IRIS_StackPop(1);

	// OFB
	IRIS_StackPushFunc("()OFB");
	IRIS_StackPush("103");
	IRIS_StackPush("");
	dataString = my_malloc(strlen(data)*2 + 1);
	UtilHexToString((uchar *) data, strlen(data), dataString);
	IRIS_StackPush(dataString);
	my_free(dataString);

	// Set the transmit buffer
	data = my_realloc(data, strlen(tx) * 2 + strlen(IRIS_StackGet(0)) + 1);
	UtilHexToString((uchar *) tx, strlen(tx), data);
	strcat(data, IRIS_StackGet(0));
	IRIS_StackPop(1);

	// Send the data
	IRIS_StackPush(NULL);	
	IRIS_StackPush(data);
	if (strcmp(comms_type, "SERIAL") == 0)
	{
		strcpy(&temp[4], "/IRIS_CFG/SER_PORT");
		IRIS_ResolveToSingleValue(temp, false);
		__ser_send();
	}
	else
		__tcp_send();

	if (strcmp(IRIS_StackGet(0), "NOERROR"))
	{
		IRIS_StackPop(1);

		// Clean up
		if (upload) UtilStrDup(&upload, NULL);
		currentObjectGroup = myCurrentObjectGroup;
		my_free(data);
		return;
	}
	IRIS_StackPop(1);

	// Get the response
	IRIS_StackPush(NULL);
	strcpy(&temp[4], "/IRIS_CFG/TIMEOUT");
	IRIS_ResolveToSingleValue(temp, false);
	strcpy(&temp[4], "/IRIS_CFG/ITIMEOUT");
	IRIS_ResolveToSingleValue(temp, false);
	if (strcmp(comms_type, "SERIAL") == 0)
	{
		strcpy(&temp[4], "/IRIS_CFG/SER_PORT");
		IRIS_ResolveToSingleValue(temp, false);
		__ser_recv();
	}
	else
		__tcp_recv();
	ptr = IRIS_StackGet(0);

	// Examine the authenticate object
	strcpy(data, "{TYPE:AUTH,RESULT:");
	UtilHexToString((uchar *) data, strlen(data), tx);
	if (ptr && strncmp(ptr, tx, strlen(tx)) == 0)
	{
		// If we are granted access, get and store the objects received from the host
		strcpy(data, "YES GRANTED");
		UtilHexToString((uchar *) data, strlen(data), tx);
		if (strncmp(&ptr[36], tx, strlen(tx)) == 0)
		{
			unsigned char ofb = ptr[61];
			ptr += 62;

			// If OFB encrypted, decrypt first
			if (ofb == '1')
			{
				IRIS_StackPush(NULL);
				IRIS_StackPush(iv);
				__iv_set();
				IRIS_StackPop(1);

				IRIS_StackPushFunc("()OFB");
				IRIS_StackPush("101");
				IRIS_StackPush("");
				IRIS_StackPush(ptr);
				ptr = IRIS_StackGet(0);
			}

			// Store the objects
			IRIS_StackPushFunc("()STORE_OBJECTS");
			IRIS_StackPush("51");
			IRIS_StackPush(ptr);
			IRIS_StackPop(2);
			if (ofb == '1')
				IRIS_StackPop(1);

			IRIS_TemporaryObjectStringValue(NULL, false);
		}

		// If this is a new session, update the session keys...
		else
		{
			uchar key[65];

			strcpy(data, "NEW SESSION");
			UtilHexToString((uchar *) data, strlen(data), tx);
			if (strncmp(&ptr[36], tx, strlen(tx)) == 0)
			{
				ptr += 60;

				// OWF KEK
				SecurityOWFKey(irisGroup, 6, 16, 6, 50, false);

				// If we have a new KEK...
				strcpy(data, "KEK:");
				UtilHexToString((uchar *) data, strlen(data), tx);
				if (strncmp(ptr, tx, strlen(tx)) == 0)
				{
					memcpy(key, &ptr[8], 64);
					key[64] = '\0';

					UtilStringToHex((char *) key, 64, (uchar *) tx); tx[32] = '\0';
					UtilStringToHex(tx, 32, key);

					SecurityInjectKey(irisGroup, 4, 16, irisGroup, 6, 16, key, "\x82\xC0");
					ptr += 74;
				}

				// If we have a new PPASN...
				strcpy(data, "PPASN:");
				UtilHexToString((uchar *) data, strlen(data), tx);
				if (strncmp(ptr, tx, strlen(tx)) == 0)
				{
					memcpy(key, &ptr[12], 32);
					key[32] = '\0';

					UtilStringToHex((char *) key, 32, (uchar *) tx); tx[16] = '\0';
					UtilStringToHex(tx, 16, key);

					SecurityInjectKey(irisGroup, 4, 16, irisGroup, 50, 8, key, "\x88\x88");
					ptr += 46;

					// OWF Variant of KEK
					SecurityOWFKey(irisGroup, 6, 16, 6, 50, true);
				}

				// If we have a new MKs...
				strcpy(data, "MKs:");
				UtilHexToString((uchar *) data, strlen(data), tx);
				if (strncmp(ptr, tx, strlen(tx)) == 0)
				{
					memcpy(key, &ptr[8], 64);
					key[64] = '\0';

					UtilStringToHex((char *) key, 64, (uchar *) tx); tx[32] = '\0';
					UtilStringToHex(tx, 32, key);

					SecurityInjectKey(irisGroup, 6, 16, irisGroup, 101, 16, key, "\x22\xC0");
					ptr += 74;
				}

				// If we have a new MKr...
				strcpy(data, "MKr:");
				UtilHexToString((uchar *) data, strlen(data), tx);
				if (strncmp(ptr, tx, strlen(tx)) == 0)
				{
					memcpy(key, &ptr[8], 64);
					key[64] = '\0';

					UtilStringToHex((char *) key, 64, (uchar *) tx); tx[32] = '\0';
					UtilStringToHex(tx, 32, key);

					SecurityInjectKey(irisGroup, 6, 16, irisGroup, 103, 16, key, "\x44\xC0");
				}

				IRIS_StackPop(1);
				goto restart;
			}
		}
	}

	// Disconnect
	if (strcmp(comms_type, "SERIAL"))
	{
		IRIS_StackPushFunc("()TCP_DISCONNECT");
		IRIS_StackPop(1);
	}

	// Clean up
	if (upload) UtilStrDup(&upload, NULL);
	currentObjectGroup = myCurrentObjectGroup;
	my_free(data);
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_GetInternalObjectData
//
// DESCRIPTION:	Look for an object in the internal table of objects
//				If found, returns the object data into a newly allocated memory
//
// PARAMETERS:	objectName	<=	The name of the file (object)
//				length		=>	Updated with the object length on exit
//
// RETURNS:		Pointer to an allocated memory structure representing file
//				object data in memory
//
//-----------------------------------------------------------------------------
//
static char * IRIS_GetInternalObjectData(char * objectName, unsigned int * length)
{
	#include "internal.table"

	int i;
	char * retString = NULL;

	// If not found externally and it is the current DISPLAY object, then try the default internal ones
	for (i = 0; internalObjects[i].object; i++)
	{
		if (strcmp(objectName, internalObjects[i].name) == 0)
		{
			if (length == NULL) return (char *) 1;

			if (objectName == currentObject)
				UtilStrDup(&currentObject, internalObjects[i].name);
			*length = strlen(internalObjects[i].object);
			return UtilStrDup(&retString, (char *) internalObjects[i].object);
		}
	}

	return NULL;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_GetObjectData
//
// DESCRIPTION:	Look for an object in the file system.
//				If found, read the object into an allocated memory
//
// PARAMETERS:	objectName	<=	The name of the file (object)
//				length		=>	Updated with the object length on exit
//
// RETURNS:		Pointer to an allocated memory structure representing file
//				object data in memory
//
//-----------------------------------------------------------------------------
//
char * IRIS_GetObjectData(char * objectName, unsigned int * length)
{
	char temp = 0;
	char * data;
	FILE_HANDLE handle;

	if (!objectName || !length) return NULL;

	// Open the file
	handle = open(objectName, FH_RDONLY);
	if (FH_ERR(handle))
	{
		if ((data = IRIS_GetInternalObjectData(objectName, length)) != NULL)
			return data;
		if (objectName == currentObject)
			IRIS_GetExternalObjectData(objectName);
		handle = open(objectName, FH_RDONLY);
		if (FH_ERR(handle))
			return NULL;
	}

	// Do a simple check first
	read(handle, &temp, 1);
	if (temp != '{') {
		close(handle);
		return NULL;
	}

	// Read the entire object
	*length = lseek(handle, 0, SEEK_END);
#ifdef _DEBUG
	*length = ftell(handle);
#endif
	lseek(handle, 0, SEEK_SET);

	data = my_malloc((*length)+1);
	read(handle, data, *length);
	data[*length] = '\0';

	// House keep and finish
	close(handle);

	// Just in case there are added data towards the end durign inserts and deletion operations....
	*length = strlen(data);

	return data;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_DownloadResourceObject
//
// DESCRIPTION:	Look for a resource object (ie object needed by another object) in the file system.
//				If found returns OK. If not, contacts the remote host and grabs it
//
// PARAMETERS:	objectName	<=	The name of the file (resource object)
//
// RETURNS:		TRUE if available or FALSE if not
//
//-----------------------------------------------------------------------------
//
bool IRIS_DownloadResourceObject(char * objectName)
{
	FILE_HANDLE handle;

	if (!objectName) return false;

	handle = open(objectName, FH_RDONLY);

	if (FH_ERR(handle))
	{
		if (IRIS_GetInternalObjectData(objectName, NULL) != NULL)
			return true;

		if (objectName == currentObject)
		{
			IRIS_GetExternalObjectData(objectName);
			handle = open(objectName, FH_RDONLY);
		}
	}

	if (FH_ERR(handle))
	{
		if (objectName != currentObject)
		{
			char * myPrevObject = prevObject;
			prevObject = currentObject;
			IRIS_GetExternalObjectData(objectName);
			prevObject = myPrevObject;

			handle = open(objectName, FH_RDONLY);
		}

		if (FH_ERR(handle))
			return false;
	}

	close(handle);
	return true;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_GetTypeObjectData
//
// DESCRIPTION:	Look for an object in the file system with TYPE = type
//
// PARAMETERS:	count <= 
//						=>	Updated with the object length on exit
//
// RETURNS:		Pointer to an allocated memory structure representing file
//				object data in memory
//
//-----------------------------------------------------------------------------
//
#ifdef _DEBUG
int dir_get_first(char * filename)
{
	dir = 0;
//	strcpy(filename, "SGB");
	strcpy(filename, "iPAY");
	return 0;
}

int dir_get_next(char * filename)
{
	if (dir == 0)
	{
		strcpy(filename, "iECR");
		dir++;
		return 0;
	}
	else if (dir == 1)
	{
		strcpy(filename, "iSCAN");
		dir++;
		return 0;
	}
	else if (dir == 2)
	{
		strcpy(filename, "iVSTOCK");
		dir++;
		return 0;
	}

	return 1;
}

#endif

char * IRIS_GetTypeObjectData(uint count, uint * length, char * type)
{
	int i, j;
	char fileName[50];
	char * data;
	char search[100];
	char * ptr;
	uint myCount = count;

	// Prepare the type search buffer
	sprintf(search, "TYPE:%s", type);

	for (j = 0; count && j < 2; j++, count = myCount)
	{
		strcpy(fileName, j?"I:":"F:");
		for (i = dir_get_first(fileName); i == 0; i = dir_get_next(fileName))
		{
			if ((data = IRIS_GetObjectData(fileName, length)) != NULL)
			{
				if (data[0] == '{' && data[*length-1] == '}' && (ptr = strstr(data, search)) != NULL && (ptr[-1] == ',' || ptr[-1] == '{'))
				{
					if (--count == 0) return data;
				}
			}

			my_free(data);
		}
	}


	return NULL;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_GetStringValue
//
// DESCRIPTION:	Look inside the object passed in 'data' for an string with the name
//				'name' and return its data stored within an allocated memory block
//				for easy parsing.
////
// PARAMETERS:	data	<=	The main object
//				name	<=	The 'string' part of the object.
//
// RETURNS:		Pointer to an allocated memory structure representing the 'value'
//				of the object in question.
//
//-----------------------------------------------------------------------------
//
char * IRIS_GetStringValue(char * data, int size, char * name, bool partial)
{
	uchar search[4000];
	int i, j, k;
	int length = 0;
	int count[20];	// Assuming a maximum of 20 nested levels...I think this is more than enough
	int level = 0;
	char * value;
	char * ptr;
	int valueStart;
	int sqwigly;
	int square;
	int position = -1000;

	// If this is a position search, then find the required position
	if (name[0] >= '0' && name[0] <= '9')
		position = atol(name);

	// Search for the start of the value
	for (i = 0, sqwigly = -1, square = 0; i < size; i++)
	{
		// If we are looking for a sqwigly bracket, it must be the next character in the object buffer or we have an OBJECT NOT FOUND.
		if (sqwigly == -1 && data[i] != '{')
			return NULL;

		// Any objects found within an array needs to be ignore completely at this stage...
		// If we change our minds later to find objects as part of an array, then this needs
		// modification...
		if (data[i] == '[')
			square++;
		else if (data[i] == ']')
			square--;
		else if (square == 0)
		{
			// If we find a { without finding the string of the object, ignore everything
			// until the closing sqwigly bracket even if the same string name appears within this
			if (data[i] == '{')
				sqwigly++;

			// If we find a closing sqwigly bracket, then nullify the effect of the corresponding open sqwigly bracket
			else if (data[i] == '}')
			{
				// If this is the last of the object, return an OBJECT NOT FOUND.
				if (--sqwigly < 0)
					return NULL;
			}

			// If we are locating the start of the object, then.
			if (sqwigly == 0 && (data[i] == '{' || data[i] == ','))
			{
				int back = 0;
				k = 0;

				// Make sure we have detected a matching name...
				if (position == -1000)
				{
					for (;	(partial == false && name[k] == data[i+k+1]) ||
							(partial == true && back == 0 && name[k+back] == data[i+k+1]) ||
							(partial == true && back && data[i+k+1] != ':'); k++)
						if (partial == true && name[k+back] == '\0' && back > -50) back--;
				}
				else if (position-- == 0)
				{
					for (; data[i+k+1] != ':' && k < 64 && data[i+k+1]; k++)
						name[k] = data[i+k+1];
					name[k] = '\0';

				}

				// ... and also detect the ':" immediately after the name
				if (data[i+k+1] == ':')
				{
					// If we find it stop
					if (name[k+back] == '\0')
					{
						i += (k + 2);
						break;
					}
				}
			}
		}
	}

	// If the object is not found, return NULL
	if (i == size)
		return NULL;

	// Find amount of memory needed to store the value in a quickly accessible location
	for (memset(count, 0, sizeof(count)), valueStart = i, sqwigly = 0, j = 0; i < size; i++)
	{
		// Treat inner objects as part of the element data and ignore arrays and commas within it (if sqwigly is set)...
		// If we find the beginning of an array, store an indication and increment the level
		if (sqwigly == 0 && (valueStart == i || data[i-1] == '[' || data[i-1] == ',') && data[i] == '[')
		{
			search[j++] = '[';
			square++;
			level++;
		}

		// If we find comma or closing bracket then capture the length of the current element
		else if (sqwigly == 0 && (data[i] == ',' || (square && data[i] == ']')))
		{
			// Store the length of the element
			search[j++] = (length+5) >> 8 | 0x80;	// Setting the top bit ensures we do not conflict with commas and brackets
			search[j++] = ((length+5) & 0xFF);		// Adding FIVE to the length allows elements to be stored later as strings ending with a NULL,
													// and also allows for the type of the element (4 bytes for allignment) to be stored in the beginning simplifying deallocation
			length = 0;

			// If a closing bracket///
			if (data[i] == ']')
			{
				search[j++] = ']';
				square--;
				length = (count[level]+2) * sizeof(char *) - 1;	//Include the last element and an end of list null pointer.
				count[level--] = 0;
				if (level == 0)
				{
					// Store the length of the element
					search[j++] = (length+5) >> 8 | 0x80;	// Setting the top bit ensures we do not conflict with commas and brackets
					search[j++] = ((length+5) & 0xFF);		// Adding TWO to the length allows elements to be stored later as strings ending with a NULL,
															// and also allows for the type of the element  (4 bytes for allignment) to be stored in the beginning simplifying deallocation
					length = 0;
				}
			}

			// else, make sure we capture the location of the comma
			else if (data[i] == ',')
			{
				if (level == 0)
				{
					i--;
					break;
				}
				count[level]++;
				search[j++] = ',';
			}

			// If the level is ZERO, then this is the end of the entire object value
			if (level == 0) break;
		}

		// If this is the end of the object, do not count it
		// Otherwise, keep counting the length of the current element
		else if (data[i] != '}' || sqwigly)
		{
			// Count down embedded objects as we find the closing sqwigly brackets
			if (data[i] == '}') sqwigly--;
			// If an internal object is detected, make sure we also count the closing sqwigly bracket
			else if (data[i] == '{') sqwigly++;

			length++;
		}
	}

	// If the element is not an array, then just allocate a single buffer for the element
	if (length)
	{
		i -= 2;									// Lose the NULL character at the end of the string and the } or , after the element value
		search[j++] = (length+5) >> 8 | 0x80;	// Setting the top bit ensures we do not conflict with commas and brackets
		search[j++] = ((length+5) & 0xFF);		// Adding FIVE to the length allows elements to be stored later as strings ending with a NULL,
	}
	else if (j == 0)
	{
		search[j++] = 0x80;
		search[j++] = 0x05;
	}

	// Allocate memory for each array and element in the value list
	for (memset(count, 0, sizeof(count)), level = 0, --j; j >= 0; j--)
	{
		if ((j > 0 && (search[j-1] & 0x80)) &&
			(j < 2 || (search[j-2] & 0x80) == 0))
		{
			count[level] = (search[j-1] & 0x7F) * 256 + search[j--];
			if (level == 0)
				ptr = value = my_calloc(count[level] < 5? 5:count[level]);	// This also ensures it is all initialised properl. Important if we need a NULL pointer at the end.
			else
			{
				for (k = 0, ptr = (char *) &value; k < level; k++)
				{
					ptr = *((char **) ptr);
					ptr[0] = 1;	// This indicates that this is an array
					ptr = &ptr[count[k]-8];
				}
				*((char **) ptr) = my_calloc(count[level]);
				ptr = *((char **) ptr);
			}
		}
		else if (search[j] == ']' && data[i] == ']')
		{
			level++;
			i--;
		}
		else if (search[j] == '[' || search[j] == ',')
		{
			if (data[i] != ',' || data[i+1] != '[' || data[i] != search[j])
			{
				for (sqwigly = 0;sqwigly || data[i] != search[j]; i--, count[level]--)
				{
					if (data[i] == '}') sqwigly++;
					else if (data[i] == '{') sqwigly--;
					ptr[count[level]-2] = data[i];
				}
				if (search[j] == '[')
					--level;
				if (level > 0) count[level-1] -= sizeof(char *);
			}
			i--;
		}
	}

	// If the element is not an array, then just store the value
	for (;i >= valueStart; i--, count[level]--)
		ptr[count[level]-2] = data[i];
	
	// Return the value data pointer
	return value;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : IRIS_DeallocateStringValue
//
// DESCRIPTION:	Deallocate the string value completely including its nested arrays
//
//				Examine the buffer pointer to by value and by nested pointers of value...
//				If the buffer ends with a null pointer (ie FOUR zeros), then this is
//				an array of pointers, so traverse one level down first. If not, just
//				deallocate and go back
//
//				THIS IS A RECURSIVE FUNCTION
//
// PARAMETERS:	value	<=	The array or element to my_free
//
// RETURNS:		None
//
//-----------------------------------------------------------------------------
//
void IRIS_DeallocateStringValue(char * value)
{
	// Make sure value is already allocated and not NULL
	if (value == NULL)
		return;

	// If this is a simple element, my_free it
	if (value[0] == 0)
		my_free(value);

	// If this is an array, pick up the array pointer lists one by one and my_free them
	else if (value[0] == 1)
	{
		int i;

		// Deallocate each element within the array
		for (i = 4; *((char **) &value[i]) != NULL; i += sizeof(char *))
			IRIS_DeallocateStringValue(*((char **) &value[i]));

		// Deallocate the array itself
		my_free(value);
	}
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_StackPush
//
// DESCRIPTION:	Push a value onto the stack.
//				If a function finds sufficient parameters and the level is the same
//				as the current stack level, the function is excuted. The function is 
//				responsible for clearing the stack and puching its result back in.
//
// PARAMETERS:	value	<=	A simple value
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void IRIS_StackPush(char * value)
{
	int i;

	if (stack == NULL) return;
	
	UtilStrDup(&stack[++stackIndex].value, value);
	stack[stackIndex].level = stackLevel;
	if (!value || value[0] != '(' || value[1] != ')') stack[stackIndex].func = 255;

	// Look for a function at the same level and resolve its value if available
	for (i = stackIndex; i >= 0 && stack[i].level == stackLevel; i--)
	{
		if (stack[i].func != 255)
		{
			if (irisFunc[stack[i].func].paramCount == (stackIndex-i))
				irisFunc[stack[i].func].funcPtr();
			break;
		}
	}
}

bool IRIS_StackPushFunc(char * function)
{
	int i;

	if (stack == NULL) return false;
	
	for (i = 0; i < C_NO_OF_IRIS_FUNCTIONS; i++)
	{
		if (strcmp(irisFunc[i].name, function) == 0)
		{
			if (irisFunc[i].paramCount && irisFunc[i].array)
				arrayOfArraysFlag = true;

			stack[stackIndex+1].func = i;
			IRIS_StackPush(function);
			return true;
		}
	}

	return false;
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_StackPop
//
// DESCRIPTION:	Pop a value out from the stack.
//
// PARAMETERS:	count	<=	Number of values to pop from the stack
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void IRIS_StackPop(int count)
{
	while(count--)
	{
		if (stackIndex >= 0)
		{
			if (stack[stackIndex].value)
				UtilStrDup(&stack[stackIndex].value, NULL);
			stackIndex--;
		}
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_StackGet
//
// DESCRIPTION:	Read a value from the stack.
//
// PARAMETERS:	offset	<=	How deep in the stack should we go from the top.
//
// RETURNS:		The value
//-------------------------------------------------------------------------------------------
//
char * IRIS_StackGet(char offset)
{
	if (stack && stackIndex >= offset)
		return stack[stackIndex-offset].value;
	return NULL;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_StackFlush
//
// DESCRIPTION:	Frees the stack if already available
//
// PARAMETERS:	None
//
// RETURNS:		The value
//-------------------------------------------------------------------------------------------
//
void IRIS_StackFlush(void)
{
	int i;

	for (i = 0; stack && i < (stackIndex+1); i++)
		if (stack[i].value) my_free(stack[i].value);

	if (stack) my_free(stack);
	stack = NULL;
	stackIndex = -1;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_StackInit
//
// DESCRIPTION:	Frees the stack if already available, then reallocates its memory
//
// PARAMETERS:	If not zero, it overrides the size of the stack. If zero, the size = C_MAX_STACK
//
// RETURNS:		The value
//-------------------------------------------------------------------------------------------
//
void IRIS_StackInit(int size)
{
	if (stack) IRIS_StackFlush();
	stack = my_calloc(sizeof(T_STACK) * (size?size:C_MAX_STACK));
	stackIndex = -1;
	stackLevel = 0;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_StackInitialised
//
// DESCRIPTION:	Checks if the stack has been initialised
//
// PARAMETERS:	NONE
//
// RETURNS:		Boolean
//-------------------------------------------------------------------------------------------
//
bool IRIS_StackInitialised(void)
{
	if (stack)
		return true;

	return false;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_ResolveToSingleValue
//
// DESCRIPTION:	Examines an object string and recursively evaluates its value to end up with a simple value
//
// PARAMETERS:	value		<=	A complex or simple value of the string
//
// RETURNS:		A simple string or "byte array" value
//-------------------------------------------------------------------------------------------
//


//
// Splits a resolvable simple value into a substring format string
// and a stripped string.
//
static void ____getSubString(char * rsimple, char * simpler)
{
	int i;
	char format[15];

	// Initialise
	i = strlen(rsimple);
	format[0] = '\0';

	// Look at the resolvable simple value for an substring marker
	if (rsimple[i-1] == '|')
	{
		// Look for the beginning of the substring marker
		for (i -= 2; i; i--)
		{
			if (rsimple[i] == '|')
			{
				// Look for qualifiers preceding the first substring marker
				if (rsimple[i-2] == '>' || rsimple[i-2] == '<')
					i -= 2;
				else if (rsimple[i-1] == '>')
					i--;

				// Get the format
				strcpy(format, &rsimple[i]);
				IRIS_StackPushFunc("()SUBSTRING");
				IRIS_StackPush(format);
				break;
			}
		}
	}

	// Get the stripped resolvable simple value
	sprintf(simpler, "%.*s", i, rsimple);
}

//
// Converts the resolvable simple value from an array to a non-array
// using its index. If no index found, the index is created as a temporary
// data element
//
static void ____toNonArray(char * rsimple)
{
	int i, j, k;
	char nonArray[100];
	char index[100];
	bool array = false;
	int indexNum = 0;

	// Initialisation
	memset(nonArray, 0, sizeof(nonArray));

	for (i = 0, j = 0; i < (int) strlen(rsimple); i++, j++)
	{
		if (rsimple[i] == '(')
		{
			int m = 0;
			char * ptr = NULL;

			// Find the closing bracket
			for (k = 0; rsimple[i+k+1]; k++)
			{
				if (rsimple[i+k+1] == ')')
				{
					if (m) m--;
					else
					{
						ptr = &rsimple[i+k+1];
						break;
					}
				}
				else if (rsimple[i+k+1] == '(')
					m++;
			}

			// It is an array
			array = true;

			if (rsimple[i+1] != ')' && ptr)
			{
				int len = ptr-&rsimple[i+1];
				int myStackIndex = stackIndex;
				char temp[100];

				memcpy(temp, &rsimple[i+1], len);
				temp[len] = '\0';
				i += len;

				stackLevel++;
				IRIS_Eval(temp, false);
				stackLevel--;

				if (stackIndex != myStackIndex)
				{
					if (IRIS_StackGet(0))
						indexNum = atol(IRIS_StackGet(0));
					else
						indexNum = 0;
					IRIS_StackPop(stackIndex - myStackIndex);
				}
			}
		}
		else if (rsimple[i] == ')')
		{
			array = false;
			if (rsimple[i-1] == '(')
			{
				sprintf(index, "%s%s", (nonArray[0] == '/' && nonArray[1] == '/')?&nonArray[1]:nonArray, indexTrailer);

				// If it is found in the data array, get it's value
				for (k = 0; k < max_temp_data; k++)
				{
					if (tempData[k].name && strcmp(tempData[k].name, index) == 0 && (tempData[k].group[0] == '\0' || strcmp(tempData[k].group, currentObjectGroup) == 0 || strcmp(currentObjectGroup, irisGroup) == 0))
					{
						indexNum = atol(tempData[k].value);
						break;
					}
				}

				// If not, set the internal index but only if if is not explicitly addressed
				if (k == max_temp_data)
					indexNum = 0;
			}

			// Adjust the non array buffer
			if (indexNum != -1)
			{
				sprintf(index, "%s%d", nonArray, indexNum);
				strcpy(nonArray, index);
			}
			j = strlen(nonArray) - 1;
		}
		else if (array == false)
			nonArray[j] = rsimple[i];
	}

	strcpy(rsimple, nonArray);
}

void IRIS_ResolveToSingleValue(char * value, bool partial)
{
	// Handle errors. A value and a stack must be available first
	if (value == NULL || stack == NULL) return;

	// If it is not an array
	if (value[0] == 0)
	{
		int i;
		char * simple = &value[4];

		// If a conversion is requested first, remember that
		if (simple[0] == '\\')
		{
			if (simple[1] == 'a')
			{
				IRIS_StackPushFunc("()TO_AHEX");
				simple += 2;
			}
			else if (simple[1] == 'h')
			{
				IRIS_StackPushFunc("()TO_HEX");
				simple += 2;
			}
		}

		// If it is a function
		if (simple[0] == '(' && simple[1] == ')')
		{
			if (IRIS_StackPushFunc(simple))
				return;
		}

		// If it starts with a '@' then it is a reference to an object TYPE string in the format @type/string
		if (simple[0] == '@')
		{
			int j;
			char temp[100];
			char * typeObjectData;
			char * tempStringData;
			uint length;

			// Create a temporary index string
			for (j = 0; j < (int) strlen(simple) && simple[j] != '/'; j++)
				temp[j] = simple[j];
			strcpy(&temp[j], indexTrailer);

			// Look for the index in the temporary data array area
			for (i = 0;  i < max_temp_data; i++)
			{
				// If we find it, stop
				if (tempData[i].name && strcmp(tempData[i].name, temp) == 0)
				{
					// If we find it and the caller is requesting only the index, push the index value onto the stack
					if (strstr(simple, indexTrailer))
					{
						if (tempData[i].group[0] == '\0' || strcmp(tempData[i].group, currentObjectGroup) == 0 || strcmp(currentObjectGroup, irisGroup) == 0)
							IRIS_Eval(tempData[i].value, false);
						else
							IRIS_StackPush(NULL);
						return;
					}
					break;
				}
			}

			// If the index is not found, assume its value = 0;
			if (i == max_temp_data) i = 0;
			else i = (tempData[i].value?atol(tempData[i].value):0);

			// Prepare the object name on its own then attempt to get the object data
			temp[j] = '\0';

			// If we have the object data, then get the requested string value
			if ((typeObjectData = IRIS_GetTypeObjectData(i+1, &length, &temp[1])) != NULL)
			{
				if ((tempStringData = IRIS_GetStringValue(typeObjectData, length, &simple[j+1], partial)) != NULL)
				{
					IRIS_ResolveToSingleValue(tempStringData, false);
					IRIS_DeallocateStringValue(tempStringData);
				}
				else IRIS_StackPush(NULL);
				my_free(typeObjectData);
			}
			else IRIS_StackPush(NULL);
		}

		// If it starts with a '~' then it references another string within the same object data in the form ~string. Extract it.
		else if (simple[0] == '~' || simple[0] == '/')
		{
			char simpler[50];
			char fullName[100];
			char * tempStringData = NULL;

			// Extract any substring information for calculation later
			____getSubString(simple, simpler);

			// Obtain the full name and resolve array subscripts
			IRIS_FullName(simpler, fullName);

			// If this is an object name request, then just return the object name itself as the value
			if (fullName[1] == '/' && strchr(&fullName[2], '/') == NULL)
			{
				IRIS_StackPush(&fullName[1]);
				return;
			}

			// If this is an object name request with a single '/', then just return the object name itself as the value but only if it exists...
			if (strchr(&fullName[1], '/') == NULL)
			{
				if (fullName[1])
				{
					FILE_HANDLE handle;
					handle = open(&fullName[1], FH_RDONLY);
					if (FH_ERR(handle))
					{
						IRIS_StackPush(NULL);
						return;
					}
					else
						close(handle);
				}

				IRIS_StackPush(fullName);
				return;
			}

			// If this is the count variable, get the count instead
			if (strstr(fullName, countTrailer))
			{
				char temp[10];
				IRIS_StackPush(ltoa(IRIS_GetCount(fullName), temp, 10));
				return;
			}

			// If it is found in the data array, return it.
			for (i = 0; i < max_temp_data; i++)
			{
				if ((partial == false && tempData[i].name && strcmp(tempData[i].name, fullName) == 0) ||
					(partial == true && tempData[i].name && strncmp(tempData[i].name, fullName, strlen(fullName)) == 0))
				{
					if (tempData[i].group[0] == '\0' || strcmp(tempData[i].group, currentObjectGroup) == 0 || strcmp(currentObjectGroup, irisGroup) == 0)
						IRIS_Eval(tempData[i].value, false);
					else
						IRIS_StackPush(NULL);
					return;
				}
			}

			// If it is not data, then possibly a string within the current object OR a different object. Resolve that if found.
			if ((simple[0] == '~' && (tempStringData = IRIS_GetStringValue(currentObjectData, currentObjectLength, &simple[1], partial)) != NULL) ||
				(tempStringData = IRIS_TemporaryObjectStringValue(fullName, partial)) != NULL)
					IRIS_ResolveToSingleValue(tempStringData, false);
			else
				IRIS_StackPush(NULL);

			IRIS_DeallocateStringValue(tempStringData);
		}


		// If it is a number or string, return it.
		else
			IRIS_StackPush(simple);
	}

	// If it is an array then
	else
	{
		int i, j;
		char ** array1 = (char **) &value[4];

		// Advance the stack level
		stackLevel++;

		// If the first element is not an array, then it is an implicit concatenation
		if ((*array1)[0] == 0)
		{
			int myStackIndex = stackIndex;
			char * singleValue = NULL;
			int size = 0;

			// Resolve each value within the array in order to concatenate them later
			for (; *array1; array1++)
				IRIS_ResolveToSingleValue(*array1, false);

			// Concatenate all values stored in the stack
			for (i = myStackIndex+1; i <= stackIndex; i++)
			{
				if (stack[i].value)
				{
					size = strlen(stack[i].value) + (singleValue?strlen(singleValue):0) + 1;
					if (singleValue)
						singleValue	= my_realloc(singleValue, size);
					else
						singleValue = my_calloc(size);
					strcat(singleValue, stack[i].value);
				}
			}

			// Pop all the values concatenated
			IRIS_StackPop(stackIndex-myStackIndex);

			// Push the newly concatenated value onto the stack
			stackLevel--;
			IRIS_StackPush(singleValue);
//			IRIS_Eval(singleValue, false);	// The idea here is for some variablesx to hold part of the evaluation string and then bringing them together will get another. Re-visit later. "{,,,},{,,,}" did not work. Only picked up the first object.
			if (singleValue) my_free(singleValue);
		}

		// If the first value is another array, then this is a CONDITION (special array of arrays!!):
		// DO NOT CONFUSE THIS WITH PATH ARRAY OF ARRAYS AS THIS IS EVALUATED SPERATELY AND INCLUDES EVENTS....HOWEVER,
		// ELEMENTS WITHIN THE PATH WILL BE RESOLVED HERE IF NEEDED...
		else
		{
			bool keepGoing = true;

			// If a function requested this array of arrays, then store it globally and push a value onto the stack to force a function call.
			// This array of arrays must be the last parameter requested by the function or this will not work properly.
			if (arrayOfArraysFlag)
			{
				char location[20];

				arrayOfArraysFlag = false;
				sprintf(location, "%ld", array1);
				stackLevel--;
				IRIS_StackPush("POINTER");
				IRIS_StackPush(location);
				return;
			}

			// For each array within the main array
			for (;keepGoing && *array1; array1++)
			{
				char ** array2 = (char **) &((*array1)[4]);
				char * first = NULL, * second = NULL;

				// Remember the beginning of the stack and set the operator to equality (default)
				int myStackIndex = stackIndex;
				char operator = '=';

				// If any of subsequent values within the array is not an array, then resolve it and do not process any further...This is the default if
				// all prior conditions fail
				if ((*array1)[0] == 0)
				{
					stackLevel--;
					IRIS_ResolveToSingleValue(*array1, false);
					return;
				}

				// For the first 4 values within each conditional array
				for (i = 0; i < 4 && *array2; array2++, i++)
				{
					char ** array3 = (char **) &((*array2)[4]);
					char * value = &(*array2)[4];
					uchar * firstHex, * secondHex;
					int size;
					bool equal = false;

					switch(i)
					{
						// The first value is the first comparison value
						case 0:
							// Resolve it to a simple value as required
							IRIS_ResolveToSingleValue(*array2, false);

							// If not available, then set it to null. Otherwise pop it from the stack
							UtilStrDup(&first, IRIS_StackGet(0));
							IRIS_StackPop(stackIndex - myStackIndex);
							break;

						// The second value is the second comparison value but possibly preceeded with an operator
						case 1:
							do
							{
								long num1, num2;
								char ** resolve = array2;

								if ((*array2)[0] == 1)
								{
									if (*array3)
									{
										resolve = array3;
										value = &(*array3)[4];
										array3++;
									}
									else
									{
										i = 4;
										break;
									}
								}

								if (value[0] == '|' || value[0] == '&' || value[0] == '!' || value[0] == '^' || value[0] == '=' || value[0] == '>' || value[0] == '<')
								{
									operator = value[0];
									IRIS_Eval(value + 1, false);
								}
								else
									IRIS_ResolveToSingleValue(*resolve, false);

								UtilStrDup(&second, IRIS_StackGet(0));
								IRIS_StackPop(stackIndex - myStackIndex);

								if (second && strcmp(second, "NULL") == 0)
									UtilStrDup(&second, NULL);

								num1 = UtilStringToNumber(first);
								num2 = UtilStringToNumber(second);
#ifdef _DEBUG
								printf("******** Comparing %s %c %s *******\n", first?first:"NULL", operator, second?second:"NULL");
#endif

								switch(operator)
								{
									case '=':
										if (num1 != -1 && num2 != -1)
										{
											if (num1 != num2)
											{
												if ((*array2)[0] == 0)
													i = 4;
											}
											else equal = true;
										}
										else if ((first && !second) || (!first && second) || (first && second && strncmp(first, second, strlen(second))))
										{
											if ((*array2)[0] == 0)
												i = 4;
										}
										else equal = true;
										break;
									case '!':
										if (num1 != -1 && num2 != -1)
										{
											if (num1 == num2) i = 4;
										}
										else if ((!first && !second) || (first && second && strncmp(first, second, strlen(second)) == 0)) i = 4;
										break;
									case '<':
										if (num1 != -1 && num2 != -1)
										{
											if (num1 >= num2) i = 4;
										}
										else if (!first || !second || strncmp(first, second, strlen(second)) >= 0) i = 4;
										break;
									case '>':
										if (num1 != -1 && num2 != -1)
										{
											if (num1 <= num2) i = 4;
										}
										else if (!first || !second || strncmp(first, second, strlen(second)) <= 0) i = 4;
										break;
									case '^':
									case '&':
									case '|':
										// If ASCII hex digits are used here which is expected, then this will convert them back to normal hex character.
										// Convert to Hex first using maximum number of digits
										if (!first || !second)
										{
											i = 4;
											break;
										}

										size = strlen(first);
										if (strlen(second) > (uint) size) size = strlen(second);
										firstHex = my_malloc(size / 2 + 1);
										secondHex = my_malloc(size / 2 + 1);
										UtilStringToHex(first, size, firstHex);
										size = UtilStringToHex(second, size, secondHex);

										// Result is TRUE if a non zero result with any hex byte
										for (j = 0; j < size; j++)
										{
											if (value[0] == '^' && (firstHex[j] ^ secondHex[j])) break;
											if (value[0] == '&' && (firstHex[j] & secondHex[j])) break;
											if (value[0] == '|' && (firstHex[j] | secondHex[j])) break;
										}
										if (j == size) i = 4;
										my_free(secondHex);
										my_free(firstHex);
										break;
								}

								UtilStrDup(&second, NULL);

							} while((*array2)[0] == 1 && equal == false);

							UtilStrDup(&first, NULL);
							break;

						// The third value is an action object....Process an object if we reach here.
						case 2:
							IRIS_ProcessActionObject(value);
							break;

						// The fourth is the value to push onto the stack. If found quit further comparisons
						case 3:
							IRIS_ResolveToSingleValue(*array2, false);
							if (IRIS_StackGet(0) == NULL || IRIS_StackGet(0)[0] == '\0')
								IRIS_StackPop(1);
							else
							{
								char * top = NULL;
								UtilStrDup(&top, IRIS_StackGet(0));
								IRIS_StackPop(1);
								stackLevel--;
								IRIS_StackPush(top);
								my_free(top);
								keepGoing = false;
							}
							break;
					}
				}
				// Just in case we did not process 4 elements in the array above - format error
				UtilStrDup(&first, NULL);
			}

			// If nothing was resolved, make sure we adjust the level of the stack.
			if (keepGoing)
			{
				stackLevel--;
				IRIS_StackPush(NULL);
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_RenameObject
//
// DESCRIPTION:	Renames an object changing its name string
//
// PARAMETERS:	objectData	<=	A buffer containing the object to be renamed
//				newName		<=	New name for the object
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void IRIS_RenameObject(char * objectData, char * newName)
{
	char * namePtr;

	FILE_HANDLE handle = open(newName, FH_NEW);

	// Find the start of the name label
	namePtr = strstr(objectData, "NAME:");

	// Write the source object up to its name
	write(handle, objectData, namePtr-objectData);

	// Write the new name with its label and a comma delimeter
	write(handle, "NAME:", 5);
	write(handle, newName, strlen(newName));
	write(handle, ",", 1);

	// Write the rest of the object
	namePtr = strchr(namePtr+1, ',');
	write(handle, namePtr+1, strlen(namePtr+1));
	close(handle);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_ProcessActionObject
//
// DESCRIPTION:	Process an action object
//
// PARAMETERS:	objectData	<=	A buffer containing the object to be evaluated.
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void IRIS_ProcessActionObject(char * objectData)
{
	int i;
	int myStackIndex;
	const char operators[] = {'=', '+', '-', '/', '*', '%', '&', '^', '|', '>', '<'};	// Also ++, --, << and >>

	// Get one string at a time
	for (i = 0; ; i++, IRIS_StackPop(stackIndex-myStackIndex))
	{
		int j;
		int size;
		char stringName[50];
		char temp[100];
		char * stringValue;
		char operator[3];
		char * first, * second, * result;
		uchar * firstHex, * secondHex;
		bool deleteFlag = false;
		bool emptySecond = false;

		// Make sure we know where we are in the stack
		myStackIndex = stackIndex;

		// Get one string at a time by setting the appropriate position number
		sprintf(stringName, "%d", i);

		// Get the proper string name and value of the position requested
		if ((stringValue = IRIS_GetStringValue(objectData, strlen(objectData), stringName, false)) == NULL)
			break;

		// Extract any possible single operators which would have been appended to the string name
		memset(operator, 0, sizeof(operator));
		for (j = 0; j < sizeof(operators); j++)
		{
			if (stringName[strlen(stringName)-1] == operators[j])
			{
				operator[0] = operators[j];
				stringName[strlen(stringName)-1] = '\0';

				if ((operator[0] == '+' || operator[0] == '-' || operator[0] == '<' ||  operator[0] == '>')  && stringName[strlen(stringName)-1] == operator[0])
				{
					stringName[strlen(stringName)-1] = '\0';
					operator[1] = operator[0];
				}

				break;
			}
		}

		// Resolve the string assignment value to a simple assignment value. Result is in the stack
		if (stringValue[0] == 0 && stringValue[4] == '\0') emptySecond = true;
		IRIS_ResolveToSingleValue(stringValue, false);
		IRIS_DeallocateStringValue(stringValue);	// No need for the string value past this point
		second = IRIS_StackGet(0);				// We now have a reference to the assignment
		if (second && strcmp(second, "NULL") == 0)
			second = NULL;

		// Obtain the current value of the string if it is not an assigment
		if (operator[0] != '\0' && operator[0] != '=')
		{
			// Obtain the current value of the string if stored in the temporary data area or within the object file
			memset(temp, 0, sizeof(temp));
			strcpy(&temp[4], stringName);

			// Get the current value of the string on the stack
			IRIS_ResolveToSingleValue(temp, false);

			// Make sure we reference the two values correctly from the stack
			first = IRIS_StackGet(0);	// This was added last
			if (first && strcmp(first, "NULL") == 0)
				first = NULL;
		}
		else first = NULL;

		temp[0] = '\0';				// This will be used for arithmetic operations
		result = NULL;				// This will be used for string operations and will eventually contain the result

		switch(operator[0])
		{
			// Assignment (most common) operator...
			case '\0':
			case '=':
				if (second)
					UtilStrDup(&result, second);
				break;
			case '*':
			case '/':
			case '%':
				if (first && second)
				{
					if (operator[0] == '*')
						sprintf(temp, "%ld", atol(first) * atol(second));
					else if (atol(second) != 0)
					{
						if (operator[0] == '/')
							sprintf(temp, "%ld", atol(first) / atol(second));
						if (operator[0] == '%')
							sprintf(temp, "%ld", atol(first) % atol(second));
					}
					break;
				}
				
				strcpy(temp, "0");
				break;
			case '+':
				if (operator[1] == '+')
				{
					if (second && second[0])
					{
						if (!first)
							UtilStrDup(&result, second);
						else
						{
							result = my_malloc(strlen(first) + strlen(second) + 1);
							sprintf(result, "%s%s", first, second);
						}
					}
					else if (emptySecond)
					{
						if (!first)
							temp[0] = '1', temp[1] = '\0';
						else
						{
							size = strlen(first);
							sprintf(temp, "%0*ld", size, atol(first)+1);
						}
					}
					else 
						UtilStrDup(&result, first);
				}
				else
				{
					size = second?strlen(second):0;
					if (first && (int) strlen(first) > size) size = strlen(first);
					sprintf(temp, "%0*ld", size, (first?atol(first):0)  + (second?atol(second):0));
				}
				break;
			case '-':
				if (operator[1] == '-')
				{
					if (first)
					{
						if (second && second[0])
						{
							size = strlen(first) - atol(second) + 1;
							if (size > 0)
							{
								result = my_malloc(size);
								sprintf(result, "%.*s", size - 1, first);
							}
						}
						else if (emptySecond)
						{
							size = strlen(first);
							sprintf(temp, "%0*ld", size, atol(first) - 1);
						}
					}
				}
				else
				{
					size = second?strlen(second):0;
					if (first && (int) strlen(first) > size) size = strlen(first);
					sprintf(temp, "%0*ld", size, (first?atol(first):0)  - (second?atol(second):0));
				}
				break;

			case '&':
			case '^':
			case '|':
				// Handle null conditions first
				if (!first) first = "00";
				if (!second) second = "00";

				// Convert to Hex first using maximum number of digits - Assuming ASCII hex digit input...Safe but rubbish result otherwise
				size = strlen(first);
				if (strlen(second) > (uint) size) size = strlen(second);

				firstHex = my_malloc(size / 2 + 1);
				secondHex = my_malloc(size / 2 + 1);

				UtilStringToHex(first, size, firstHex);
				size = UtilStringToHex(second, size, secondHex);

				// Result is TRUE if a non zero result
				for (j = 0; j < size; j++)
				{
					if (operator[0] == '^')			firstHex[j] ^= secondHex[j];
					else if (operator[0] == '&')	firstHex[j] &= secondHex[j];
					else							firstHex[j] |= secondHex[j];
				}
				result = my_malloc(size*2+1);
				UtilHexToString(firstHex, size, result);
				my_free(secondHex);
				my_free(firstHex);
				break;
			case '>':
				if (operator[1] == '>')
				{
					if (first)
					{
						int count;
						int max = (second && second[0] != '\0')?atol(second):99999;

						// Only operate on objects
						if (stringName[0] != '/' || stringName[1] != '/')
							break;

#ifdef _DEBUG
						printf("Removing array Object ====> %s\n", &first[1]);
#endif

						// If the there is a number at the end of the object name (ie an array object), then this is used to delete a single object array without shifting downwards.
						if (first[strlen(first)-1] >= '0' && first[strlen(first)-1] <= '9')
							_remove(&first[1]);

						// Otherwise, find all the object arrays and remove them sequentially...
						else for (count = 0; count < max; count++)
						{
							sprintf(temp, "%s%d", &first[1], count);
#ifdef _DEBUG
							printf("Removing Object ====> %s\n", temp);
#endif
							if (_remove(temp) == -1)
							{
								if (!second || second[0] == '\0')
									break;
							}
						}
					}
				}
				else
					deleteFlag = true;
				break;
			case '<':
				if (operator[1] == '<')
				{
					if (second && second[0])
						size = atol(second);
					else
						size = 1;

					if (first && (int) strlen(first) > size)
						UtilStrDup(&result, &first[size]);
					else
						UtilStrDup(&result, NULL);
				}
				else
				{
					char * data;
					uint length;

					// We need to create or override the "first" object. "first" contains the object name in this case, and
					// the second contains the source object name.

					// Get the second object data
					if (first && second && (data = IRIS_GetObjectData(&second[1], &length)) != NULL)
					{
						IRIS_RenameObject(data, &first[1]);
						my_free(data);
						IRIS_TemporaryObjectStringValue(NULL, false);
					}
				}
				break;
		}

		// If we have a numeric result, make it a string first
		if (temp[0])
			UtilStrDup(&result, temp);

		// Obtain the full name and resolve array subscripts
		IRIS_FullName(stringName, temp);

		// Store the result
		if (temp[0] && (operator[0] != '<' || operator[1] != '\0') && (operator[0] != '>' || operator[1] != '>'))
			IRIS_StoreData(temp, result, deleteFlag);

		if (result) my_free(result);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_Eval
//
// DESCRIPTION:	Evaluates aa string value into a simple value and returns the result to the
//				caller via the stack
//
// PARAMETERS:	value	<=	The value
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void IRIS_Eval(char * value, bool partial)
{
	char * evalObject = my_malloc(strlen(value)+10);
	char * resolvableStringValue;

	sprintf(evalObject, "{EVAL:%s}", value);

	// Extract a resolvable string from the evaluation object
	resolvableStringValue = IRIS_GetStringValue(evalObject, strlen(evalObject), "EVAL", false);
	my_free(evalObject);

	// Resolve it to a single simple string. Result in the stack
	IRIS_ResolveToSingleValue(resolvableStringValue, partial);

	// Finally lose the resolvable string
	IRIS_DeallocateStringValue(resolvableStringValue);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_FullName
//
// DESCRIPTION:	Evaluates aa string value into a simple value and returns the result to the
//				caller via the stack
//
// PARAMETERS:	value	<=	The value
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void IRIS_FullName(char * string, char * fullName)
{
	// Examine if we are accessing a temporary data (for the current object by default).
	// We need a fully qualified string name
	if (string[0] == '~')
		sprintf(fullName, "/%s/%s", currentObject, &string[1]);
	else
		strcpy(fullName, string);

	// If it is in the form string(), then use string/INDEX to reference the correct array element.
	____toNonArray(fullName);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_StoreData
//
// DESCRIPTION:	Evaluates aa string value into a simple value and returns the result to the
//				caller via the stack
//
// PARAMETERS:	value	<=	The value
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//

//
// Stores the string and its simple value in the temporary data array area.
//
// Returns false if not enough epace
//
static bool ____storeData(char * string, char * value, char * group)
{
	int i;
	char * ptr;
	int strLen = strlen(string);

#ifdef _DEBUG
	printf("Storing data:::::::::::::     %s = %s\n", string, value?value:"NULL");
#else
//	if (debug)
//	{
//		char keycode;
//		char tempBuf[2000];
//		DispText("", 1, 1, false, false, false);
//		sprintf(tempBuf, "Storing => %s:%.60s ", string, value?value:"NULL");
//		write_at(tempBuf, strlen(tempBuf), 1, 1);
//		while(read(STDIN, &keycode, 1) != 1);
//	}
#endif

	// Stop any malicious naming that overrides the main object identifiers
	if	(string &&
			((strLen >= 5 && strcmp(&string[strLen-5], "/NAME") == 0) ||
			 (strLen >= 5 && strcmp(&string[strLen-5], "/TYPE") == 0) ||
			 (strLen >= 6 && strcmp(&string[strLen-6], "/GROUP") == 0))
		)
		return false;

	// Locate the first empty location and delete the string and its value if available
	for (i = 0; i < max_temp_data; i++)
	{
		if (tempData[i].name && string && strcmp(tempData[i].name, string) == 0)
		{
			UtilStrDup(&tempData[i].name, NULL);
			UtilStrDup(&tempData[i].value, NULL);
			UtilStrDup(&tempData[i].group, NULL);
			break;
		}
	}

	// If the data area is full....
	if (i == C_MAX_TEMP_DATA)
		return false;

	// If there is room and we do not want to delete, store/update the entry.
	if (value)
	{
		UtilStrDup(&tempData[i].name, string);
		UtilStrDup(&tempData[i].value, value);
		UtilStrDup(&tempData[i].group, group);
		if (i == max_temp_data) max_temp_data++;
	}
	else if (string && i < max_temp_data)
	{
		// Compact the temporary data
		if (i != (max_temp_data-1))
		{
			tempData[i] = tempData[max_temp_data-1];
			memset(&tempData[max_temp_data-1], 0, sizeof(T_TEMPDATA));
		}

		// We now have one less element
		max_temp_data--;

		ptr = &string[strlen(string)-1];

		// If deleting and the string ends with a number, then it is an array....
		if (*ptr >= '0' && *ptr <= '9')
		{
			int index;
			char fromTemp[100];
			char toTemp[100];

			// Point to the beginning of the array number
			while (ptr > (string+1) && ptr[-1] >= '0' && ptr[-1] <= '9') ptr--;

			// Shift all temporary data arrays following this temporary data downwards		
			for (index = atol(ptr)+1;;index++)
			{
				sprintf(fromTemp, "%.*s%d", ptr-string, string, index);
				sprintf(toTemp, "%.*s%d", ptr-string, string, index-1);

				for (i = 0; i < max_temp_data; i++)
				{
					if (tempData[i].name && strcmp(tempData[i].name, fromTemp) == 0)
					{
						UtilStrDup(&tempData[i].name, NULL);
						UtilStrDup(&tempData[i].name, toTemp);
						break;
					}
				}

				if (i == max_temp_data) break;
			}
		}
	}

	return true;
}

static ____remove_object(char * objectName)
{
	char * ptr = &objectName[strlen(objectName)-1];
#ifdef _DEBUG
	printf("Removing Object ====> %s\n", objectName);
#endif
	_remove(objectName);

	// Find if there are any array files following this object and shift them downwards
	if (*ptr >= '0' && *ptr <= '9')
	{
		int index;
		char fromObject[100];
		char toObject[100];
		char * nextObjectData;
		uint length;

		// Point to the beginning of the array number
		while (ptr > (objectName+1) && ptr[-1] >= '0' && ptr[-1] <= '9') ptr--;

		// Shift all objects arrays following this object downwards		
		for (index = atol(ptr)+1; ;index++)
		{
			sprintf(fromObject, "%.*s%d", ptr-objectName, objectName, index);
			sprintf(toObject, "%.*s%d", ptr-objectName, objectName, index-1);

			if ((nextObjectData = IRIS_GetObjectData(fromObject, &length)) == NULL)
				break;

			IRIS_RenameObject(nextObjectData, toObject);
#ifdef _DEBUG
			printf("Renaming Object ====> from:%s to:%s\n", fromObject, toObject);
#endif
			_remove(fromObject);
			my_free(nextObjectData);
		}
	}

	IRIS_TemporaryObjectStringValue(NULL, false);
}

#ifdef _DEBUG
static int _delete_(FILE * fp, int count, char * ptr, char * objectData, uint objectLength)
{
	char * restOfData = my_calloc(objectLength - (uint) (ptr - objectData) + 1);
	long position = ftell(fp);

	fread(restOfData, 1, objectLength - (uint) (ptr - objectData) + 1, fp);
	fseek(fp, position, SEEK_SET);
	fwrite(restOfData + count, 1, strlen(restOfData) - count + 1, fp);
	fseek(fp, position, SEEK_SET);
	my_free(restOfData);

	// *** NOTE that this does not truncate the file but adds a null character towards the end. The file size will not change as result...
	// If porting to other devices and using this method, deal with this please... Possibly by reading the entire file as a string and writing it back...This is how I had it in the beginning...

	return 0;
}

static int _insert(FILE * fp, char * data, int count, char * ptr, char * objectData, uint objectLength)
{
	char * restOfData = my_calloc(objectLength - (uint) (ptr - objectData) + 1);
	long position = ftell(fp);

	fread(restOfData, 1, objectLength - (uint) (ptr - objectData) + 1, fp);

	fseek(fp, position, SEEK_SET);
	fwrite(data, 1, count,fp);
	position = ftell(fp);

	fwrite(restOfData, 1, strlen(restOfData) + 1, fp);
	fseek(fp, position, SEEK_SET);
	my_free(restOfData);

	return count;
}
#endif

//-------------------------------------------------------------------------------------------
void IRIS_StoreData(char * fullName, char * value, bool deleteFlag)
{
	int j;
	int offset = 0;
	char * objectData;
	uint objectLength;
	char objectName[100];
	char * group = NULL;


	// Examine if we are accessing the object file itself
	if (fullName[1] == '/') offset = 1;

	// Extract the object name on its own
	for (j = 0; fullName[j+1+offset] != '/' && fullName[j+1+offset]; j++)
		objectName[j] = fullName[j+1+offset];
	objectName[j] = '\0';

	// Get the object data
	if ((objectData = IRIS_GetObjectData(objectName, &objectLength)) == NULL && offset == 1)
	{
		// If this is a delete operation, then attempt a deletion in case it is part of an array object requiring shifting downwards of other objects
		if (deleteFlag)
		{
			____remove_object(objectName);
			return;
		}

		// If there is no value, stop now
		if (!value)
			return;

		// Create enough space for the overheads (ie TYPE:DATA,NAME: and GROUP: and VERSION: and a bit more... then any new tag:value
		objectData = my_malloc	(	50 + strlen(objectName) + (fullName[j+2]?strlen(currentObjectGroup):strlen(value)) + strlen(currentObjectVersion) +
									(fullName[j+2]?strlen(&fullName[j+3]):0) + strlen(value));

		// Add the overheads with the appropriate name, group and version.
		sprintf(objectData, "{TYPE:DATA,NAME:%s,GROUP:%s,VERSION:%s", objectName, fullName[j+2]?currentObjectGroup:(value?value:""), currentObjectVersion);

		// Add the new tag:value if it exists.
		if (fullName[j+2] && fullName[j+3])
			sprintf(&objectData[strlen(objectData)], ",%s:%s}", &fullName[j+3], value);
		else
			strcat(objectData,"}");

		// Store the data and exit...all done
		IRIS_PutNamedObjectData(objectData, strlen(objectData), objectName);
		UtilStrDup(&objectData, NULL);
		return;
	}

	// If the object already exists and this is an object creation operation, then no point to continue further for speed purposes
	if (offset == 1 && fullName[j+2] == '\0' && deleteFlag == false)
	{
		UtilStrDup(&objectData, NULL);
		return;
	}

	// Obtain the group information but not for objectless temporary data
	if (objectData)
		group = IRIS_GetStringValue(objectData, objectLength, "GROUP", false);

	// The object must have a simple value group for restricted access. If no group, my_free access.
	if (!group || (group[0] == 0 && (group[4] == '\0' || strcmp(&group[4], currentObjectGroup) == 0 || strcmp(currentObjectGroup, irisGroup) == 0)))
	{
		// If it is a temporary data area, store/update the value
		if (offset == 0)
		{
			// Only store if an object AND a string are specified
			if (strchr(&fullName[1], '/') != NULL)
				____storeData(fullName, deleteFlag?NULL:value, group?&group[4]:"");
		}
		else
		{
			char * type = IRIS_GetStringValue(objectData, objectLength, "TYPE", false);

			if (type)
			{
				if (type[0] == 0 && (strcmp(&type[4], "DATA") == 0 || strncmp(&type[4], "CONFIG", 5) == 0) )
				{
					char * ptr, * ptr2;
					char search[100];

					// If this is an object name request
					if (fullName[j+2] == '\0')
					{
						// Remove the object if requested
						if (deleteFlag)
							____remove_object(objectName);
					}

					// If this is an array element deletion, thne modify the buffered data and write the whole thing back in one go
					else if ((deleteFlag || !value) && fullName[strlen(fullName)-1] >= '0' && fullName[strlen(fullName)-1] <= '9')
					{
						int len;
						int element;

						for (len = 0; fullName[j+3] < '0' || fullName[j+3] > '9'; len++, j++)
							search[len] = fullName[j+3];
						search[len] = '\0';
						element = atol(&fullName[j+3]);

						for (ptr = objectData; (ptr = strstr(ptr, search)) != NULL;)
						{
							if (ptr[-1] == ',' || ptr[-1] == '{')
							{
								int element2 = 0;

								for (ptr2 = &ptr[len];*ptr2 >= '0' && *ptr2 <= '9'; ptr2++)
									element2 = element2 * 10  + *ptr2 - '0';

								if (ptr2 != &ptr[len])
								{
									if (element == element2)
									{
										if ((ptr2 = strchr(ptr, ',')) == NULL)
											ptr2 = strchr(ptr, '}');
										if (ptr[-1] == ',') ptr--;
										memmove(ptr, ptr2, strlen(ptr2) + 1);
									}
//									else
									else if (element < element2)	// Only affect array elements greater than the one getting deleted...new V1.06
									{
										char newTag[100];
										sprintf(newTag, "%s%d", search, element2 - 1);
										memcpy(ptr, newTag, strlen(newTag));
										// This is to overcome array element 10 changing to 9, or 100 changing to 99 where the new
										// tag string length is smaller.
										if ((ptr + strlen(newTag)) != ptr2)
											memmove(ptr + strlen(newTag), ptr2, strlen(ptr2) + 1);
										ptr += strlen(newTag);
									}
									else ptr += strlen(search);		// new V1.06
								}
							}
							else
							{
								if ((ptr2 = strchr(ptr, ',')) == NULL)
									ptr2 = strchr(ptr, '}');
								ptr = ptr2;
							}
						}

						IRIS_PutObjectData(objectData, strlen(objectData));
					}
					else
					{
						char * newTagValue = NULL;

						// Create the new tag value pair
						if (deleteFlag == false && value)
						{
							newTagValue = my_malloc(strlen(&fullName[j+3]) + strlen(value) + 3);
							sprintf(newTagValue, ",%s:%s", &fullName[j+3], value);
						}

						// Look for the beginning of the string
						sprintf(search, ",%s:", &fullName[j+3]);
						if ((ptr = strstr(objectData, search)) == NULL)
						{
							if (*objectData == '{' && strncmp(&objectData[1], &search[1], strlen(&search[1])) == 0)
								ptr = objectData + 1;
						} else if (newTagValue) ptr++;

						// If an existing value exists, find the end of the existing tag:value
						if (ptr)
						{
							if ((ptr2 = strchr(ptr+1,',')) == NULL)	// This will not work with complex value. It must be simple
								ptr2 = strchr(ptr+1,'}');
							else if (!newTagValue && *ptr != ',') ptr2++;

							// If they are exactly the same value, do not attempt to write anything....skip this.
							if (newTagValue && (strlen(&newTagValue[1]) == (uint) (ptr2 - ptr)) && strncmp(&newTagValue[1], ptr, ptr2-ptr) == 0)
							{
								printf("Skipping storing same PERM data ====> %s:%s\n", fullName, value);								
								ptr = NULL;
							}
						}
						// If not found, add the new tag:value it to the end of the file as long as we have a new tag:value
						else if (newTagValue)
							ptr2 = ptr = strrchr(objectData,'}');

						// Replace with the new tag:value
						if (ptr)
						{
							FILE_HANDLE handle;

							// Open the object file for an update
							printf("Storing PERM data ====> %s:%s\n", fullName, value);
							handle = open(objectName, FH_RDWR);
							lseek(handle, ptr - objectData, SEEK_SET);

							if (newTagValue && (strlen(&newTagValue[1]) == (uint) (ptr2 - ptr)))
								write(handle, &newTagValue[1], ptr2 - ptr);
							else
							{
								int offset = 0;
								if (ptr != ptr2)
								{
									_delete_(handle, ptr2 - ptr, ptr, objectData, objectLength);
									offset = 1;
								}
								if (newTagValue)
									_insert(handle, &newTagValue[offset], strlen(&newTagValue[offset]), ptr, objectData, objectLength);
							}

							// If this is an array element getting deleted, then slosh all following elements backwards
							if (!newTagValue)
							{
								char * ptr = &search[strlen(search) - 1];

								*ptr = '\0';	// The search looses the ending colon
								ptr--;
								if (*ptr >= '0' && *ptr <= '9')
								{
									// Lose any trailing "array" digits
									while (*ptr >= '0' && *ptr <= '9')
									{
										*ptr = '\0';
										ptr--;
									}

									// Look for the next element
									lseek(handle, 0, SEEK_SET);

								}
							}

							close(handle);
							IRIS_TemporaryObjectStringValue(NULL, false);
						}
						UtilStrDup(&newTagValue, NULL);
					}
				}
				IRIS_DeallocateStringValue(type);
			}
		}
	}

	// Clean up
	IRIS_DeallocateStringValue(group);
	UtilStrDup(&objectData, NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_GetCount
//
// DESCRIPTION:	Returns the count of the a string array
//
//
// PARAMETERS:	fullName	<=	The "count" or "index" name of the array - fully name qualified for other array on
//								top of the one we are evaluating. For an array /a/b/c(), the index is /a/b/c/INDEX
//								and the count is /a/b/c/COUNT for example.
//
//								for /a/b()/c(), it should be /a/b5/c/INDEX or /a/b5/c/COUNT for an example wher
//								/a/b/INDEX is currently = 5.
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
int IRIS_GetCount(char * fullName)
{
	int i = -1;
	char array[100];
	char * countPtr;
	bool stop = false;

	strcpy(array, fullName);
	stackLevel++;

	// Make sure it is an index or count string
	if ((countPtr = strstr(array, indexTrailer)) != NULL || (countPtr = strstr(array, countTrailer)) != NULL)
	{
		int myStackIndex = stackIndex;

		for(;!stop;)
		{
			// Look for the next array
			sprintf(countPtr, "%d", ++i);

			// If an object is being checked, then just examine if the file exists.
			if (strchr(&array[1], '/') == NULL)
			{
				FILE_HANDLE handle;
				handle = open(&array[1], FH_RDONLY);
				if (FH_ERR(handle))
					stop = true;
				else close(handle);
			}
			else
			{
				// Go ahead and evaluate the string
				IRIS_Eval(array, true);		// We pass partial because we only want the first IRIS_GetStringValue() to be partial or first search in temporary data area to be partial

				// If we can't find it, we have reached the end of the array
				if (IRIS_StackGet(0) == NULL)
					stop = true;

				IRIS_StackPop(stackIndex - myStackIndex);
			}
		}
	}

	stackLevel--;
	return i;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_ClrTemp
//
// DESCRIPTION:	Clears the temporary data of the specified object.
//				Only if the group is authorised access by the current object group that this is authorised
//
//				This can only be used to remove temporary data from multiple objects if they all start
//				with the object name supplied
//
//
// PARAMETERS:	objectName	<=	The object name whose temporary data will be affected
//
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
int IRIS_ClrTemp(char * objectName)
{
	int i;
	int count = 0;
	int count2 = 0;

	// If it is found in the data array, remove it
	for (i = 0; i < max_temp_data; i++)
	{
		if (tempData[i].name)
		{
			count2++;
			if (tempData[i].name && strncmp(&tempData[i].name[1], objectName, strlen(objectName)) == 0 && tempData[i].name[1+strlen(objectName)] == '/' &&
				(tempData[i].group[0] == '\0' || strcmp(tempData[i].group, currentObjectGroup) == 0 || strcmp(currentObjectGroup, irisGroup) == 0))
			{
				count++;
				____storeData(tempData[i--].name, NULL, NULL);
			}
		}
	}

	return count;
}
