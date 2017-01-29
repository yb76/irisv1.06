//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       irisutil.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports security functions
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
#include <errno.h>

#ifdef __VMAC
#	include "eeslapi.h"
#	include "logsys.h"
#	include "devman.h"
#	include "vmac.h"
#	include "version.h"
#endif


//
// Local include files
//
#include "alloc.h"
#include "zlib.h"
#include "as2805.h"
#include "security.h"
#include "utility.h"
#include "iris.h"
#include "irisfunc.h"
#include "display.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//

//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//
char * currentTableName = NULL;
char * currentTableData = NULL;
uint currentTableLength;
bool currentTableValid = false;
char * currentTablePromptValue = NULL;

char * currentGraphicsName = NULL;
char * currentGraphicsData = NULL;
uint currentGraphicsLength;
bool currentGraphicsValid = false;
char * currentGraphicsImageValue = NULL;

char lastFaultyFileName[100];
char lastFaultyGroup[50];

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// GENERAL iRIS UTILITY FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void __tcp_disconnect_do(void);

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()LOCATE
//
// DESCRIPTION:	Locates and updates the array index
//
// PARAMETERS:	index	=>	The index to update starting from zero until count
//				first	<=	The first string to compare its value to
//				second	<=	The second string to compare its value to
//				wildcard<=	Skip n character from the "second" at the end of the "second" string.
//							If wildcard is "CPAT", then the second is treated as "from-to-length".
//							If wildcard is "MIN", then return the one with the least value
//							If wildcard is "MAX", then return the one with the least value
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __locate(void)
{
	char * wildcard = IRIS_StackGet(0);
	char * second = IRIS_StackGet(1);
	char * first = IRIS_StackGet(2);
	char * index = IRIS_StackGet(3);

	stackLevel++;

	if (second && first && index && wildcard)
	{
		char fullName[100];

		int myStackIndex = stackIndex;
		int i, j, count;
		long num;
		char temp[10];

		// Adjust any strings passed by reference
		if (second[0] == '\'') second++;
		if (first[0] == '\'') first++;
		if (index[0] == '\'') index++;

		// Get the count by removing index string and continuously appending an incremental number until we hit a string not found
		if (strstr(index, "/INDEX"))
		{
			IRIS_FullName(index, fullName);
			count = IRIS_GetCount(fullName);

			// Loop until index reachs count
			for (i = 0, j = 9999; i < count; i++)
			{
				// Reset the stack
				IRIS_StackPop(stackIndex - myStackIndex);

				// Set the value of the index
				IRIS_StoreData(fullName, ltoa(i, temp, 10), false);

				// Evaluate the first (complex) string
				IRIS_Eval(first, false);

				// Evaluate the second (complex) string
				IRIS_Eval(second, false);

				if (IRIS_StackGet(0) && IRIS_StackGet(1))
				{
					// If we get 2 results and they match, stop. This is the required index value
					if (stackIndex == (myStackIndex+2))
					{
						long num1, num2;
						char * comp = IRIS_StackGet(0);

						if (j == 9999) j = 0;

						if (strcmp(wildcard, "CPAT") == 0)
						{
							char * ptr;

							// Locate the "from" string....
							for (ptr = comp; *ptr; ptr++)
							{
								if (*ptr == '=')
									break;
							}

							// It must be >= "from"
							if (*ptr && strncmp(IRIS_StackGet(1), comp, ptr - comp) >= 0)
							{
								// Locate the "to" string....
								for (comp = ptr = ptr + 1; *ptr; ptr++)
								{
									if (*ptr == '=')
										break;
								}

								// It must be <= "to"
								if (*ptr && strncmp(IRIS_StackGet(1), comp, ptr - comp) <= 0)
								{
									int len;
									char * card = IRIS_StackGet(1);
									int checkLen = UtilStringToNumber(ptr + 1);

									// Find out the card length
									for (len = 0; *card && *card != '='; len++, card++);

									// If the check length = 0 or the lengths match, we found it...
									if (len == checkLen || checkLen == 0)
										break;
								}
							}
						}
						else if (strcmp(wildcard, "MIN") == 0)
						{
							num2 = UtilStringToNumber(comp);
							if (i == 0 || num2 < num)
								num = num2, j = i;
						}
						else if (strcmp(wildcard, "MAX") == 0)
						{
							num2 = UtilStringToNumber(comp);
							if (i == 0 || num2 > num)
								num = num2, j = i;
						}
						else
						{
							while(comp[strlen(comp)-1] == wildcard[0] && wildcard[0])
								comp[strlen(comp)-1] = '\0';

							num1 = UtilStringToNumber(IRIS_StackGet(1));
							num2 = UtilStringToNumber(comp);

							if (num1 != -1 && num2 != -1)
							{
								if (num1 == num2)
									break;
							}
							else if (strncmp(IRIS_StackGet(1), comp, strlen(comp)) == 0)
								break;
						}
					}
				}
			}

			// Reset the stack
			IRIS_StackPop(stackIndex - myStackIndex);

			if (i == count)
			{
				// Set the maximum or minimum detected index if MAX or MIN requested
				if (strcmp(wildcard, "MIN") == 0 || strcmp(wildcard, "MAX") == 0)
					IRIS_StoreData(fullName, ltoa(j, temp, 10), false);

				// If we do not get a matching result, set the index to 9999
				else
					IRIS_StoreData(fullName, ltoa(9999, temp, 10), false);
			}
		}
	}

	// Remove the paramters and function name from the stack
	IRIS_StackPop(5);

	stackLevel--;
	IRIS_StackPush(NULL);	// Dummy value
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TEXT_TABLE
//
// DESCRIPTION:	Retrieves the text string from the text table
//
// PARAMETERS:	name	<=	Text table object name. This must be of type: TEXT TABLE.
//				string	<=	A string whose value will be compared to the first value of each array
//							When a match occurs, the search stops and the second value is 
//							returned from the array
//
// RETURNS:		The second value from the array but only if found
//-------------------------------------------------------------------------------------------
//
void __text_table(void)
{
	char * search = IRIS_StackGet(0);
	char * textTableObjectName = IRIS_StackGet(1);
	char * found = NULL;
	long num1 = UtilStringToNumber(search);

	if (search && textTableObjectName)
	{
		char * typeValue = NULL;

		// Try and find it in the system or remotely at the host
		if (textTableObjectName && strcmp(textTableObjectName, "DEFAULT_T"))
		{
			if (IRIS_DownloadResourceObject(textTableObjectName) == false)
			{
				IRIS_StackPop(3);
				IRIS_StackPush(NULL);
				return;
			}
		}

		// Obtain the object data if not already obtained
		if (currentTableData == NULL || currentTableName == NULL || strcmp(textTableObjectName, currentTableName))
		{
			currentTableValid = false;
			UtilStrDup(&currentTableName, textTableObjectName);
			UtilStrDup(&currentTableData, NULL);
			currentTableData = IRIS_GetObjectData(currentTableName, &currentTableLength);
		}

		// Given an object data
		if (currentTableData)
		{
			// Search for string "TYPE" and return its value in an allocated tree structures "array of arrays where the leaves are simple values"
			if (!currentTableValid)
				typeValue = IRIS_GetStringValue(currentTableData, currentTableLength, "TYPE", false);

			// Only continue if it is a TEXT TABLE object type
			if (currentTableValid || typeValue)
			{
				if (currentTableValid || (typeValue[0] == 0 && strcmp(&typeValue[4], "TEXT TABLE") == 0))
				{
					// Given an object data, search for string "PROMPT" and return its value in an allocated tree strcutures "array of arrays where the leaves are simple values"					
					if (!currentTableValid || !currentTablePromptValue)
					{
						if (currentTablePromptValue)
							IRIS_DeallocateStringValue(currentTablePromptValue);
						currentTablePromptValue = IRIS_GetStringValue(currentTableData, currentTableLength, "PROMPT", false);
					}

					// Check that the prompt string value is an array
					if (currentTablePromptValue)
					{
						if (currentTablePromptValue[0] == 1)
						{
							// Point to the beginning of the array past the array indicator
							char ** array1 = (char **) &currentTablePromptValue[4];

							// We haave a valid text table now
							currentTableValid = true;

							// Traverse the outer array list as long as each element in the list is another array
							for (; *array1 && (*array1)[0] == 1 && found == NULL; array1++)
							{
								char ** array2 = (char **) &((*array1)[4]);
								int i, j;

								// Traverse the internal array list only if they are simple values
								for (i = 0; i < 2 && *array2 && (*array2)[0] == 0 && found == NULL; array2++, i++)
								{
									// We have a list of values now. Point to a potential beginning of the value
									char * value = &(*array2)[4];
									long num2 = -1;
									
									if (num1 != -1)
										num2 = UtilStringToNumber(value);
					
									switch(i)
									{
										case 0:
											// Compare them as numbers if they both qualify as numbers
											if (num1 != -1 && num2 != -1)
											{
												if (num1 != num2) i = 1;
											}
											// Compare them as strings
											else if (strcmp(value, search)) i = 1;
											break;
										case 1:
											for (j = 0; value && j < (int) strlen(value); j++)
												if (value[j] == '`') value[j] = ',';
											UtilStrDup(&found, value);
											break;
									}
								}
							}
						}
					}
				}
				IRIS_DeallocateStringValue(typeValue);
			}
		}
	}

	// Lose the parameters and function name
	IRIS_StackPop(3);

	// Push the result onto the stack if available
	IRIS_StackPush(found);
	if (found) my_free(found);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()MAP_TABLE
//
// DESCRIPTION:	Analyses a map table. Each row has three values. If the first equals the second,
//				then the third value is returned
//
// PARAMETERS:	None
//
// RETURNS:		The third simplified and resolved value in the table
//-------------------------------------------------------------------------------------------
//
void __map_table(void)
{
	char * defaultValue = NULL;
	char * table = IRIS_StackGet(1);
	char * mapTableObjectName = IRIS_StackGet(2);
	char * found = NULL;

	defaultValue = UtilStrDup(&defaultValue, IRIS_StackGet(0));
	stackLevel++;

	if (table && mapTableObjectName)
	{
		char * objectData;
		uint length;

		// Try and find it in the system or remotely at the host
		if (mapTableObjectName)
		{
			if (IRIS_DownloadResourceObject(mapTableObjectName) == false)
			{
				IRIS_StackPop(4);
				stackLevel--;
				IRIS_StackPush(defaultValue);
				UtilStrDup(&defaultValue, NULL);
				return;
			}
		}

		// Obtain the object data
		objectData = IRIS_GetObjectData(mapTableObjectName, &length);

		// Given an object data
		if (objectData)
		{
			// Search for string "table" and return its value in an allocated tree strcutures "array of arrays where the leaves are simple values"
			char * tableValue = IRIS_GetStringValue(objectData, length, table, false);

			// Check that the prompt string value is an array
			if (tableValue)
			{
				if (tableValue[0] == 1)
				{
					// Point to the beginning of the array past the array indicator
					char ** array1 = (char **) &tableValue[4];

					// Traverse the outer array list as long as each element in the list is another array
					for (; *array1 && (*array1)[0] == 1 && found == NULL; array1++)
					{
						char ** array2 = (char **) &((*array1)[4]);
						char * first = NULL, * second = NULL;
						int myStackIndex = stackIndex;
						int i;

						// Traverse the internal array list only if they are simple values
						for (i = 0; i < 3 && *array2 && (*array2)[0] == 0 && found == NULL; array2++, i++)
						{
							// We have a list of values now. Point to a potential beginning of the value
							char * value = &(*array2)[4];
			
							switch(i)
							{
								case 0:
									// Get the first value
									IRIS_Eval(value, false);
									if (myStackIndex != stackIndex)
										first = IRIS_StackGet(0);
									break;
								case 1:
									// Get the second value
									IRIS_Eval(value, false);
									if (myStackIndex != stackIndex)
										second = IRIS_StackGet(0);

									// If different, move on to the next array to check it
									if ((!first && second) || (first && !second) || (first && second && strncmp(first, second, strlen(second)))) i = 2;
									break;
								case 2:
									UtilStrDup(&found, value);
									break;
							}
						}

						IRIS_StackPop(stackIndex - myStackIndex);
					}
				}
				IRIS_DeallocateStringValue(tableValue);
			}
			my_free(objectData);
		}

	}

	// Lose the parameters and function name
	IRIS_StackPop(4);
	stackLevel--;

	// Push the result onto the stack if available
	IRIS_StackPush(found?found:defaultValue);
	UtilStrDup(&defaultValue, NULL);
	if (found) my_free(found);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()NEW_OBJECT
//
// DESCRIPTION:	Creates a new object
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __new_object(void)
{
	char * table = IRIS_StackGet(0);
	char * objectName = IRIS_StackGet(1);
	int count = 0;
	char temp[10];

	if (table && objectName)
	{
		char * objectData;
		uint length;

		// Obtain the object data
		objectData = IRIS_GetObjectData(objectName, &length);

		// Given an object data
		if (objectData)
		{
			// Search for string "table" and return its value in an allocated tree strcutures "array of arrays where the leaves are simple values"
			char * tableValue = IRIS_GetStringValue(objectData, length, table, false);

			// Check that the prompt string value is an array
			if (tableValue)
			{
				if (tableValue[0] == 1 && tableValue[1] == 0 && tableValue[2] == 0 && tableValue[3] == 0)		// We must have an array
				{
					char ** array1 = (char **) &tableValue[4];
					char * myObject = NULL;
					char * newObjectName = NULL;
					char ** loopArray = NULL;
					int operation = C_PUT;

					for (; *array1; array1++)
					{
						if ((*array1)[0] == 1)		// Only work with arrays within the main array
						{
							int index;
							bool name = false;
							char ** array2 = (char **) &((*array1)[4]);
							int nameStart = -1;

							for (index = 0; *array2 && index < 2; array2++, index++)
							{
								int myStackIndex = stackIndex;

								switch(index)
								{
									case 0:
										IRIS_ResolveToSingleValue(*array2, false);
										if (IRIS_StackGet(0))
										{
											if (strcmp(IRIS_StackGet(0), "LOOP") == 0)
												loopArray = array1, operation = C_LOOP;
											else if (strcmp(IRIS_StackGet(0), "ENDLOOP") == 0)
												operation = C_END_LOOP;
											else
											{
												operation = C_PUT;
												if (strcmp(IRIS_StackGet(0), "NAME") == 0)
												{
													name = true;
													if (myObject) my_free(myObject);
													myObject = my_malloc(strlen(currentObjectGroup) + 40);
													sprintf(myObject, "{TYPE:DATA,GROUP:%s", currentObjectGroup);
												}
												if (myObject)
												{
													int size = strlen(myObject) + strlen(IRIS_StackGet(0)) + 3;
													myObject = my_realloc(myObject, size);
													nameStart = (int) strlen(myObject);
													sprintf(&myObject[strlen(myObject)], ",%s:", IRIS_StackGet(0));
												}
											}
										}
										break;
									case 1:
										IRIS_ResolveToSingleValue(*array2, false);
										if (operation == C_END_LOOP)
										{
											if (IRIS_StackGet(0) && strcmp(IRIS_StackGet(0), "LOOP") == 0)
												array1 = loopArray;
										}
										else if (operation != C_LOOP && IRIS_StackGet(0) && myObject)
										{
											// Allow one extra space for a closing brace '}' at the end. Save another allocation of just one more byte
											int size = strlen(myObject) + strlen(IRIS_StackGet(0)) + 2;
											myObject = my_realloc(myObject, size);
											strcat(myObject, IRIS_StackGet(0));

											// Store the object file name
											if (name && newObjectName == NULL)
												UtilStrDup(&newObjectName, IRIS_StackGet(0));

											// Count number of string:value pairs added
											count++;
										}
										else if (nameStart > -1 && myObject)
											myObject[nameStart] = '\0';
									default:
										break;
								}
								IRIS_StackPop(stackIndex - myStackIndex);
							}
						}
					}

					// We have dealt with all the arrays within the main array, now create the new object.
					if (myObject && newObjectName)
					{
#ifdef _DEBUG
						FILE * fp = fopen(newObjectName, "w+b");

						if (fp)
						{
							strcat(myObject, "}");
							fwrite(myObject, strlen(myObject), 1, fp);
							fclose(fp);
						}
#else
						int handle = open(newObjectName, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND);
						if (handle != -1)
						{
							strcat(myObject, "}");
							write(handle, myObject, strlen(myObject));
							close(handle);
						}
#endif
					}

					UtilStrDup(&myObject, NULL);
					UtilStrDup(&newObjectName, NULL);
				}
				IRIS_DeallocateStringValue(tableValue);
			}
			my_free(objectData);
		}
	}

	// Lose the parameters and function name
	IRIS_StackPop(3);

	// Push the number of "string:value" elements added to the new object excluding the type and group that are added automatically
	IRIS_StackPush(ltoa(count, temp, 10));
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()OBJECTS_CHECK
//
// DESCRIPTION:	Examine all the objects for authenticity and remove if not
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __objects_check(void)
{
	int i;
	uint length;
	char fileName[150];
	char oldFileName[100];
	char * onlyOne = IRIS_StackGet(0);
	char * sk = IRIS_StackGet(1);
	char * data;
	char * search1 = "TYPE:DISPLAY";
	char * search2 = "TYPE:TEXT_TABLE";
	char * search3 = "TYPE:PROFILE";
	char * search4 = "TYPE:IMAGE";
	char * ptr;
	bool erase = false;
	int count = 0;

	// Initialisation
	DispInit();
	strcpy(fileName, "I:");
	strcpy(oldFileName, "I:");
	lastFaultyFileName[0] = '\0';
	lastFaultyGroup[0] = '\0';

	for (i = dir_get_first(fileName); i == 0; i = dir_get_next(fileName))
	{
#ifdef __VMAC
		VMACLoop();
#endif
		if (onlyOne == NULL || onlyOne[0] != '1')
		{
			if ((++count % 10) == 0) DispText(".", 9999, 0, false, false, false);
		}

		if ((data = IRIS_GetObjectData(fileName, &length)) != NULL)
		{
			if	(	data[0] == '{' && strstr(data, "NAME:") != NULL &&
					((ptr = strstr(data, search1)) != NULL || (ptr = strstr(data, search2)) != NULL || (ptr = strstr(data, search3)) != NULL || (ptr = strstr(data, search4)) != NULL) &&
					(ptr[-1] == ',' || ptr[-1] == '{')
				)
			{
				// If signature does not exist, disacard objects
				if ((ptr = strstr(data, "SIGN:")) != NULL)
				{
					char *ptrtmp;
					char sign[40];
					int myStackIndex = stackIndex;
					stackLevel++;

					// Replace signature with ZEROS keeping it safe.
					memcpy(sign, ptr+5, sizeof(sign));
					memcpy(ptr+5, "0000000000000000000000000000000000000000", sizeof(sign));

					// Encrypt using iRIS secret key
					SecuritySetIV(NULL);
					IRIS_StackPushFunc("()ENC");
					IRIS_StackPush(sk);			// Key location
					IRIS_StackPush(NULL);		// 16 byte double length

					// Calculate SHA-1. Result is the data for encryption
					IRIS_StackPushFunc("()SHA1");
					IRIS_StackPushFunc("()TO_AHEX");
					IRIS_StackPush(data);

#ifndef __UNSIGNED
					// If SHA-1 does match, all OK
					if (memcmp(IRIS_StackGet(0), sign, sizeof(sign))) {
						erase = true;
					}
#endif

					// House keep
					IRIS_StackPop(stackIndex - myStackIndex);
					stackLevel--;
				}
				else erase = true;
			}
				
			// If we are to erase the object, go ahead
			if (erase)
			{
				char * objectGroup;

				// Erase the file
				DebugDisp("wrong sign:%s", fileName);
				_remove(fileName); //TESTING
				if (onlyOne == NULL || onlyOne[0] != '1')
					DispText("*", 9999, 0, false, false, false);
				
				// Keep a record of the last file we erased
				strcpy(lastFaultyFileName, fileName);
				if ((objectGroup = IRIS_GetStringValue(data, length, "GROUP", false)) != NULL)
				{
					if (objectGroup[0] == 0)
						strcpy(lastFaultyGroup, &objectGroup[4]);
					if (objectGroup) IRIS_DeallocateStringValue(objectGroup);
				}

				// House keep
				strcpy(fileName, oldFileName);
			}
			else strcpy(oldFileName, fileName);

			// Lose the object Data. Not needed any longer
			UtilStrDup(&data, NULL);
		}

		if (erase)
		{
			if (onlyOne && onlyOne[0] == '1')
				break;
			erase = false;
		}
	}

	// Lose the parameters and function name
	IRIS_StackPop(3);

	// Return t he result of the authenticity check
	if (lastFaultyFileName[0] == '\0')
		fileName[0] = '\0';
	else
		sprintf(fileName, "%s/%s", lastFaultyGroup, lastFaultyFileName);
	IRIS_StackPush(fileName);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()FAULTY_OBJECT
//
// DESCRIPTION:	Returns the last faulty object name detected
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __faulty_object(void)
{
	// Lose the function name
	IRIS_StackPop(1);

	IRIS_StackPush(lastFaultyFileName[0]?lastFaultyFileName:NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()FAULTY_GROUP
//
// DESCRIPTION:	Returns the last faulty group name detected
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __faulty_group(void)
{
	// Lose the function name
	IRIS_StackPop(1);

	IRIS_StackPush(lastFaultyGroup[0]?lastFaultyGroup:NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DECOMPRESS
//
// DESCRIPTION:	Decompress the data supplied, frees it and returns an uncompressed buffer
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
static unsigned char * decompress(unsigned char * input, unsigned long outSize, unsigned long size)
{
	unsigned char * output;
	z_stream d_stream;
	int err;

	// Allocate an output buffer
	output = my_malloc(outSize+50);	// Extra memory just in case the decompression uses more memory becuase of a fault

	// Initialisation
	memset(&d_stream, 0, sizeof(d_stream));
	d_stream.next_in = input;
	d_stream.next_out = output;
	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;
	d_stream.avail_in = 0;
	err = inflateInit(&d_stream);

	// Process the compressed stream
	while (d_stream.total_in < size)
	{
		d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
		err = inflate(&d_stream, Z_NO_FLUSH);
		if (err == Z_STREAM_END) break;
		if (err != Z_OK) break;
	}

	// Finalise the decompression
	err = inflateEnd(&d_stream);

	// Return the uncompressed output
	return output;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()STORE_OBJECTS
//
// DESCRIPTION:	A privilaged function. It can only be used by the iRIS group
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
static uint getNextObject(char * objects, uint * index, uint objectsLength)
{
	char data;
	unsigned int length = 0;
	int marker = 0;

	for (;(*index+length) < objectsLength;)
	{
		// Identify the byte we are examining
		data = objects[*index+length];

		// Detect the start of the object
		if (marker == 0 && data != '{')
			(*index)++;

		// If we detect the start of an object, increment the object marker
		if (data == '{')
			marker++;

		// If the marker is detected, start accounting for the json object
		if (marker)
			length++;

		// If we detect the end of an object, decrement the marker
		if (data == '}')
		{
			marker--;
			if (marker < 0)
			{
				// Incorrectly formatted JSON object. Found end of object without finding beginning of object
				return 0;
			}
			if (marker == 0)
				break;
		}
	}

	if (length)
		return length;

	return 0;
}

void __store_objects(void)
{
	uint index;
	uint count = 0;
	char temp[10];
	char * objects = IRIS_StackGet(0);
	char * sk = IRIS_StackGet(1);
	uint length;
	unsigned long size, outSize;
	char * objects_hex;

	// Handle null errors
	if (!sk || !objects || strcmp(currentObjectGroup, irisGroup))
	{
		IRIS_StackPop(3);
		IRIS_StackPush(NULL);
		return;
	}

/*	{
		char keycode;
		char tempBuf[2000];
		sprintf(tempBuf, "size => %d ", strlen(objects));
		write_at(tempBuf, strlen(tempBuf), 1, 1);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
	// Convert from string to hex
	size = strlen(objects)/2;
	objects_hex = my_calloc(size + 1);
	UtilStringToHex(objects, strlen(objects), (unsigned char *) objects_hex);
	IRIS_StackPop(1);
	objects = objects_hex + 8;
	size -= 8;
	outSize = atol(objects_hex);

	// Decompress the objects first
	objects = (char *) decompress((unsigned char *) objects, outSize, size);
	my_free(objects_hex);

	// Get one object at a time
	for (index = 0; (length = getNextObject(objects, &index, strlen(objects))) != 0; index += length, count++)
	{
		// Get the object type
		char * type = IRIS_GetStringValue(&objects[index], length, "TYPE", false);

		// If type does not exist, reject the object
		if (type == NULL)
			continue;

		// If type is a display object, then authenticate the signature of the file
		if (strcmp(&type[4], "DISPLAY") == 0 || strcmp(&type[4], "TEXT TABLE") == 0 || strcmp(&type[4], "IMAGE") == 0 || strcmp(&type[4], "PROFILE") == 0)
		{
			char sign[40];
			char * ptr;

			// Store character after object in a safe place. We are about to change it to NULL to manipulate this a sa string.
			char keep = objects[index+length];

			// Change object to a null terminated string.
			objects[index+length] = '\0';

/*	{
		char keycode;
		char tempBuf[2000];
		sprintf(tempBuf, "step 1 ");
		write_at(tempBuf, strlen(tempBuf), 1, 1);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
			// If signature does not exist, disacard objects
			if ((ptr = strstr(&objects[index], "SIGN:")) != NULL)
			{
				int myStackIndex = stackIndex;
				stackLevel++;

/*	{
		char keycode;
		char tempBuf[2000];
		sprintf(tempBuf, "step 2 ");
		write_at(tempBuf, strlen(tempBuf), 1, 1);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
				// Replace signature with ZEROS keeping it safe.
				memcpy(sign, ptr+5, sizeof(sign));
				memcpy(ptr+5, "0000000000000000000000000000000000000000", sizeof(sign));

				// Encrypt using iRIS secret key
				SecuritySetIV(NULL);
				IRIS_StackPushFunc("()ENC");
				IRIS_StackPush(sk);			// Key location
				IRIS_StackPush(NULL);		// 16 byte double length

				// Calculate SHA-1. Result is the data for encryption
				IRIS_StackPushFunc("()SHA1");
				IRIS_StackPushFunc("()TO_AHEX");
				IRIS_StackPush(&objects[index]);

/*	{
		char keycode;
		char tempBuf[2000];
		sprintf(tempBuf, "step 3 %X ", IRIS_StackGet(0));
		write_at(tempBuf, strlen(tempBuf), 1, 1);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
				// If SHA-1 does not match
#ifndef __UNSIGNED
				if (memcmp(IRIS_StackGet(0), sign, sizeof(sign)) == 0)
#endif
				{
					// Reinsert the correct sign in the object
					memcpy(ptr+5, sign, sizeof(sign));

					// Store the object
					IRIS_PutObjectData(&objects[index], length);
				}

				// House keep
				IRIS_StackPop(stackIndex - myStackIndex);
				stackLevel--;
			}

			// Restore the keep character
			objects[index+length] = keep;
		}

		// If this is a configuration or data object, store it without further authentication. It is not executable.
		// An IMAGE to print that is not signed should be of TYPE:DATA.
		else if (strncmp(&type[4], "CONFIG", 6) == 0 || strcmp(&type[4], "DATA") == 0)
		{
			// Store the object
			IRIS_PutObjectData(&objects[index], length);
		}

		// If this is an EXPIRY command, then lose the object or group of objects
		else if (strcmp(&type[4], "EXPIRE") == 0)
		{
			char * objectName;
			char * groupName, * groupValue, * groupType;

			// Get the object name then remove it from the terminal
			if ((objectName = IRIS_GetStringValue(&objects[index], length, "NAME", false)) != NULL)
			{
				if (objectName[0] == '\0')
					_remove(&objectName[4]);
				IRIS_DeallocateStringValue(objectName);
			}

			// Get the group name, then remove all objects of that belong to the specified group
			if ((groupName = IRIS_GetStringValue(&objects[index], length, "GROUPNAME", false)) != NULL)
			{
				if (groupName[0] == '\0' && (groupValue = IRIS_GetStringValue(&objects[index], length, "GROUPVALUE", false)) != NULL)
				{
					if (groupValue[0] == '\0')
					{
						int i, j;
						char fileName[50];
						char oldFileName[50];
						char * data;
						uint len;
						char * ptr;

						groupType = IRIS_GetStringValue(&objects[index], length, "GROUPTYPE", false);

						for (j = 0; j < 2; j++)
						{
							strcpy(fileName, j?"I:":"F:");
							strcpy(oldFileName, j?"I:":"F:");
							for (i = dir_get_first(fileName); i == 0; i = dir_get_next(fileName))
							{
								if ((data = IRIS_GetObjectData(fileName, &len)) != NULL)
								{
									if (data[0] == '{' && data[len-1] == '}' && (ptr = strstr(data, &groupName[4])) != NULL &&
										(ptr[-1] == ',' || ptr[-1] == '{') && strncmp(&ptr[strlen(&groupName[4])+1], &groupValue[4], strlen(&groupValue[4])) == 0 &&
										(groupType == NULL || groupType[0] != '\0' || ((ptr = strstr(data, "TYPE:")) != NULL &&
										(ptr[-1] == ',' || ptr[-1] == '{') && strncmp(&ptr[5], &groupType[4], strlen(&groupType[4])) == 0)))
									{
										_remove(fileName);
										strcpy(fileName, oldFileName);
									}
									else strcpy(oldFileName, fileName);
									my_free(data);
								}
							}
						}

						if (groupType) IRIS_DeallocateStringValue(groupType);
					}

					IRIS_DeallocateStringValue(groupValue);
				}

				IRIS_DeallocateStringValue(groupName);
			}
		}

		// If this is an SHUTDOWN command, then reboot the terminal
		else if (strcmp(&type[4], "SHUTDOWN") == 0)
#ifdef _DEBUG
			exit(0);
#else
		{
			__tcp_disconnect_do();
			SVC_WAIT(500);
			SVC_RESTART("");
		}
#endif

		// If this an environment variable setting, then set the environment variable
		else if (strcmp(&type[4], "ENV") == 0)
		{
			char * key;
			char * value;
			char * gid;
			int gid_orig = -1;

			// Get the file name
			if ((key = IRIS_GetStringValue(&objects[index], length, "KEY", false)) != NULL)
			{
				// Get the file data and create or override the file
				if (key[0] == '\0' && (value = IRIS_GetStringValue(&objects[index], length, "VALUE", false)) != NULL)
				{
					if (value[0] == '\0')
					{
						if ((gid = IRIS_GetStringValue(&objects[index], length, "GID", false)) != NULL)
						{
#ifndef _DEBUG
							if (gid[0] == '\0')
							{
								gid_orig = get_group();
								set_group(atoi(&gid[4]));
							}
#endif
							IRIS_DeallocateStringValue(gid);
						}
#ifndef _DEBUG
						put_env(&key[4], &value[4], strlen(&value[4]));
						if (gid_orig != -1)
							set_group(gid_orig);
#endif
					}

					IRIS_DeallocateStringValue(value);
				}

				IRIS_DeallocateStringValue(key);
			}
		}

		// If this is an binary file upgrade, then store store or override the file
		else if (strcmp(&type[4], "FILE") == 0)
		{
			char * fileName;
			char * fileData;

			// Get the file name
			if ((fileName = IRIS_GetStringValue(&objects[index], length, "NAME", false)) != NULL)
			{
				// Get the file data and create or override the file
				if (fileName[0] == '\0' && (fileData = IRIS_GetStringValue(&objects[index], length, "DATA", false)) != NULL)
				{
#ifdef _DEBUG
					FILE * fp;
#else
					int handle;
#endif
					char * hexData;
					unsigned int hexSize;

					if (fileData[0] == '\0')
					{
						// Convert the file data to hex and store it in the file
						hexSize = strlen(&fileData[4]) / 2 + 1;
						hexData = my_malloc(hexSize);
						hexSize = UtilStringToHex(&fileData[4], strlen(&fileData[4]), (uchar *) hexData);

#ifdef _DEBUG
						// Get a file pointer to store the new file
						if ((fp = fopen(&fileName[4], "w+b")) != NULL)
						{
							fwrite(hexData, 1, hexSize, fp);
							fclose(fp);
						}
#else
						if ((handle = open(&fileName[4], O_CREAT | O_TRUNC | O_WRONLY | O_APPEND | O_CODEFILE)) != -1)
						{
							write(handle, hexData, hexSize);
							close(handle);
						}
#endif

						my_free(hexData);
					}

					IRIS_DeallocateStringValue(fileData);
				}

				IRIS_DeallocateStringValue(fileName);
			}
		}

		// If this is a binary file upgrade, then store store or override the file
		else if (strcmp(&type[4], "MERGE") == 0)
		{
			char * fileName;
			char * fileMax;

			// Get the proper file name
			if ((fileName = IRIS_GetStringValue(&objects[index], length, "NAME", false)) != NULL)
			{
				uint len;

				// Make sure it is a simple file name value
				if (fileName[0] == '\0' && (fileMax = IRIS_GetStringValue(&objects[index], length, "MAX", false)) != NULL)
				{
					int i;
#ifdef _DEBUG
					FILE * wrfp;
					
					wrfp = fopen(&fileName[4], "w+b");

					// Join the files together
					for (i = 0; wrfp; i++)
					{
						FILE * fp;
						char temp[50];
						unsigned char * data;
						sprintf(temp, "%s_%d", &fileName[4], i);

						// Read the partial file
						if (i == atoi(&fileMax[4]) || (fp = fopen(temp, "rb")) == NULL)
						{
							fclose(wrfp);
							break;
						}

						// Get the file size
						fseek(fp, 0, SEEK_END);
						len = ftell(fp);
						fseek(fp, 0, SEEK_SET);

						// Read the partial file
						data = my_malloc(len);
						fread(data, 1, len, fp);
						fclose(fp);
						_remove(temp);

						fwrite(data, 1, len, wrfp);
						my_free(data);
					}
#else
					int whandle;

					// We must have all the files or we do not merge
					for (i = 0; fileMax[0] == '\0' && i < atoi(&fileMax[4]); i++)
					{
						char temp[50];
						sprintf(temp, "%s_%d", &fileName[4], i);
						if (dir_get_file_sz(temp) == -1)
							break;
					}

					if (i == atoi(&fileMax[4]))
					{
						whandle = open(&fileName[4], O_CREAT | O_TRUNC | O_WRONLY | O_APPEND | O_CODEFILE);

						// Join the files together
						for (i = 0; whandle != -1; i++)
						{
							int handle;
							char temp[50];
							unsigned char * data;
							sprintf(temp, "%s_%d", &fileName[4], i);

							// Read the partial file
							if (i == atoi(&fileMax[4]) || (handle = open(temp, O_RDONLY)) == -1)
							{
								close(whandle);
								break;
							}

							// Get the file size
							len = dir_get_file_sz(temp);

							// Read the partial file
							data = my_malloc(len);
							read(handle, (char *) data, len);
							close(handle);
							_remove(temp);

							write(whandle, (char *) data, len);
							my_free(data);
						}
					}
#endif
					IRIS_DeallocateStringValue(fileMax);
				}

				IRIS_DeallocateStringValue(fileName);
			}
		}

		// House keep
		IRIS_DeallocateStringValue(type);
	}

	my_free(objects);
//	my_free(objects_hex);
	IRIS_StackPop(2);
	sprintf(temp, "%d", count);
	IRIS_StackPush(temp);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DOWNLOAD_REQ
//
// DESCRIPTION:	Requests a download of a resource (file) from the iRIS host
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __download_req(void)
{
	char * request;
	char * fileName = IRIS_StackGet(0);

	// Prepare the filename download request
	request = my_malloc(strlen(fileName) + 70);
	sprintf(request, "{TYPE:GETFILE,NAME:%s}", fileName);

	// Lose the current stack. No longer required.
	IRIS_StackPop(2);

	// Add it to the upload
	IRIS_AppendToUpload(request);
	my_free(request);

	// Push a dummy return value back on the stack
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()UPLOAD_MSG
//
// DESCRIPTION:	Requests an upload of a resource to the iRIS host
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __upload_msg(void)
{
	char * message = IRIS_StackGet(0);

	// Add it to the upload
	IRIS_AppendToUpload(message);

	// Lose the current stack. No longer required.
	IRIS_StackPop(2);

	// Push a dummy return value back on the stack
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()UPLOAD_OBJ
//
// DESCRIPTION:	Adds an object to the upload message
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __upload_obj(void)
{
	char * objectName = IRIS_StackGet(0);
	char * objectGroup;
	uint objectLength;
	char * objectData;

	// Make sure the object name is available
	if (objectName)
	{
		// Get the object data
		if ((objectData = IRIS_GetObjectData(objectName, &objectLength)) != NULL)
		{
			// Obtain the group information
			objectGroup = IRIS_GetStringValue(objectData, objectLength, "GROUP", false);

			// The object must have a simple value group for restricted access. If no group, my_free access.
			if (!objectGroup || (objectGroup[0] == 0 && (objectGroup[4] == '\0' || strcmp(&objectGroup[4], currentObjectGroup) == 0 || strcmp(currentObjectGroup, irisGroup) == 0)))
				// Add it to the upload
				IRIS_AppendToUpload(objectData);

			// Lose the group information. Not needed any longer
			if (objectGroup) IRIS_DeallocateStringValue(objectGroup);

			// Lose the object Data. Not needed any longer
			UtilStrDup(&objectData, NULL);
		}
	}

	// Lose the function name and object name
	IRIS_StackPop(2);

	// Push a dummy return value back on the stack
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DOWNLOAD_OBJ
//
// DESCRIPTION:	Download an object from the host if not already available
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __download_obj(void)
{
	char * objectName = IRIS_StackGet(0);

	// Use the system utility to do so.
	IRIS_DownloadResourceObject(objectName);

	// Lose the function name and object name
	IRIS_StackPop(2);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()REMOTE
//
// DESCRIPTION:	Initiate a remote session to upload and download
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __remote(void)
{
	// Use the system utility to do so.
	IRIS_GetExternalObjectData(NULL);

	// Lose the function name
	IRIS_StackPop(1);
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PREV_OBJECT
//
// DESCRIPTION:	Returns the previous display object
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __prev_object(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(prevObject);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()NEXT_OBJECT
//
// DESCRIPTION:	Returns the next display object
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __next_object(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(nextObject);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CURR_OBJECT
//
// DESCRIPTION:	Returns the current display object
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __curr_object(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(currentObject);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CURR_GROUP
//
// DESCRIPTION:	Returns the current display object version
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __curr_group(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(currentObjectGroup);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CURR_VERSION
//
// DESCRIPTION:	Returns the current display object version
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __curr_version(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(currentObjectVersion);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CURR_EVENT
//
// DESCRIPTION:	Returns the current display object version
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __curr_event(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(currentEvent);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CURR_EVENT_VALUE
//
// DESCRIPTION:	Returns the current display object version
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __curr_event_value(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(currentEventValue);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()NEW_EVENT_VALUE
//
// DESCRIPTION:	Returns the current display object version
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __new_event_value(void)
{
	IRIS_StackPop(1);
	IRIS_StackPush(newEventValue);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CLR_TMP
//
// DESCRIPTION:	Returns the current display object version
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __clr_tmp(void)
{
	int count;
	char temp[50];
	char * objectName = IRIS_StackGet(0);


	count = IRIS_ClrTemp((objectName && objectName[0])? objectName:currentObject);

	IRIS_StackPop(2);
	IRIS_StackPush(ltoa(count, temp, 10));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()REARM
//
// DESCRIPTION:	Returns the current display object version
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __rearm(void)
{
	IRIS_StackPop(1);
	displayTimeoutMultiplier = displayTimeoutMultiplierFull;
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()VMAC_APP
//
// DESCRIPTION:	Attempts to activate the VMAC application using VMAC
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __vmac_app(void)
{
	char * event = IRIS_StackGet(0);
	char * appName = IRIS_StackGet(1);

#ifdef __VMAC
	VMACHotKey(appName, atoi(event));
//	EESL_send_event(appName, atoi(event), NULL, 0);
//	VMACInactive();
//	VMACDeactivate();
//	SVC_WAIT(3000);
#endif

	IRIS_StackPop(3);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()BATTERY_STATUS
//
// DESCRIPTION:	Returns the current battery status
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __battery_status(void)
{
#ifndef __VX670
	IRIS_StackPop(1);
	IRIS_StackPush("OK");
#else
	int status = get_battery_sts();

	IRIS_StackPop(1);
	IRIS_StackPush(status <= 0?"NOK":"OK");	// If battery level is critical, report that
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DOCK_STATUS
//
// DESCRIPTION:	Returns the current battery status
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __dock_status(void)
{
#ifndef __VX670
	IRIS_StackPop(1);
	IRIS_StackPush("OK");
#else
	int dock = get_dock_sts();

	IRIS_StackPop(1);
	IRIS_StackPush(dock != 0?"NOK":"OK");
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()ENV_GET
//
// DESCRIPTION:	Returns the environment variable value
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __env_get(void)
{
	char * var = IRIS_StackGet(0);
	char value[100];

	if (var)
	{
		int n = get_env(var, value, sizeof(value));
		value[n] = '\0';
		IRIS_StackPop(2);
		IRIS_StackPush(value);
	}
	else
	{
		IRIS_StackPop(2);
		IRIS_StackPush(NULL);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()ENV_PUT
//
// DESCRIPTION:	Updates an environment variable value
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __env_put(void)
{
	char * value = IRIS_StackGet(1);
	char * var = IRIS_StackGet(0);

	if (var && value)
		put_env(var, value, strlen(value));

	IRIS_StackPop(3);
	IRIS_StackPush(value);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()FORCE_NEXT_OBJECT
//
// DESCRIPTION:	Override the next object at the end of an event.
//
// PARAMETERS:	Next Object Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __force_next_object(void)
{
	char * nextObjectName = IRIS_StackGet(0);

	if (nextObjectName)
		UtilStrDup(&forceNextObject, nextObjectName);

	IRIS_StackPop(2);
	IRIS_StackPush(NULL);
}

