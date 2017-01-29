/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       display.c
**
** DATE CREATED:    10 July 2007
**
** AUTHOR:          Tareq Hafez
**
** DESCRIPTION:     This module contains the display functions
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Include Files
**-----------------------------------------------------------------------------
*/

/*
** Standard include files.
*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*
** KEF Project include files.
*/
#include <svc.h>

/*
** Local include files
*/
#include "auris.h"
#include "comms.h"
#include "display.h"

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
extern int conHandle;

/*
**-----------------------------------------------------------------------------
** FUNCTION   : DispInit
**
** DESCRIPTION: Initialise the display module
**
** PARAMETERS:	None
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
void DispInit(void)
{
	if (conHandle == -1)
		conHandle = open(DEV_CONSOLE, 0);
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : DispClearScreen
**
** DESCRIPTION: Initialise the display module
**
** PARAMETERS:	None
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
void DispClearScreen(void)
{
	// Initialisation
	DispInit();

	clrscr();

//#ifdef __VMAC
#if 0
	DispText("Dev lock mode", 0, 0, true, false, true);
	SVC_WAIT(250);
	clrscr();
#endif

}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : DispText
**
** DESCRIPTION: Write the text on the LCD screen,
**
** PARAMETERS:	text		<=	Text to write
				row			<=	Row to display it at
**				col			<=	Column to display it at
**				largeFont	<=	Font size: Large or small
**				inverse		<=	Indicates if reverse video is required
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
void DispText(char * text, uint row, uint col, bool clearLine, bool largeFont, bool inverse)
{
	const char * emptyLine = "                     ";

	// Initialisation
	DispInit();

	// If centre requested
	if (col == 255)
		col = ((largeFont?MAX_COL_LARGE_FONT:MAX_COL) - strlen(text)) / 2;

	// If right justificatino requested
	else if  (col == 254)
	{
		col = (largeFont?MAX_COL_LARGE_FONT:MAX_COL) - strlen(text);
	}

	if (!largeFont)
	{
		inverse?setfont("f:asc8x21i.vft"):setfont("");
		if (row == 9999)
			write(conHandle, text, strlen(text));
		else
		{
			if (clearLine) write_at(emptyLine, 21, 1, row+1);
			write_at(text, strlen(text), col+1, row+1);
		}
	}
	else
	{
		inverse?setfont("f:ir8x16i.vft"):setfont("f:ir8x16.vft");
		if (row == 9999)
			write(conHandle, text, strlen(text));
		else
		{
			if (clearLine) write_at(emptyLine, 16, 1, row+1);
			write_at(text, strlen(text), col+1, row+1);
		}
	}
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : DispWriteGraphics
**
** DESCRIPTION: Write the text on the LCD screen,
**
** PARAMETERS:	text		<=	Text to write
				row			<=	Row to display it at
**				col			<=	Column to display it at
**				largeFont	<=	Font size: Large or small
**				inverse		<=	Indicates if reverse video is required
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
void DispGraphics(uchar * graphics, uint row, uint col)
{
	uchar bitmap;
	int i, j, k, m;
	int width = graphics[1]/8;
	int widthchar = graphics[1]/6;
	int height = graphics[3]/8;

	// Initialisation
	DispInit();

	// Set font to 6x8 to achieve the best resolution. The other 2 are 8x16 or 16x16 which will
	// give us a bad column resolution
	setfont("");

	// Do the conversion and output. It must be multiples of 8 pixels in both directions unfortunately
	// Paint as many rows as requested
	for (m = 0; m < height; m++)
	{
		char data[126];

		gotoxy((21 - widthchar)/2+1, row+1+m);
		memset(data, 0, sizeof(data));

		for (k = 0; k < width; k++)
		{
			for (bitmap = 0x80, j = 0; j < 8 && (j+k*8) < sizeof(data); j++, bitmap >>= 1)
			{
				for (i = 0; i < 8; i++)
				{
					if (graphics[4 + i*width + k + m*width*8] & bitmap)
						data[j + k*8] += (1 << i);
				}
			}
		}
		putpixelcol(data, k*8/6*6);
	}
}

#ifdef __VX670
/*
**-----------------------------------------------------------------------------
** FUNCTION   : DispSignal
**
** DESCRIPTION: Updates the signal strength
**
** PARAMETERS:	value		<=	A value from 0 to 31
				row			<=	Row to display it at
**				col			<=	Column to display it at
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
void DispSignal(uint row, uint col)
{
	char data[18];
	int value;
	int index;
	uchar line;

	// Get the signal value
	value = Comms(E_COMMS_FUNC_SIGNAL_STRENGTH, NULL);

	// Set font to 6x8 to achieve the best resolution. The other 2 are 8x16 or 16x16 which will
	// give us a bad column resolution
	setfont("");

	memset(data, 0, sizeof(data));
	
	gotoxy(col + 1, row + 1);

	if (value == 0 || value > 32)
		write_at("   ", 3, col+1, row+1);
	else
	{
		for (index = 0, line = 0x80; value >= 0 && index < 18; index++, value -= 4)
		{
			data[index * 2] = line;
			line >>= 1;
			line |= 0x80;
		}

		putpixelcol(data, 18);
	}
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : DispUpdateBattery
**
** DESCRIPTION: Update the battery display
**
** PARAMETERS	row			<=	Row to display it at
**				col			<=	Column to display it at
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
void DispUpdateBattery(uint row, uint col)
{
	char temp[30];
	static const char * table[20] =
	{
		"\x81\x84\x84\x8A",
		"\x81\x84\x84\x89",
		"\x81\x84\x84\x88",
		"\x81\x84\x84\x87",
		"\x81\x84\x84\x86",
		"\x81\x84\x84\x85",
		"\x81\x84\x8A\x85",
		"\x81\x84\x89\x85",
		"\x81\x84\x88\x85",
		"\x81\x84\x87\x85",
		"\x81\x84\x86\x85",
		"\x81\x84\x85\x85",
		"\x81\x8A\x85\x85",
		"\x81\x89\x85\x85",
		"\x81\x88\x85\x85",
		"\x81\x87\x85\x85",
		"\x81\x86\x85\x85",
		"\x81\x85\x85\x85",
		"\x82\x85\x85\x85",
		"\x83\x85\x85\x85"
	};
	long fullcharge;
	long remainingcharge;

	if ((fullcharge = get_battery_value(FULLCHARGE)) != -1 &&
		(remainingcharge = get_battery_value(REMAININGCHARGE)) != -1 &&
		fullcharge > 0)
	{
		int index = remainingcharge * 20 / fullcharge;
		sprintf(temp, "%3d%% %s", remainingcharge * 100 / fullcharge, (char *) table[index>19?19:index]);
		DispText(temp, row, col, false, false, false);
	}
	else
		DispText("        ", row, col, false, false, false);
}
#endif

char DebugDisp (const char *template, ...)
{
    char stmp[128];
    char key;
    va_list ap;
    int old_mode = 0;

    memset(stmp,0,sizeof(stmp));
    va_start (ap, template);
    vsnprintf (stmp,128, template, ap);
    va_end (ap);

    if(get_display_coordinate_mode() == PIXEL_MODE) {
        old_mode = 1;
        set_display_coordinate_mode(CHARACTER_MODE);
    }

    DispClearScreen();

    setfont("");
    write_at(stmp, strlen(stmp), 1, 0);
    while(read(STDIN, &key, 1) != 1);
    key &= 0x7F;
    if(old_mode)
        set_display_coordinate_mode(PIXEL_MODE);
    return key;

}

int DebugPrint (const char*template,...) {

    va_list ap;
    char stmp[128];
    char s_debug[30] = "\033k042DEBUG:";
    int pLen;

    memset(stmp,0,sizeof(stmp));
    va_start (ap, template);
    vsnprintf (stmp,128, template, ap);

    PrtPrintBuffer(strlen(s_debug), s_debug, 0);
    PrtPrintBuffer(strlen(stmp), stmp, 2);//E_PRINT_END
    PrtPrintBuffer(2, "\n\n", 2);//E_PRINT_END
    return 0;
}

