/*
**-----------------------------------------------------------------------------
** PROJECT:			iRIS
**
** FILE NAME:       utilstringtohex.c
**
** DATE CREATED:    30 January 2008
**
** AUTHOR:          Tareq Hafez
**
** DESCRIPTION:     This module has various utility functions used by other modules
**-----------------------------------------------------------------------------
*/

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

/*
** Local include files
*/
#include "utility.h"

/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Module variable definitions and initialisations.
**-----------------------------------------------------------------------------
*/

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : UtilStringToHex
**
** DESCRIPTION:	Transforms an ASCII HEX string into a hex byte array. The string can only contain
**				0 to 9, : to ?, and A to F tp get the correct results. Otherwise the values are
**				wrong.
**
** PARAMETERS:	string		<=	The ASCII HEX string
**				length		<=	Number of ASCII characters to convert from the right
**				hex			=>	Array to store the converted hex bytes
**
** RETURNS:		The converted hex byte array
**-------------------------------------------------------------------------------------------
*/
int UtilStringToHex(char * string, int length, uchar * hex)
{
	int i;
	int index;

	// Handle error conditions
	if (!string || !hex)
		return 0;

	// Calculate the number of bytes required
	index = length / 2;
	if (length & 0x01) index++;
	length = index;

	// Initialise to all ZEROs
	memset(hex, 0, length);

	for (i = strlen(string)-1, index--; i >= 0  && index >= 0; index--, i--)
	{
		hex[index] = (string[i] >= 'A'? (string[i] - 'A' + 0x0A):(string[i] - '0'));
		if (--i >= 0)
			hex[index] |= (string[i] >= 'A'? (string[i] - 'A' + 0x0A):(string[i] - '0')) << 4;
	}

	return length;
}
