#ifndef __UTILITY_H
#define __UTILITY_H

/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       security.h
**
** DATE CREATED:    29 January 2008
**
** AUTHOR:			Tareq Hafez
**
** DESCRIPTION:     Cryptographic Security Functions
**
**-----------------------------------------------------------------------------
*/

//
//-----------------------------------------------------------------------------
// Constant Definitions.
//-----------------------------------------------------------------------------
//

//
//-----------------------------------------------------------------------------
// Type Definitions
//-----------------------------------------------------------------------------
//

//
//-----------------------------------------------------------------------------
// Function Definitions
//-----------------------------------------------------------------------------
//
int UtilStringToHex(char * string, int length, uchar * hex);
char * UtilHexToString(uchar * hex, int length, char * string);

void UtilBCDToHex(char * string);

void UtilBinToBCD(uchar data, uchar * bcd, uchar length);

char * UtilStrDup(char ** dest, char * source);

long UtilStringToNumber(char * string);

#endif /* __UTILITY_H */
