#ifndef __DISPLAY_H
#define __DISPLAY_H

/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       display.h
**
** DATE CREATED:    10 July 2007
**
** AUTHOR:			Tareq Hafez
**
** DESCRIPTION:     This hsould be used when referencing items within display.c
**-----------------------------------------------------------------------------
*/

//
//-----------------------------------------------------------------------------
// Constant Definitions.
//-----------------------------------------------------------------------------
//
#define	MAX_ROW					8
#define	MAX_COL					21
#define	MAX_ROW_LARGE_FONT		4
#define	MAX_COL_LARGE_FONT		16

//
//-----------------------------------------------------------------------------
// Function Declarations
//-----------------------------------------------------------------------------
//
void DispInit(void);
void DispClearScreen(void);
void DispText(char * text, unsigned int row, unsigned int col, unsigned char clearLine, unsigned char largeFont, unsigned char inverse);
void DispGraphics(unsigned char * graphics, unsigned int row, unsigned int col);
void DispSignal(uint row, uint col);
void DispUpdateBattery(unsigned int row, unsigned int col);

#endif /* __DISPLAY_H */
