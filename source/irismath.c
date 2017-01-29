//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       irismath.c
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

//
// Local include files
//
#include "alloc.h"
#include "security.h"
#include "utility.h"
#include "sha1.h"
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
static int msgStart_cont;
static int msgCont_cont;
static int msgEnd_cont;

static int msgStart_keep;
static int msgCont_keep;
static int msgEnd_keep;

static int attrStart;
static int attrEnd;


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// MATH FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()MUL, ()DIV, ()MOD, ()ADD, ()SUB
//
// DESCRIPTION:	Performs a math operation on two numbers
//
// PARAMETERS:	None
//
// RETURNS:		The resulting number as a string
//-------------------------------------------------------------------------------------------
//
void __math(void)
{
	long res = 0;
	char result[30];
	long f = 0;
	long s = 0;

	// Obtain the first value if available
	if (IRIS_StackGet(1))
		f = atol(IRIS_StackGet(1));

	// Obtain the second value if available
	if (IRIS_StackGet(0))
		s = atol(IRIS_StackGet(0));

	// Perform the appropriate operation
	if (strcmp(IRIS_StackGet(2), "()MUL") == 0)
		res = f * s;
	else if (strcmp(IRIS_StackGet(2), "()DIV") == 0 && s)
		res = f / s;
	else if (strcmp(IRIS_StackGet(2), "()MOD") == 0 && s)
		res = f % s;
	else if (strcmp(IRIS_StackGet(2), "()SUM") == 0)
		res = f + s;
	else if (strcmp(IRIS_StackGet(2), "()SUB") == 0)
		res = f - s;

	// Lose the function and parameters
	IRIS_StackPop(3);

	// Make the result a string and push back onto the stack
	sprintf(result, "%ld", res);
	IRIS_StackPush(result);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()RAND
//
// DESCRIPTION:	Returns a random number (Warning: Uses First DES Key Slot)
//
// PARAMETERS:	None
//
// RETURNS:		A random number as a string
//-------------------------------------------------------------------------------------------
//
void __rand(void)
{
	ulong rand;
	uchar kvc[3];
	char result[30];

	SecurityGenerateKey(irisGroup, 0, 8);
	SecurityKVCKey(irisGroup, 0, 8, kvc);
	rand = kvc[2]*65536 + kvc[1]*256 + kvc[0];

	sprintf(result, "%ld", rand);

	IRIS_StackPop(1);
	IRIS_StackPush(result);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()LUHN
//
// DESCRIPTION:	Verifies the LUHN digit of a PAN
//
// PARAMETERS:	None
//
// RETURNS:		false if not OK
//				true if OK
//-------------------------------------------------------------------------------------------
//
void __luhn(void)
{
	int luhn, i, j;
	char * pan = IRIS_StackGet(0);

	if (pan)
	{
		for (luhn = 0, i = strlen(pan)-1, j = 0; i >= 0; i--, j++)
		{
			uchar digit = pan[i]-'0';
			luhn += j%2? (digit>=5? (digit%5)*2+1:digit*2):digit;
		}
	}

	IRIS_StackPop(2);

	if (luhn%10 || !pan)
		IRIS_StackPush(NULL);
	else
		IRIS_StackPush("TRUE");
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()FMT
//
// DESCRIPTION:	re-sizes a number
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __fmt(void)
{
	char result[100];
	char myFormat[10];

	if (IRIS_StackGet(1) == NULL)
		strcpy(result, "");
	else
	{
		sprintf(myFormat, "%%%ss", IRIS_StackGet(1));
		sprintf(result, myFormat, IRIS_StackGet(0)?IRIS_StackGet(0):"");
	}

	IRIS_StackPop(3);
	IRIS_StackPush(result);
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()AMOUNT
//
// DESCRIPTION:	Re-formats a number string to an amount string
//
// PARAMETERS:	None
//
// RETURNS:		The number formatted as an amount
//-------------------------------------------------------------------------------------------
//
void __amount(void)
{
	char result[30];
	char curr_symbol[20];
	unsigned int curr_decimal;
	int i = 0, index = 0;
	char * comma = IRIS_StackGet(0);
	char * value = IRIS_StackGet(1);

	// Get the currency details
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

	// Initialise
	memset(result, 0, sizeof(result));

	if (!value || !comma)
	{
		IRIS_StackPop(3);
		sprintf(result, "%s%s%0*d", curr_symbol, curr_decimal?"0.":"0", curr_decimal, 0);
		IRIS_StackPush(result);		// $0.00 for Australia
		return;
	}

	// If the amount is negative, place the first negative bracket first
	if (value[0] == '-')
	{
		result[i++] = '-';
		value++;
	}

	// Add the dollar sign
	strcat(&result[i], curr_symbol);
	i += strlen(curr_symbol);

	if (strlen(value) <= curr_decimal)
		result[i++] = '0';
	else
	{
		index = (strlen(value) - curr_decimal) % 3;
		if (index == 0) index = 3;

		memcpy(&result[i], value, index);

		for (i += index; strlen(&value[index]) > curr_decimal; index += 3)
		{
			if (comma[0] == '1') result[i++] = ',';
			memcpy(&result[i], &value[index], 3);
			i += 3;
		}
	}

	result[i++] = '.';
	sprintf(&result[i], "%0*s", curr_decimal, &value[index]);

	IRIS_StackPop(3);
	IRIS_StackPush(result);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()LEN
//
// DESCRIPTION:	Returns the "value" length
//
// PARAMETERS:	value
//
// RETURNS:		The length of the value. If value is NULL then NULL is returned.
//-------------------------------------------------------------------------------------------
//
void __len(void)
{
	char * value = IRIS_StackGet(0);

	if (value)
	{
		char temp[10];
		int length = strlen(value);
		
		// Clean up and return NULL if value does not exist
		IRIS_StackPop(2);
		IRIS_StackPush(ltoa(length, temp, 10));
		return;
	}

	// Clean up and return NULL if value does not exist
	IRIS_StackPop(2);
	IRIS_StackPush("0");
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()STRIP
//
// DESCRIPTION:	Strip the blanks at the end of a string
//
// PARAMETERS:	None
//
// RETURNS:		The current time in struct tm format
//-------------------------------------------------------------------------------------------
//
void __strip(void)
{
	char * value = IRIS_StackGet(0);
	char * result = NULL;

	if (value)
	{
		int i;

		// Initialisation
		UtilStrDup(&result, value);

		// Strip the trailing spaces
		for (i = strlen(result)-1; result[i] == ' '; i--)
			result[i] = '\0';
	}

	IRIS_StackPop(2);
	IRIS_StackPush(result);
	UtilStrDup(&result, NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PAD
//
// DESCRIPTION:	pad the string with equal spaces to the left and right
//
// PARAMETERS:	None
//
// RETURNS:		The current time in struct tm format
//-------------------------------------------------------------------------------------------
//
void __pad(void)
{
	char * len = IRIS_StackGet(0);
	char * value = IRIS_StackGet(1);

	if (value && len && (atol(len) > strlen(value)))
	{
		char result[100];

		sprintf(result,"%*s%*s", (atol(len) + strlen(value)) / 2, value, (atol(len) - strlen(value)) / 2, " ");
		IRIS_StackPop(3);
		IRIS_StackPush(result);
	}
	else
	{
		IRIS_StackPop(3);
		IRIS_StackPush(value);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SUBSTRING
//
// DESCRIPTION:	Extracts part of a string
//
// PARAMETERS:	None
//
// RETURNS:		The current time in struct tm format
//-------------------------------------------------------------------------------------------
//
void __substring(void)
{
	char * value = IRIS_StackGet(0);
	char * format = IRIS_StackGet(1);

	if (value && format)
	{
		int i = 0, j;
		bool left = false;
		char from = '\0';
		int start = 0, end = 0;
		char * result = NULL;

		// Initialisation
		UtilStrDup(&result, value);

		for(;;)
		{
			// Get direction
			if (format[i] == '>')
			{
				left = true;
				i++;
			}
			else if (format[i] == '<') i++;

			// Get starting character
			if (format[i] != '|')
			{
				from = format[i];
				i++;
			}

			// Check we now have an array marker
			if (format[i] != '|')
				break;

			// Get start subscript value
			for (i++; format[i] != '|' && format[i] != '-'; i++)
			{
				if (format[i] < '0' || format[i] > '9')
				{
					IRIS_StackPop(3);
					IRIS_Eval(result, false);
					my_free(result);
					return;
				}
				else start = start * 10 + format[i] - '0';
			}

			// Get end subscript value
			if (format[i] != '-')
				end = start;
			else for (i++; format[i] != '|'; i++)
			{
				if (format[i] < '0' || format[i] > '9')
				{
					IRIS_StackPop(3);
					IRIS_Eval(result, false);
					my_free(result);
					return;
				}
				else end = end * 10 + format[i] - '0';
			}

			// If the end is not specified, then set the end to the end of the string
			if (left && end == 0)
				end = strlen(value);
			else if (!left && start == 0)
				start = strlen(value);

			// Adjust the start and end to reference position ZERO as the beginning
			if (start) start--;
			if (end) end--;

			// Look for the from character
			if (from || left == false)
			{
				for (i = 0; value[i] != from && value[i] != '\0'; i++);
				if (value[i] == from) {if (left) i++; else i--;}
				else if (value[i] == '\0') {if (left) i=0; else i--;}
			}
			else i = 0;

			// Adjust the starting and ending positions
			if (left)
			{
				start += i;
				if (end != (int) (strlen(value)-1)) end += i;
			}
			else
			{
				start = i - start;
				if (start < 0) start = 0;
				end = i - end;
			}

			// Make sure we are within the subscripts so we do not crash
			if (start < 0 || start >= (int) strlen(value) || end < 0 || end >= (int) strlen(value)) 
				break;

			// Extract the string
			for (j = 0, i = start; i <= end; i++, j++)
			{
				result[j] = value[i];
			}
			result[j] = '\0';

			break;
		}

		// Clean the stack of function name and parameters, then push the result onto the stack
		IRIS_StackPop(3);
		IRIS_Eval(result, false);
		my_free(result);
	}
	else
	{
		IRIS_StackPop(3);
		IRIS_StackPush(NULL);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SHA1
//
// DESCRIPTION:	Calculates SHA1 of the supplied buffer
//
// PARAMETERS:	Assumes input is an ASCII hex.
//				If normal ASCII characters, the caller must convert it first by using "\h"
//
// RETURNS:		SHA1 (20 hex bytes = 40 ASCII bytes)
//-------------------------------------------------------------------------------------------
//
void __sha1(void)
{
	sha1_context sha1;
	uchar hexDigest[20];
	char digest[41];

	uchar * hex;

	// Get the data that we need to SHA1 in ASCII from the stack
	char * data = IRIS_StackGet(0);

	if (data)
	{
		int size = strlen(data) / 2;

		// Allocate enough hex buffer and convert the ASCII hex data to hex.
		hex = my_malloc(size+1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Perform sha-1
		sha1_starts(&sha1);
		sha1_update(&sha1, hex, size);
		sha1_finish(&sha1, hexDigest);

		// Lose the function name and data
		IRIS_StackPop(2);
		my_free(hex);

		// Change digest to ASCII hex and return it
		UtilHexToString(hexDigest, sizeof(hexDigest), digest);

		IRIS_StackPush(digest);
	}
	else
	{
		IRIS_StackPop(2);
		IRIS_StackPush(NULL);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TO_ASCII_HEX
//
// DESCRIPTION:	Converts ASCII bytes to ASCII hex bytes effectively doubling the size of the string.
//				For example TAREQ convets to 5441524551
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __to_ascii_hex(void)
{
	char * ascii_hex;
	char * hex = IRIS_StackGet(0);

	if (hex)
	{
		ascii_hex = my_malloc(strlen(hex) * 2 + 1);
		UtilHexToString((uchar *) hex, strlen(hex), ascii_hex);

		IRIS_StackPop(2);

		IRIS_StackPush(ascii_hex);
		my_free(ascii_hex);
	}
	else
	{
		IRIS_StackPop(2);
		IRIS_StackPush(NULL);
	}
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TO_HEX
//
// DESCRIPTION:	Converts ASCII Hex bytes to ASCII bytes effectively halfing the size with the odd extra.
//				For example 5441524551 convets to TAREQ
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __to_hex(void)
{
	char * ascii_hex = IRIS_StackGet(0);
	uchar * hex; 

	if (ascii_hex)
	{
		hex = my_calloc(strlen(ascii_hex) / 2 + 1);
		UtilStringToHex(ascii_hex, strlen(ascii_hex), hex);

		IRIS_StackPop(2);

		IRIS_StackPush((char *) hex);
		my_free(hex);
	}
	else
	{
		IRIS_StackPop(2);
		IRIS_StackPush(NULL);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TO_SAFE_HEX
//
// DESCRIPTION:	Converts ASCII Hex bytes to ASCII bytes effectively halfing the size with the odd extra.
//				For example 5441524551 convets to TAREQ, but ensures that special characters
//				used for JSON objects such as , [ ] { } are xored with 0x80.
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __to_safe_hex(void)
{
	char * ascii_hex = IRIS_StackGet(0);
	uchar * hex; 

	if (ascii_hex)
	{
		int i, j;
		hex = my_calloc(strlen(ascii_hex) / 2 + 1);
		UtilStringToHex(ascii_hex, strlen(ascii_hex), hex);

		IRIS_StackPop(2);

		for (i = 0, j = strlen((char *) hex); i < j; i++)
		{
			if (hex[i] == ',' || hex[i] == '[' || hex[i] == ']' || hex[i] == '{'  || hex[i] == '}' )
				hex[i] |= 0x80;
		}

		IRIS_StackPush((char *) hex);
		my_free(hex);
	}
	else
	{
		IRIS_StackPop(2);
		IRIS_StackPush(NULL);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CHECKSUM_XOR
// FUNCTION   : ()CHECKSUM_SUM
//
// DESCRIPTION:	Calculates an XOR or SUM checksum over some ASCII bytes
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __checksum_xor_sum(char * ascii_hex, char * from, char * from_offset, char * to, char * to_offset, int xor_sum)
{
	uchar * hex;
	unsigned checksum = 0;
	char cs[3];

	if (ascii_hex)
	{
		int i;
		int start = 0;
		int size = strlen(ascii_hex);
		int end = size;

		if (from && from[0])
		{
			for (start = 0; start < size; start += 2)
			{
				if (strncmp(from, &ascii_hex[start], strlen(from)) == 0)
					break;
			}
			if (start >= size) start = 0;
		}

		if (from_offset && from_offset[0])
		{
			start += atoi(from_offset);
			if (start < 0 || start >= size) start = 0;
		}

		if (to && to[0] && size >= 2)
		{
			for (end = size-2; end >= 0; end -= 2)
			{
				if (strncmp(to, &ascii_hex[end], strlen(to)) == 0)
					break;
			}
			if (end < 0) end = size;
		}
		
		if (to_offset && to_offset[0])
		{
			end += atoi(to_offset);
			if (end < 0 || end > size) end = size;
		}

		// Must terminal the string for the conversion to work
		if (end < size)
			ascii_hex[end] = '\0';

		if (start < end)
			size = (end - start) / 2;
		else
			size /= 2;

		hex = my_calloc(size + 1);
		UtilStringToHex(&ascii_hex[start], size*2, hex);

		for (i = 0; i < size; i++)
		{
			if (xor_sum == 0)
				checksum ^= hex[i];
			else
				checksum += hex[i];
		}

		if (xor_sum)
			checksum %= 0x100;

		my_free(hex);
	}

	IRIS_StackPop(6);
	sprintf(cs, "%02X", checksum);

	IRIS_StackPush((char *) cs);
}


void __checksum_xor(void)
{
	char * to_offset = IRIS_StackGet(0);
	char * to = IRIS_StackGet(1);
	char * from_offset = IRIS_StackGet(2);
	char * from = IRIS_StackGet(3);
	char * ascii_hex = IRIS_StackGet(4);

	__checksum_xor_sum(ascii_hex, from, from_offset, to, to_offset, 0);
}

void __checksum_sum(void)
{
	char * to_offset = IRIS_StackGet(0);
	char * to = IRIS_StackGet(1);
	char * from_offset = IRIS_StackGet(2);
	char * from = IRIS_StackGet(3);
	char * ascii_hex = IRIS_StackGet(4);

	__checksum_xor_sum(ascii_hex, from, from_offset, to, to_offset, 1);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CSV
//
// DESCRIPTION:	Breaks down a CSV message into its data fields
//				* It looks for a starting character and an ending character
//				* It stores the data in CSV object.
//				* An array of fields will be stored as F0,F1,F2,F3,etc.
//				* It looks for a TAB seperator
//				* An array of fields after the TAB will be stored as T0,T1,T2,T3,etc.
//				* An fields after the ending character will be stored as L
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __csv(void)
{
	char * sep = IRIS_StackGet(0);
	char * end = IRIS_StackGet(1);
	char * start = IRIS_StackGet(2);
	char * record = IRIS_StackGet(3);
	char * name = IRIS_StackGet(4);
	char * objectData;
	char * temp;

	if (record)
	{
		int i, j;
		int startIndex = 0;
		int endIndex = strlen(record);
		unsigned char seperator = ',';
		char field = 'F';

		temp = my_malloc(1000);

		// Find the index of the start character
		if (start && start[0] && start[1] && start[2] == '\0')
		{
			unsigned char search;
			UtilStringToHex(start, 2, &search);
			for (i = 0; i < (int) strlen(record); i++)
			{
				if (search == record[i])
				{
					startIndex = i;
					break;
				}
			}
		}

		// Find the index of the end character
		if (end && end[0] && end[1] && end[2] == '\0')
		{
			unsigned char search;
			UtilStringToHex(end, 2, &search);
			for (i = 0; i < (int) strlen(record); i++)
			{
				if (search == record[i])
				{
					endIndex = i;
					break;
				}
			}
		}

		// Change the seperator if needed
		if (sep && sep[0] && sep[1] && sep[2] == '\0')
			UtilStringToHex(sep, 2, &seperator);

		// Create a parsing file
		objectData = my_malloc(200);
		sprintf(objectData, "{TYPE:DATA,NAME:%s,GROUP:%s", name, currentObjectGroup);

		// Make up the next field until a seperator or end index is found
		strcpy(temp, ",F0:");
		for (i = startIndex+1, j = 0; i < endIndex; i++)
		{
			if (record[i] != ',' && record[i] != seperator)
			{
				temp[strlen(temp)+1] = '\0';
				temp[strlen(temp)] = record[i];
			}
			else
			{
				objectData = my_realloc(objectData, strlen(objectData) + strlen(temp) + 2);
				strcat(objectData, temp);

				if (record[i] == seperator)
					field = 'T', j = -1;
				
				sprintf(temp, ",%c%d:", field, ++j);
			}
		}

		objectData = my_realloc(objectData, strlen(objectData) + strlen(temp) + 2);
		strcat(objectData, temp);

		if (endIndex != (int) strlen(record))
		{
			sprintf(temp, ",L:%s", &record[endIndex+1]);
			objectData = my_realloc(objectData, strlen(objectData) + strlen(temp) + 2);
			strcat(objectData, temp);
		}

		strcat(objectData, "}");

		IRIS_PutNamedObjectData(objectData, strlen(objectData), name);
		my_free(objectData);
		my_free(temp);
	}

	IRIS_StackPop(6);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()XML
//
// DESCRIPTION:	Returns a value within an XML message
//
//				Locates within a "message", the body specified as a nested argument A/B/C and returns the body
//				unless the "attribute" is defined where the attribute value of the found element is returned.
//
//				If "from" is empty or NULL, the search starts from the beginning of the message until the end
//				of the message.
//
//				If "from" is "START", the search continues from the last elemment that CONTAINED the last found
//				For example: If the previous operation looked for A/B/C, then using "from" = START will restrict
//				the search to the body of A/B. Other examples: A ==> Entire message, A/B ==> body of A.
//
//				If "from" is "CONT", this is similar to "START" except that it continues from where it left off.
//				For example: A/B/C ==> Body of B but just after the end of element C, A ==> Immediately after element A,
//				A/B ==> body of A but just after the end of element B.
//
//				Example of a message ~MSG
//				<A>......
//					<B>......</B>......
//					<B>....
//						<D>....</D>....
//						<D>....</D>....
//						<E>.....
//							<D>...</D>....
//						</E>....
//						<D>....</D>....
//					</B>....
//				</A>.....
//				<C>....</C>
//
//				To locate A, B, B, C:
//				~A:[()XML,~MSG,A,,],~B(0):[()XML,~MSG,B,,START],~B(1):[()XML,~MSG,B,,CONT],~C:[()XML,~MSG,C,,]
//				OR
//				~A:[()XML,~MSG,A,,],~B(0):[()XML,~MSG,A/B,,],~B(1):[()XML,~MSG,B,,CONT],~C:[()XML,~MSG,C,,]
//
//				To locate D,D,D - but this will not include the D within the element E:
//				~D(0):[()XML,~MSG,A/B/D,,],~D(1):[()XML,~MSG,D,,CONT],~D(2):[()XML,~MSG,D,,CONT]
//
// PARAMETERS:	message		<=	The entire XML message. This should NOT be changed during continuations (ie from = START or CONT)
//				what		<=	A nested element to look for
//				attribute	<=	The attribute name to search for within the element. If this is supplied, the value of the
//								attribute is returned instead of the element body.
//				from		<=	blank (RESET assumed), START or CONT.
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
static bool isblank(char data)
{
	if (data == ' ' || data == '\t' || data == '\r' || data == '\n')
		return true;
	else
		return false;
}

static char * __xml_get(char * what, char * message, int msgStart, int * msgEnd, int * bodyStart, int * bodyEnd)
{
	int i;
	int j = 0;
	int block = 0;
	int sameNameLevel = 0;
	char name[100];

	// Initialisation
	*bodyStart = *bodyEnd = attrStart = attrEnd = -1;

	// Traverse the message looking for the required tag...
	for (i = msgStart; i < *msgEnd; i++)
	{
		// If this is a comment line, skip it
//		if (message[i] == '<' && message[i+1] == '!' && message[i+2] == '-' && message[i+3] == '-')
//			while (i < *msgEnd && message[i-2] == '-' && message[i-1] == '-' && message[i] != '>') i++;

		// If this is a header or comment record, skip it
		if (message[i] == '<' && (message[i+1] == '?' || message[i+1] == '!'))
			while (i < *msgEnd && message[i] != '>') i++;

		// Find the beginning of the next element
		else if (block == 0 && message[i] == '<' && message[i+1] != '/')
		{
			// Find the element name
			for (j = 0; !isblank(message[++i]) && message[i] != '/' && message[i] != '>' && j < (sizeof(name)-1); j++)
				name[j] = message[i];
			name[j] = '\0';

			// Does it match what we are looking for?
			for (j = 0; name[j] && name[j] == what[j]; j++);

			// Yes, we got a match...
			if (name[j] == '\0' && (what[j] == '/' || what[j] == '\0'))
			{
				int k;
				int end = 0;

				// Find the start and end of the attribute text
				for (attrStart = attrEnd = i; i < *msgEnd; i++, attrEnd++)
				{
					// If there is no body, indicate so and return...
					if (message[i] == '/' && message[i+1] == '>')
					{
						*msgEnd = i + 2;
						return &what[j+(what[j]?1:0)];
					}

					// Find the end of the element start tag
					if (message[i] == '>')
					{
						i++;
						break;
					}
				}

				// Find the start and end of the body text
				for (*bodyStart = *bodyEnd = i; i < *msgEnd; i++)
				{
					if (message[i] == '<')
						end = 1, k = 0, *bodyEnd = i;

					else if (end == 1 && k == j)
					{
						while (isblank(message[i])) i++;
						if (message[i] == '>')
							sameNameLevel++;
						else end = k = 0;
					}
					else if (end == 1 && message[i] == what[k])
						k++;
					else if (end == 1 && message[i] == '/')
						end = 2;

					else if (end == 2 && k == j)
					{
						while (isblank(message[i])) i++;
						if (message[i] == '>' && sameNameLevel == 0)
						{
							*msgEnd = i + 1;
							return &what[j+(what[j]?1:0)];
						}
						if (sameNameLevel) sameNameLevel--;
						end = k = 0;
					}
					else if (end == 2 && message[i] == what[k])
						k++;
					else
						end = k = 0;
				}

				// ill formed XML message
				return NULL;
			}

			// We do not have a match, start looking for the ending tag
			block = 1, i--;
		}

		// The end of the element is found without a body, we can restart the search again
		else if (block == 1 && message[i] == '/' && message[i+1] == '>')
		{
			if (sameNameLevel)
				sameNameLevel--, block = 2;
			else
				block = 0;
			i++;
		}

		// The end of the starting tag is found, we can now start looking for the ending tag
		else if (block == 1 && message[i] == '>')
			block = 2;

		// If we find another nested tag with the same name, then we must find an extra ending tag with the same name
		else if (block == 2 && message[i] == '<')
		{
			int temp = i;
			for (i++, j = 0; name[j] == message[i]; i++, j++);
			if (name[j] == '\0')
				block = 1, i--, sameNameLevel++;
			else i = temp;
		}

		// The ending tag is found, we can now find for the ending tag closing bracket
		else if (block == 2 && message[i-1] == '<' && message[i] == '/')
		{
			int temp = i;
			for (i++, j = 0; name[j] == message[i]; i++, j++);
			if (name[j] == '\0')
				block = 3, i--;
			else i = temp;
		}

		// The closing bracket of the ending tag is found, we can restart the search again
		else if (block == 3 && message[i] == '>')
		{
			if (sameNameLevel)
				sameNameLevel--, block = 2;
			else
				block = 0;
		}
	}

	return NULL;
}

static void __xml_get_attribute(char * message, char * attribute, int pop)
{
	if (message && attribute && attribute[0] && attrStart >= 0 && attrEnd >= 0)
	{
		char * ptr;
		char * start = &message[attrStart];
		char * end = &message[attrEnd];

		while ((ptr = strstr(start, attribute)) != NULL && ptr < end)
		{
			for (ptr += strlen(attribute); ptr < end && *ptr && isblank(*ptr); ptr++);
			if (*ptr == '=')
			{
				for (ptr++; ptr < end && *ptr && isblank(*ptr); ptr++);
				if (*ptr == '"' || *ptr == '\'')
				{
					start = ptr;
					for (ptr++; ptr < end && *ptr && *ptr != *start; ptr++);
					if (*ptr == *start)
					{
						char * data = my_malloc(ptr - start);
						memcpy(data, start+1 , ptr - start - 1);
						data[ptr - start - 1] = '\0';
						IRIS_StackPop(pop);
						IRIS_StackPush(data);
						my_free(data);
						return;
					}
				}
			}
			else start = ptr;
		}
	}

	IRIS_StackPop(pop);
	IRIS_StackPush(NULL);
}

void __xml(void)
{
	char * from = IRIS_StackGet(0);			// Where to start searching from
	char * attribute = IRIS_StackGet(1);	// If an attribute is requested and not the value/body
	char * what = IRIS_StackGet(2);			// A nested "what" separated by '/' characters.
	char * message = IRIS_StackGet(3);		// The XML message

	if (message && what && what[0])
	{
		int msgStart;
		int msgEnd;
		int bodyStart = (from && strcmp(from, "START") == 0)?msgStart_cont:((from && strcmp(from, "CONT") == 0)?msgCont_cont:0);
		int bodyEnd = (from && (strcmp(from, "START") == 0 || strcmp(from, "CONT") == 0))?msgEnd_cont:strlen(message);

		// Keep looking within the message until the correct element is found
		do
		{
			// Set the message start search location...
			msgStart = bodyStart;

			// If this is not the beginning of a continuation, set the message start search location for the next XML continuation search
			if (!from || strcmp(from, "CONT") || what != IRIS_StackGet(2))
				msgStart_cont = bodyStart;

			// Set the message end search location
			msgEnd = msgEnd_cont = bodyEnd;

			what = __xml_get(what, message, msgStart, &msgEnd, &bodyStart, &bodyEnd);
		} while (what && what[0]);

		// Setup the continuation markers
		msgCont_cont = msgEnd;

		// If we end up with an empty string, then we found what we are looking for...
		if (what && what[0] == '\0')
		{
			if (attribute && attribute[0])
			{
				__xml_get_attribute(message, attribute, 5);
				return;
			}
			else
			{
				char * data = my_malloc(bodyEnd - bodyStart + 1);
				memcpy(data, &message[bodyStart] , bodyEnd - bodyStart);
				data[bodyEnd - bodyStart] = '\0';
				IRIS_StackPop(5);
				IRIS_StackPush(data);
				my_free(data);
				return;
			}
		}
	}

	IRIS_StackPop(5);
	IRIS_StackPush(NULL);
}


void __xml_attr(void)
{
	char * attribute = IRIS_StackGet(0);	// If an attribute is requested and not the value/body
	char * message = IRIS_StackGet(1);		// The XML message

	__xml_get_attribute(message, attribute, 3);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()XML_SAVE_CONTEXT
//
// DESCRIPTION:	Saves the current context of an XML search
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __xml_save_context(void)
{
	msgStart_keep = msgStart_cont;
	msgCont_keep = msgCont_cont;
	msgEnd_keep = msgEnd_cont;

	IRIS_StackPop(1);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()XML_RESTORE_CONTEXT
//
// DESCRIPTION:	Saves the current context of an XML search
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __xml_restore_context(void)
{
	msgStart_cont = msgStart_keep;
	msgCont_cont = msgCont_keep;
	msgEnd_cont = msgEnd_keep;

	IRIS_StackPop(1);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DW_ENCODE
//
// DESCRIPTION:	Encode a payload (in ASCII hex) for transmission via DataWire
//
// PARAMETERS:	None
//
// RETURNS:		The encoded payload
//-------------------------------------------------------------------------------------------
//
void __dw_encode(void)
{
	char * payload = IRIS_StackGet(0);
	char * output = NULL;
	unsigned i, j;

	if (payload)
	{
		// Allocate enough buffer.... We only need a maximum of 3/2 the original size....
		output = my_malloc(strlen(payload) * 2);

		for (i = j = 0; i < strlen(payload); i += 2)
		{
			if ((payload[i] == '3' && payload[i+1] < 'A') ||
				(payload[i] == '4' && payload[i+1] > '0') ||
				(payload[i] == '5' && payload[i+1] < 'B') ||
				(payload[i] == '6' && payload[i+1] > '0') ||
				(payload[i] == '7' && payload[i+1] < 'B'))
//			if (0)
			{
				output[j] = (payload[i] - '0') << 4;
				output[j++] += (payload[i+1] >= 'A'?(payload[i+1] - 'A' + 0x0A):(payload[i+1] - '0'));
			}
			else
			{
				output[j++] = '|';
				output[j++] = payload[i];
				output[j++] = payload[i+1];
			}
		}

		output[j] = '\0';
	}

	IRIS_StackPop(2);
	IRIS_StackPush(output);
	my_free(output);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DW_DECODE
//
// DESCRIPTION:	Decode a payload received from DataWire
//
// PARAMETERS:	None
//
// RETURNS:		The decoded payload
//-------------------------------------------------------------------------------------------
//
void __dw_decode(void)
{
	char * payload = IRIS_StackGet(0);
	char * output = NULL;
	unsigned i, j;

	if (payload)
	{
		// Allocate enough buffer.... We only need a maximum of 3/2 the original size....
		output = my_malloc(strlen(payload) * 2);

		for (i = j = 0; i < strlen(payload); i++)
		{
			if (payload[i] == '|')
			{
				output[j++] = payload[++i];
				output[j++] = payload[++i];
			}
			else
			{
				payload[i] &= 0x7F;
				output[j++] = ((payload[i] >> 4) >= 0x0A)?((payload[i] >> 4) - 0x0A + 'A'):((payload[i] >> 4) + '0');
				output[j++] = ((payload[i] & 0x0F) >= 0x0A)?((payload[i] & 0x0F) - 0x0A + 'A'):((payload[i] & 0x0F) + '0');
			}
		}

		output[j] = '\0';
	}

	IRIS_StackPop(2);
	IRIS_StackPush(output);
	my_free(output);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()CRC_16
//
// DESCRIPTION:	Calculate the CRC-16 of data
//
// PARAMETERS:	None
//
// RETURNS:		The CRC-16 value
//-------------------------------------------------------------------------------------------
//
#ifdef _DEBUG
static const unsigned short crc_table[256] = {
0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/* CRC calculation macros */
#define CRC_INIT 0xFFFF
#define CRC(crcval,newchar) crcval = (crcval >> 8) ^ \
	crc_table[(crcval ^ newchar) & 0x00ff]

static unsigned short SVC_CRC_CRC16_M(const unsigned char * message, int length, unsigned short crc)
{
	unsigned long i;

	for(i = 0; i < length; i++)
		CRC(crc, message[i]);

	return crc;
}
#endif

void __crc_16(void)
{
	char crc[5];

	uchar * hex;

	// Get the data that we need to calculate the CRC-16 from the stack
	char * data = IRIS_StackGet(0);

	if (data)
	{
		int size = strlen(data) / 2;
		unsigned short crcshort;
		unsigned char crchex[2];

		// Allocate enough hex buffer and convert the ASCII hex data to hex.
		hex = my_malloc(size+1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Calculate CRC-16
		crcshort = SVC_CRC_CRC16_M(hex, size, 0);
		crchex[0] = crcshort >> 8;
		crchex[1] = crcshort & 0xff;

		// Lose the function name and data
		IRIS_StackPop(2);
		my_free(hex);

		// Change digest to ASCII hex and return it
		UtilHexToString(crchex, 2, crc);

		IRIS_StackPush(crc);
	}
	else
	{
		IRIS_StackPop(2);
		IRIS_StackPush(NULL);
	}
}
