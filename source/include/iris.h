#ifndef __IRIS_H
#define __IRIS_H

/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       iris.h
**
** DATE CREATED:    31 January 2008
**
** AUTHOR:			Tareq Hafez
**
** DESCRIPTION:     Header file for the iris JSON object support functions
**-----------------------------------------------------------------------------
*/

#include "input.h"


/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/
extern char irisGroup[];

/*
**-----------------------------------------------------------------------------
** Type definitions
**-----------------------------------------------------------------------------
*/
typedef struct
{
	char * group;
	char * name;
	char * value;
	uchar flag;		// bit 0: Read access by other groups allowed. (.r)
					// bit 1: Write access by other groups allowed (.w)
					// bit 2: Byte Array. (.h)
					// bit 8: temp. Automatically deleted when IDLE display is current. (.t)
} T_TEMPDATA;

extern char * currentObject;
extern char * currentObjectData;
extern uint currentObjectLength;
extern char * currentObjectVersion;
extern char * currentObjectGroup;
extern char * currentEvent;
extern char * currentEventValue;
extern char * newEventValue;

extern char * prevObject;
extern char * nextObject;

extern char * forceNextObject;

extern int stackIndex;
extern uchar stackLevel;

extern int mapIndex;

extern ulong displayTimeout;
extern int displayTimeoutMultiplier;
extern int displayTimeoutMultiplierFull;

extern unsigned char lastKey;
extern bool vmacRefresh;

extern char * currentTableName;
extern char * currentTableData;
extern bool currentTableValid;
extern char * currentTablePromptValue;

// This is defined within irismain.c
char * getLastKeyDesc(unsigned char key, T_KEYBITMAP * keyBitmap);


/*
**-----------------------------------------------------------------------------
** Functions
**-----------------------------------------------------------------------------
*/
// Push a simple string value onto the stack if stack available
void IRIS_StackPush(char * value);
bool IRIS_StackPushFunc(char * function);

// Pop "count" values from the top of the stack
void IRIS_StackPop(int count);

// Examine a stack value offseted by "offset" from the top of the stack
char * IRIS_StackGet(char offset);

// Flushes the stack and deallocates all memory used within the stack
void IRIS_StackFlush(void);

// (Re)allocates the stack. The stack size is as specified or C_MAX_STACK if size = 0
void IRIS_StackInit(int size);

bool IRIS_StackInitialised(void);

// Add data to be uploaded to the host at the next session
void IRIS_AppendToUpload(char * addition);

// Store an object data buffer into a file.
void IRIS_PutNamedObjectData(char * objectData, uint length, char * name);

// Store an object data buffer into a file. The file name will be the value of the string "NAME" in the object
void IRIS_PutObjectData(char * objectData, uint length);

// Establish a connection with the host to obtain the object specified and upload any pending data to the host
void IRIS_GetExternalObjectData(char * objectName);

// Get the object data buffer from a file. The file name is the object name. Files are stored by their object names.
char * IRIS_GetObjectData(char * objectName, unsigned int * length);

// Examines a resource object if available within the terminal storage. If not, it contacts the host and downloads it
bool IRIS_DownloadResourceObject(char * objectName);

// Given an object data, search for a string and return its value in an allocated tree strcutures "array of arrays where the leaves are simple values"
char * IRIS_GetStringValue(char * data, int size, char * name, bool partial);

// Deallocate the allocate tree structure structure
void IRIS_DeallocateStringValue(char * value);

// Given an allocated tree structure, evaluate the entire tree pushing (and poping for functions and concatenations) values on the stack
void IRIS_ResolveToSingleValue(char * value, bool partial);

// Given an object data that is an action object data. Examine each string in the object and its actual object location within the system, then assign that value to that actual location
void IRIS_ProcessActionObject(char * objectData);

// Given a string value, create a temporary object surrouding it and evaluate the value
void IRIS_Eval(char * value, bool partial);

// Returns the fully qualified name of the string parameter
void IRIS_FullName(char * string, char * fullName);

// Given a fully qualified string name and value, store the value in the string
void IRIS_StoreData(char * fullName, char * value, bool deleteFlag);

// Returns the count of an array
int IRIS_GetCount(char * fullName);

// Clears the temporary data of the specified object only if the current group permits. The specified group can be the beginning of the object name affecting multiple objects if required
int IRIS_ClrTemp(char * objectName);

#endif /* __IRIS_H */
