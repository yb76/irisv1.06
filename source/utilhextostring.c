/*
**-----------------------------------------------------------------------------
** PROJECT:			iRIS
**
** FILE NAME:       utility.c
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
** FUNCTION   : UtilHexToString
**
** DESCRIPTION:	Transforms hex byte array to a string. Each byte is simply split in half.
**				The minimum size required is double that of the hex byte array
**
** PARAMETERS:	hex			<=	Array to store the converted hex data
**				string		=>	The number string.
**				length		<=	Length of hex byte array
**
** RETURNS:		The converted string
**-------------------------------------------------------------------------------------------
*/
char * UtilHexToString(uchar * hex, int length, char * string)
{
	int i;

	if (string)
	{
		string[0] = '\0';

		if (hex)
		{
			for (i = 0; i < length; i++)
				sprintf(&string[i*2], "%02X", hex[i]);
		}
	}

	return string;
}
