//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iris2805.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports AS2805 functions
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
#include "AS2805.h"
#include "security.h"
#include "utility.h"
#include "iris.h"
#include "irisfunc.h"

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
int AS2805_Error = -1;
static uchar iv_ofb[8] = "\x01\x23\x45\x67\x89\xAB\xCD\xEF";

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// AS2805 FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_GET
//
// DESCRIPTION:	Returns the current AS2805 request stream if it is still available
//				(Used for MACing while stream ios getting built
//
// PARAMETERS:	None
//
// RETURNS:		The AS2805 request in ASCII hex
//-------------------------------------------------------------------------------------------
//
void __as2805_get(void)
{
	uint length;
	char * string;
	uchar * request = AS2805Position(&length);

	// Release function name from stack
	IRIS_StackPop(1);

	// If AS2805 request not available
	if (length == 0)
		IRIS_StackPush(NULL);

	// AS2805 available. Send it back
	else
	{
		// Convert the request buffer to ASCII Hex
		string = my_malloc(length * 2 + 1);
		UtilHexToString(request, length, string);

		// return with the request buffer as a string
		IRIS_StackPush(string);
		my_free(string);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_ERR
//
// DESCRIPTION:	Returns the current AS2805 field number that caused an error during a CHK operation
//
// PARAMETERS:	None
//
// RETURNS:		AS2805 field number
//-------------------------------------------------------------------------------------------
//
void __as2805_err(void)
{
	char temp[10];

	// Release function name from stack
	IRIS_StackPop(1);

	sprintf(temp, "%d", AS2805_Error);
	IRIS_StackPush(temp);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_OFB
//
// DESCRIPTION:	OFB encrypts an AS2805 stream according to AS2805 standard
//				This function assumes a 3DES key
//
// PARAMETERS:	key		<=	The key used to OFB the AS2805 stream
//				request	<=	The AS2805 request iono ASCII hex string
//
// RETURNS:		The request OFB'd
//-------------------------------------------------------------------------------------------
//
void __as2805_ofb(void)
{
	uchar * duplicate;
	char * request = IRIS_StackGet(0);	// This is the request in ASCII hex string
	char * key = IRIS_StackGet(1);
	uchar * hex;
	int length;

	// Make sure we have a buffer request to process.
	if (!key || !request || strlen(request) == 0)
	{
		IRIS_StackPop(3);
		IRIS_StackPush(NULL);
		return;
	}

	// Convert the AS2805 data back to a hex stream
	hex = my_malloc(strlen(request)/2);
	length = UtilStringToHex(request, strlen(request), hex);

	// Create a duplicate of the stream
	duplicate = my_malloc(length);
	memcpy(duplicate, hex, length);

	// OFB encrypt the result using the key supplied
	SecuritySetIV(iv_ofb);
	SecurityCrypt(currentObjectGroup, (uchar) atoi(key), 16, length, hex, false, true);

	// Restore the clear field from the duplicate
	AS2805OFBAdjust(duplicate, hex, length);

	// Release the duplicate copy
	my_free(duplicate);

	// Convert the hex stream to ASCII Hex
	request = my_malloc(length * 2 + 1);
	UtilHexToString(hex, length, request);

	// We got our message now, release internal buffers
	AS2805Close();

	// return with the request buffer as a string
	IRIS_StackPop(3);
	IRIS_StackPush(request);
	my_free(request);
	my_free(hex);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_MAKE
//
// DESCRIPTION:	Creates an AS2805 stream
//
// PARAMETERS:	arrayOfArrays	<=	An simple or complex value that needs to be resolved to a simple value
//									The simple value must finally resolve to an array of arrays. Each array
//									consists of [element {= AS2805 field Number}, element {= AS2805 field value}]
//
//									Each element within the array can be complex or simple again
//
// RETURNS:		The request
//-------------------------------------------------------------------------------------------
//
void __as2805_make(void)
{
	int i;
	uint length = 0;
	uchar * request;
	char * string;
	char ** arrayOfArrays = (char **) atol(IRIS_StackGet(0));		// This is potentially dangerous if a normal striong was pushed onto the stack
	char * paramHealthCheck = IRIS_StackGet(1);

	// Make sure we were passed a good pointer
	if (!arrayOfArrays || !paramHealthCheck || strcmp(paramHealthCheck, "POINTER"))
	{
		IRIS_StackPop(3);
		IRIS_StackPush(NULL);		// An empty request.
		return;
	}

	// Lose the parameters and function name.	
	IRIS_StackPop(3);

	// Increse the stack level to ensure our values are not consumed by lower functions in the stack
	stackLevel++;

	// Set the AS2805 error to none
	AS2805_Error = -1;

	// Resolve element to a single value string
	if ((*arrayOfArrays)[0] == 1)
	{
		// Initialise the AS2805 buffer with maximum of 1000 bytes.....This should be plenty.
		request = AS2805Init(1000);

		// For each array within the main array
		for (;*arrayOfArrays; arrayOfArrays++)
		{
			char ** internalArray = (char **) &((*arrayOfArrays)[4]);
			uchar fieldNum;
			char * fieldValue;

			// We are not expecting simple values but arrays, so ignore these ones....as comment perhaps for now
			if ((*arrayOfArrays)[0] == 0)
				continue;

			// Process the first two values within each internal array
			for (i = 0; i < 2 && *internalArray; internalArray++, i++)
			{
				int myStackIndex = stackIndex;

				// Resolve it to a simple value as required
				IRIS_ResolveToSingleValue(*internalArray, false);

				// Check that we have a value. If not, skip this field assignment and move on to the next field
				if (myStackIndex == stackIndex)
					break;

				// If any of the array values are NULL (not available), then discard the entry
				if (IRIS_StackGet(0) == NULL)
				{
					IRIS_StackPop(stackIndex - myStackIndex);
					break;
				}

				switch(i)
				{
					// Field number
					case 0:
						// Get the field number
						fieldNum = atoi(IRIS_StackGet(0));

						// If the field number is 64 or 128 (MAC location), then set the bit to ensure MAC calculation is OK.
						if (fieldNum == 64 || fieldNum == 128)
							AS2805SetBit(fieldNum);

						break;

					// The field value
					case 1:
						// Get the field value
						fieldValue = IRIS_StackGet(0);

						// Pack the value
						length = AS2805Pack(fieldNum, fieldValue);

						break;
				}

				// Only interested in the top vlue. Lose others that could be pushed in error by the application
				IRIS_StackPop(stackIndex - myStackIndex);
			}
		}
	}

	stackLevel--;

	// Handle an empty message request situation
	if (length == 0)
	{
		IRIS_StackPush(NULL);
		return;
	}

	// Convert the request buffer to ASCII Hex
	string = my_malloc(length * 2 + 1);
	UtilHexToString(request, length, string);

	// We got our message now, release internal buffers
	AS2805Close();

	// return with the request buffer as a string
	IRIS_StackPush(string);
	my_free(string);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_MAKE_CUSTOM
//
// DESCRIPTION:	Creates a custom AS2805 stream - no bitmaps
//
// PARAMETERS:	element	<=	An simple of complex element that needs to be resolved to a simple element
//							The simple element must finally resolve to an array of arrays. Each array
//							consists of [element {= AS2805 field Number}, element {= AS2805 field value}]
//
//							Each element within the array can be complex or simple again
//
// RETURNS:		The request
//-------------------------------------------------------------------------------------------
//
void __as2805_make_custom(void)
{
	int i;
	uint length = 0;
	uchar * request = NULL;
	char * string;
	char ** arrayOfArrays = (char **) atol(IRIS_StackGet(0));		// This is potentially dangerous if a normal string was pushed onto the stack
	char * paramHealthCheck = IRIS_StackGet(1);

	// Make sure we were passed a good pointer
	if (!arrayOfArrays || !paramHealthCheck || strcmp(paramHealthCheck, "POINTER"))
	{
		IRIS_StackPop(3);
		IRIS_StackPush(NULL);		// An empty request.
		return;
	}

	// Lose the parameters and function name.	
	IRIS_StackPop(3);

	// Increse the stack level to ensure our values are not consumed by lower functions in the stack
	stackLevel++;

	// Set the AS2805 error to none
	AS2805_Error = -1;

	// Resolve element to a single value string
	if ((*arrayOfArrays)[0] == 1)
	{
		char ** loopArray = NULL;

		// Initialise the custom AS2805 buffer with maximum of 1000 bytes.....This should be plenty.
		request = my_malloc(1000);

		// For each array within the main array
		for (;*arrayOfArrays; arrayOfArrays++)
		{
			char ** internalArray = (char **) &((*arrayOfArrays)[4]);
			uchar formatType;
			uint fieldSize;
			char * fieldValue;

			// We are not expecting simple values but arrays, so ignore these ones....as comment perhaps for now
			if ((*arrayOfArrays)[0] == 0)
				continue;

			// Process the first three values within each internal array
			for (i = 0; i < 3 && *internalArray; internalArray++, i++)
			{
				int myStackIndex = stackIndex;
				char * format;

				// Resolve it to a simple value as required
				IRIS_ResolveToSingleValue(*internalArray, false);

				// Check that we have a value. If not, skip this field assignment and move on to the next field
				if (myStackIndex == stackIndex)
					break;

				// If any of the array values are NULL (not available), then discard the entry
				if (IRIS_StackGet(0) == NULL)
				{
					IRIS_StackPop(stackIndex - myStackIndex);
					break;
				}

				switch(i)
				{
					// Format character
					case 0:
						// Get the field number
						format = IRIS_StackGet(0);

						if (strcmp(format, "LOOP") == 0) formatType = C_LOOP, loopArray = arrayOfArrays;
						else if (strcmp(format, "ENDLOOP") == 0) formatType = C_END_LOOP; 

						else if (strcmp(format, "LLNVAR") == 0) formatType = C_LLNVAR;
						else if (strcmp(format, "LLAVAR") == 0) formatType = C_LLAVAR;
						else if (strcmp(format, "LLLNVAR") == 0) formatType = C_LLLNVAR;
						else if (strcmp(format, "LLLVAR") == 0) formatType = C_LLLVAR;

						else if (strcmp(format, "MMDDhhmmss") == 0) formatType = C_MMDDhhmmss;
						else if (strcmp(format, "hhmmss") == 0) formatType = C_hhmmss;
						else if (strcmp(format, "YYMMDD") == 0) formatType = C_YYMMDD;
						else if (strcmp(format, "YYMM") == 0) formatType = C_YYMM;
						else if (strcmp(format, "MMDD") == 0) formatType = C_MMDD;

						else if (strncmp(format, "an", 2) == 0) formatType = C_STRING;
						else if (strcmp(format, "ns") == 0) formatType = C_STRING;

						else if (strcmp(format, "x+n") == 0) formatType = C_AMOUNT;
						else if (format[0] == 'z') formatType = C_LLNVAR;
						else if (format[0] == 'n') formatType = C_BCD;
						else if (format[0] == 'N') formatType = C_BCD_LINK;
						else if (format[0] == 'b') formatType = C_BITMAP;
						else i = 3;
						break;

					// Field size
					case 1:
						// Get the field number
						if (formatType != C_LOOP && formatType != C_END_LOOP)
							fieldSize = atoi(IRIS_StackGet(0));
						break;

					// The field value
					case 2:
						// If we reached the end of the loop and the single value = LOOP, go back to the beginning of the loop
						if (formatType == C_END_LOOP && strcmp(IRIS_StackGet(0), "LOOP") == 0 && loopArray)
							arrayOfArrays = loopArray;
						// Get the field value
						else
							fieldValue = IRIS_StackGet(0);

						// Pack the value
						AS2805BufferPack(fieldValue, formatType, fieldSize, request, &length);

						IRIS_StackPop(stackIndex - myStackIndex);	// Only interested in the top vlue. Lose others that could be pushed in error by the application
						break;
				}

				// Only interested in the top value. Lose others that could be pushed in error by the application
				IRIS_StackPop(stackIndex - myStackIndex);
			}
		}
	}

	stackLevel--;

	// Handle an empty message request situation
	if (length == 0)
	{
		IRIS_StackPush(NULL);
		return;
	}

	// Convert the request buffer to ASCII Hex
	string = my_malloc(length * 2 + 1);
	UtilHexToString(request, length, string);
	if (request) my_free(request);

	// We got our message now, release internal buffers
	AS2805Close();

	// return with the request buffer as a string
	IRIS_StackPush(string);
	my_free(string);
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_BREAK
//
// DESCRIPTION:	Creates an AS2805 stream
//
// PARAMETERS:	arrayOfArrays	<=	An simple or complex value that needs to be resolved to a simple value
//									The simple value must finally resolve to an array of arrays. Each array
//									consists of [element {= AS2805 field Number}, element {= AS2805 field value}]
//
//									Each element within the array can be complex or simple again
//
// RETURNS:		The request
//-------------------------------------------------------------------------------------------
//
void __as2805_break(void)
{
	int i;
	uint length;
	char temp[10];
	uchar * response;
	char ** arrayOfArrays = (char **) atol(IRIS_StackGet(0));		// This is potentially dangerous if a normal string was pushed onto the stack
	char * paramHealthCheck = IRIS_StackGet(1);
	char * data = IRIS_StackGet(2);

	// Make sure we were passed a good pointer
	if (!data || !arrayOfArrays || !paramHealthCheck || strcmp(paramHealthCheck, "POINTER"))
	{
		IRIS_StackPop(4);
		IRIS_StackPush(NULL);		// An empty request.
		return;
	}

	// Convert the response to hex
	length = strlen(data)/2;
	response = my_malloc(length + 200);		// To cover ill-formatted messages
	UtilStringToHex(data, strlen(data), response);

	// Lose the parameters and function name.	
	IRIS_StackPop(4);

	// Increse the stack level to ensure our values are not consumed by lower functions in the stack
	stackLevel++;

	// Set the AS2805 error to none
	AS2805_Error = -1;

	// Resolve element to a single value string
	if ((*arrayOfArrays)[0] == 1)
	{
		// For each array within the main array
		for (;*arrayOfArrays && AS2805_Error == -1; arrayOfArrays++)
		{
			char fullName[100];
			char ** internalArray = (char **) &((*arrayOfArrays)[4]);
			int operation;
			uchar fieldNum;
			char * fieldValue;

			// We are not expecting simple values but arrays, so ignore these ones....as comment perhaps for now
			if ((*arrayOfArrays)[0] == 0)
				continue;

			// Process the first three values within each internal array
			for (i = 0;i < 3 && *internalArray && AS2805_Error == -1; internalArray++, i++)
			{
				char * value = &(*internalArray)[4];
				int myStackIndex = stackIndex;

				// Resolve it to a simple value as required
				IRIS_ResolveToSingleValue(*internalArray, false);

				if (i != 2 || operation == C_CHK)
				{
					// Check that we have a value. If not, skip this field assignment and move on to the next field
					if (myStackIndex == stackIndex || IRIS_StackGet(0) == NULL)
						break;
				}

				switch(i)
				{
					// Operation 
					case 0:
						if (strcmp(IRIS_StackGet(0), "CHK") == 0) operation = C_CHK;
						else if (strcmp(IRIS_StackGet(0), "GET") == 0) operation = C_GET;
						else if (strcmp(IRIS_StackGet(0), "GETS") == 0) operation = C_GETS;
						else if (strcmp(IRIS_StackGet(0), "ADD") == 0) operation = C_ADD;
						else if (strcmp(IRIS_StackGet(0), "IGN") == 0) operation = C_IGN;
						else i = 3;
						break;

					// Field number
					case 1:
						// Get the field number
						fieldNum = atoi(IRIS_StackGet(0));
						break;

					// The field value
					case 2:
						// Allocate enough space for the field value
						fieldValue = my_malloc(2000);
						fieldValue[0] = '\0';

						// Unpack the value
						AS2805Unpack(fieldNum, fieldValue, response, length);

/*	{
		char keycode;
		char tempBuf[500];
		sprintf(tempBuf, "break = value:%.10s num:%d op:%d len:%d  ", fieldValue, fieldNum, operation, length);
		write_at(tempBuf, strlen(tempBuf), 1, 1);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
						if (operation == C_CHK)
						{
							long num1;
							long num2;

							// If not already available, then it is not valid
							if (IRIS_StackGet(0) == NULL)
								AS2805_Error = fieldNum;
							else
							{
								// Get the numbers if they can be converted
								num1 = UtilStringToNumber(IRIS_StackGet(0));
								num2 = UtilStringToNumber(fieldValue);

								if (num1 != -1 && num2 != -1)
								{
									if (num1 != num2)
										AS2805_Error = fieldNum;
								}
								else if (strcmp(IRIS_StackGet(0), fieldValue))
									AS2805_Error = fieldNum;
							}
						}
						else if ((operation == C_GET || operation == C_GETS || operation == C_ADD) && value[0] != '\0')
						{
							long num;

							// Store the unpacked value in temporary data area or object file
							IRIS_FullName(value, fullName);

							// If we are adding, get the value of the existing one
							if (operation == C_ADD)
							{
								unsigned long result =  atol(fieldValue);

								// Get the current value
								IRIS_Eval(value, false);

								// Add it to the new value
								if (IRIS_StackGet(0))
									result += atol(IRIS_StackGet(0));

								// Lose the current value from the stack
								IRIS_StackPop(1);

								// Prepare the new value for unpacking
								sprintf(fieldValue, "%ld", result);
							}

							// If this is a pure number, try and lose the leading zeros (take care of the leading '-' sign as well
							else if (operation == C_GET && (num = UtilStringToNumber(fieldValue)) != -1)
								sprintf(fieldValue, "%ld", num);

							// Store the data
							IRIS_StoreData(fullName, fieldValue, false);
						}

/*	{
		char keycode;
		char tempBuf[500];
		sprintf(tempBuf, "break = AS2805 Error = %d  ", AS2805_Error);
		write_at(tempBuf, strlen(tempBuf), 1, 1);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
						my_free(fieldValue);

						break;
				}

				// Only interested in the top vlue. Lose others that could be pushed in error by the application
				IRIS_StackPop(stackIndex - myStackIndex);
			}
		}
	}

	stackLevel--;

	// We got our message now, release internal buffers
	AS2805Close();
	my_free(response);

	// Return the result. If -1, no error. If a positive number then this is the field number that had the error during a CHK operation
	sprintf(temp, "%d", AS2805_Error);
	IRIS_StackPush(temp);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_BREAK_CUSTOM
//
// DESCRIPTION:	Creates a custom AS2805 stream - no bitmaps
//
// PARAMETERS:	element	<=	An simple of complex element that needs to be resolved to a simple element
//							The simple element must finally resolve to an array of arrays. Each array
//							consists of [element {= AS2805 field Number}, element {= AS2805 field value}]
//
//							Each element within the array can be complex or simple again
//
// RETURNS:		The request
//-------------------------------------------------------------------------------------------
//
void __as2805_break_custom(void)
{
	int i;
	uint respLength;
	uint length = 0;
	char temp[10];
	uchar * response;
	char ** arrayOfArrays = (char **) atol(IRIS_StackGet(0));		// This is potentially dangerous if a normal string was pushed onto the stack
	char * paramHealthCheck = IRIS_StackGet(1);
	char * data = IRIS_StackGet(2);
	bool tooLong = false;

//	debug = 1;

	// Make sure we were passed a good pointer
	if (!data || !arrayOfArrays || !paramHealthCheck || strcmp(paramHealthCheck, "POINTER"))
	{
		IRIS_StackPop(4);
		IRIS_StackPush(NULL);		// An empty request.
		return;
	}

	// Convert the response to hex
	respLength = strlen(data) / 2;
	response = my_malloc(respLength + 200);		// To cover ill formatted messages
	UtilStringToHex(data, strlen(data), response);

	// Lose the parameters and function name.	
	IRIS_StackPop(4);

	// Increse the stack level to ensure our values are not consumed by lower functions in the stack
	stackLevel++;

	// Set the AS2805 error to none
	AS2805_Error = -1;

	// Resolve element to a single value string
	if ((*arrayOfArrays)[0] == 1)
	{
		char ** loopArray = NULL;

		// For each array within the main array
		for (;!tooLong && *arrayOfArrays && AS2805_Error == -1; arrayOfArrays++)
		{
			char fullName[100];
			char ** internalArray = (char **) &((*arrayOfArrays)[4]);
			uchar formatType;
			uint fieldSize;
			int operation;
			char * fieldValue;

			// We are not expecting simple values but arrays, so ignore these ones....as comment perhaps for now
			if ((*arrayOfArrays)[0] == 0)
				continue;

			// Process the first three values within each internal array
			for (i = 0;!tooLong && i < 4 && *internalArray && AS2805_Error == -1; internalArray++, i++)
			{
				char * value = &(*internalArray)[4];
				int myStackIndex = stackIndex;
				char * format;

				// Resolve it to a simple value as required
				IRIS_ResolveToSingleValue(*internalArray, false);


				if (i == 0 || ((i == 1 || i == 2) && (operation != C_LOOP && operation != C_END_LOOP)) || (i == 3 && operation == C_CHK))
				{
					// Check that we have a value. If not, skip this field assignment and move on to the next field
					if (myStackIndex == stackIndex || IRIS_StackGet(0) == NULL)
						break;
				}

				switch(i)
				{
					// Operation
					case 0:

						// Get the operation
						if (strcmp(IRIS_StackGet(0), "CHK") == 0) operation = C_CHK;
						else if (strcmp(IRIS_StackGet(0), "GET") == 0) operation = C_GET;
						else if (strcmp(IRIS_StackGet(0), "GETS") == 0) operation = C_GETS;
						else if (strcmp(IRIS_StackGet(0), "ADD") == 0) operation = C_ADD;
						else if (strcmp(IRIS_StackGet(0), "IGN") == 0) operation = C_IGN;
						else if (strcmp(IRIS_StackGet(0), "LOOP") == 0) operation = C_LOOP, loopArray = arrayOfArrays;
						else if (strcmp(IRIS_StackGet(0), "ENDLOOP") == 0) operation = C_END_LOOP;
						else i = 4;

						break;

					// Format character
					case 1:
						if (operation != C_LOOP && operation != C_END_LOOP)
						{
							// Get the field number
							format = IRIS_StackGet(0);

							if (strcmp(format, "LLNVAR") == 0) formatType = C_LLNVAR;
							else if (strcmp(format, "LLAVAR") == 0) formatType = C_LLAVAR;
							else if (strcmp(format, "LLLNVAR") == 0) formatType = C_LLLNVAR;
							else if (strcmp(format, "LLLVAR") == 0) formatType = C_LLLVAR;

							else if (strcmp(format, "MMDDhhmmss") == 0) formatType = C_MMDDhhmmss;
							else if (strcmp(format, "hhmmss") == 0) formatType = C_hhmmss;
							else if (strcmp(format, "YYMMDD") == 0) formatType = C_YYMMDD;
							else if (strcmp(format, "YYMM") == 0) formatType = C_YYMM;
							else if (strcmp(format, "MMDD") == 0) formatType = C_MMDD;

							else if (strncmp(format, "an", 2) == 0) formatType = C_STRING;
							else if (strcmp(format, "ns") == 0) formatType = C_STRING;

							else if (strcmp(format, "x+n") == 0) formatType = C_AMOUNT;
							else if (format[0] == 'z') formatType = C_LLNVAR;
							else if (format[0] == 'n') formatType = C_BCD;
							else if (format[0] == 'N') formatType = C_BCD_LINK;
							else if (format[0] == 'b') formatType = C_BITMAP;
							else i = 4;
						}

						break;

					// Field size
					case 2:
						// Get the field number
						if (operation != C_LOOP && operation != C_END_LOOP)
							fieldSize = atoi(IRIS_StackGet(0));
						break;

					// The field value
					case 3:
						// If we reached the end of the loop and the single value = LOOP, go back to the beginning of the loop
						if (operation == C_END_LOOP && IRIS_StackGet(0) && strcmp(IRIS_StackGet(0), "LOOP") == 0 && loopArray)
							arrayOfArrays = loopArray;

						// If we get a value from the initial loop construct, skip that many from the body of the loop and possibly beyond
						else if (operation ==  C_LOOP && IRIS_StackGet(0))
							arrayOfArrays += atoi(IRIS_StackGet(0));

						// Get the field value
						else if (operation != C_LOOP && operation != C_END_LOOP)
						{
							// Allocate enough space for the field value
							fieldValue = my_malloc(2000);
							fieldValue[0] = '\0';

							// Unpack the value
							AS2805BufferUnpack(fieldValue, formatType, fieldSize, response, &length);

/*	{
		char keycode;
		char tempBuf[200];
		sprintf(tempBuf, "break vvv = %s %d %d %d %d %d  ", fieldValue, formatType, fieldSize, operation, length, respLength);
		write_at(tempBuf, strlen(tempBuf), 1, 1);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
							// Make sure we have not gone past the response field
							if (length > respLength)
								tooLong = true;
							else if (operation == C_CHK)
							{
								long num1;
								long num2;

								// If not already available, then it is not valid
								if (IRIS_StackGet(0) == NULL)
									AS2805_Error = -1000;
								else
								{
									// Get the numbers if they can be converted
									num1 = UtilStringToNumber(IRIS_StackGet(0));
									num2 = UtilStringToNumber(fieldValue);

									if (num1 != -1 && num2 != -1)
									{
										if (num1 != num2)
											AS2805_Error = -1000;
									}
									else if (strcmp(IRIS_StackGet(0), fieldValue))
										AS2805_Error = -1000;
								}								
							}
							else if ((operation == C_GET || operation == C_GETS || operation == C_ADD) && value[0] != '\0')
							{
								long num;

								// Store the unpacked value in temporary data area or object file
								IRIS_FullName(value, fullName);

								// If we are adding, get the value of the existing one
								if (operation == C_ADD)
								{
									unsigned long result =  atol(fieldValue);

									// Get the current value
									IRIS_Eval(value, false);

									// Add it to the new value
									if (IRIS_StackGet(0))
										result += atol(IRIS_StackGet(0));

									// Lose the current value from the stack
									IRIS_StackPop(1);

									// Prepare the new value for unpacking
									sprintf(fieldValue, "%ld", result);
								}

								// If this is a pure number, try and lose the leading zeros (take care of the leading '-' sign as well
								else if (operation == C_GET && (num = UtilStringToNumber(fieldValue)) != -1)
									sprintf(fieldValue, "%ld", num);

								// Store the data
								IRIS_StoreData(fullName, fieldValue, false);
							}

							my_free(fieldValue);
						}

						break;
				}

				// Only interested in the top vlue. Lose others that could be pushed in error by the application
				IRIS_StackPop(stackIndex - myStackIndex);
			}
		}
	}

	stackLevel--;

	// We got our message now, release internal buffers
	AS2805Close();
	my_free(response);

	// Return the result. If -1, no error. If a positive number then this is the field number that had the error during a CHK operation
	sprintf(temp, "%d", AS2805_Error);
	IRIS_StackPush(temp);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_BCD_LENGTH
//
// DESCRIPTION:	Sets the attribute of the AS2805 length portion of variable fields. It is either 
//				represented in BCD format or ASCII format.
//
// PARAMETERS:	None.
//
// RETURNS:		None.
//-------------------------------------------------------------------------------------------
//
void __as2805_bcd_length(void)
{
	char * state = IRIS_StackGet(0);

	if (state)
	{
		if (strcmp(state, "TRUE") == 0)
			AS2805BcdLength(true);
		else
			AS2805BcdLength(false);
	}

	IRIS_StackPop(2);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AS2805_OFB_PARAM
//
// DESCRIPTION:	Sets new OFB parameters to vary from the standard.
//
// PARAMETERS:	None.
//
// RETURNS:		None.
//-------------------------------------------------------------------------------------------
//
void __as2805_ofb_param(void)
{
	uchar * hex;
	char * iv_ofb_string = IRIS_StackGet(0);
	char * addField = IRIS_StackGet(1);
	char * maxField = IRIS_StackGet(2);

	AS2805OFBVariation((maxField && maxField[0])?atoi(maxField):53, (addField && addField[0])?atoi(addField):0);

	if (iv_ofb_string)
	{
		int size = strlen(iv_ofb_string) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_malloc(size + 1);
		size = UtilStringToHex(iv_ofb_string, strlen(iv_ofb_string), hex);

		if (size == 8)
			memcpy(iv_ofb, hex, 8);
		my_free(hex);
	}

	IRIS_StackPop(4);
	IRIS_StackPush(NULL);
}
