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
** FUNCTION   : UtilBinToBCD
**
** DESCRIPTION:	Transforms an unsigned long to a BCD array
**
** PARAMETERS:	data		<=	The unsigned long variable
**				bcd			=>	Array to store the converted bcd amount
**				length		=>	Number of BCD digits required. This must be an even number.
**
** RETURNS:		None
**-------------------------------------------------------------------------------------------
*/
void UtilBinToBCD(uchar data, uchar * bcd, uchar length)
{
	ulong divider = 1;

	if (!bcd) return;

	// Adjust the divider to the correct value
	while (--length) divider *= 10;

	// Prepare the STAN variant in the correct format
	for (; divider >= 1; bcd++, divider /= 100)
	{
		*bcd = (uchar) ((data / divider % 10) << 4);
		if (divider == 1) break;
		*bcd |= data / (divider / 10) % 10;
	}
}
