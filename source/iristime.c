//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iristime.c
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
#include "my_time.h"
#include "iris.h"
#include "irisfunc.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//
#define	C_YEAR		0
#define	C_MON		1
#define	C_DAY		2
#define	C_HOUR		3
#define	C_MIN		4
#define	C_SEC		5

#define	C_FMT_MAX	6

//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//
static unsigned long ticks;

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// TIME FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()NOW
//
// DESCRIPTION:	Returns the current time
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __now(void)
{
	char result[50];	// Just in case the time function returns  something weird. However, 15 is enough if all OK.

	// Pop the function name from the stack
	IRIS_StackPop(1);

	// Push the result onto the stack
	sprintf(result, "%lu", my_time(NULL));
	IRIS_StackPush(result);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TIME_SET
//
// DESCRIPTION:	Sets the current time
//
// PARAMETERS:	value	<=	 time to set clock to. Make sure it is populated with YYMMDDhhmmss at least
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __time_set(void)
{
	time_t sometime;
	char * newTime = IRIS_StackGet(0);

	if (newTime)
	{
		sometime = atol(newTime);
		timeSet(my_gmtime(&sometime));
	}

	// Pop the function name and value from the stack
	IRIS_StackPop(2);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()MKTIME
//
// DESCRIPTION:	converta a time structure to a formatted string
//
// PARAMETERS:	format	<=	Maps the value provided to ne of the elements within "struct tm"
//				value	<=	 time to set clock to. Make sure it is populated with YYMMDDhhmmss at least
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __mktime(void)
{
	char result[50];
	time_t seconds;
	struct tm myTime;
	char * value = IRIS_StackGet(0);
	char * format = IRIS_StackGet(1);

	memset(&myTime, 0, sizeof(myTime));

	if (value && format)
	{
		unsigned long number = atol(value);

		if (strcmp(format, "YYYY") == 0 && number > 1900)
			myTime.tm_year = number - 1900;
		else if (strcmp(format, "YY") == 0 && number < 100)
			myTime.tm_year = number + 100;
		else if (strcmp(format, "MM") == 0 && number <= 12)
			myTime.tm_mon = number - 1;
		else if (strcmp(format, "DD") == 0 && number <= 31)
			myTime.tm_mday = number ;
		else if (strcmp(format, "hh") == 0 && number <= 23)
			myTime.tm_hour = number ;
		else if (strcmp(format, "mm") == 0 && number <= 59)
			myTime.tm_min = number ;
		else if (strcmp(format, "ss") == 0 && number <= 59)
			myTime.tm_sec = number ;
	}

	IRIS_StackPop(3);

	seconds = my_mktime(&myTime);
	sprintf(result, "%lu", seconds);

	IRIS_StackPush(result);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TIME
//
// DESCRIPTION:	converta a time structure to a formatted string
//
// PARAMETERS:	format	<=	Maps the value provided to ne of the elements within "struct tm"
//				value	<=	 time to set clock to. Make sure it is populated with YYMMDDhhmmss at least
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void ____time(char * value, int * which, char * output, int * j)
{
	int k;
	time_t sometime = atol(value);
	struct tm * myTime = my_gmtime(&sometime);

	// Process year 4 digits format first
	while (which[C_YEAR] >= 4)
	{
		sprintf(&output[*j],"%0.4d", myTime->tm_year + 1900);
		(*j) += 4;
		which[C_YEAR] -= 4;
	}

	// Process month 3 digit format second
	while (which[C_MON] >= 3)
	{
		static const char * months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
		sprintf(&output[*j],"%s", months[myTime->tm_mon]);
		(*j) += 3;
		which[C_MON] -= 3;
	}

	// Process the 2-digit formats including the year now
	for (k = 0; k < C_FMT_MAX; k++)
	{
		while (which[k] >= 2)
		{
			sprintf(&output[*j],"%0.2d",	k == C_YEAR? (myTime->tm_year%100):
											(k == C_MON? (myTime->tm_mon + 1):
											(k == C_DAY? myTime->tm_mday:
											(k == C_HOUR? myTime->tm_hour:
											(k == C_MIN? myTime->tm_min:myTime->tm_sec)))));
			(*j) += 2;
			which[k] -= 2;
		}

		if (which[k])
		{
			output[(*j)++] = (k == C_YEAR?'Y':(k == C_MON?'M':(k == C_DAY?'D':(k == C_HOUR?'h':(k == C_MIN?'m':'s')))));
			which[k]--;
		}
	}
}

void __time(void)
{
	int i, j;
	char output[30];
	int which[C_FMT_MAX];

	char * value = IRIS_StackGet(0);
	char * format = IRIS_StackGet(1);
	char current = '\0';

	if (value && format)
	{
		// Initialise
		memset(which, 0, sizeof(which));

		// 
		for (i = 0, j = 0; i < (int) strlen(format); i++)
		{
			if (current && format[i] != current)
				____time(value, which, output, &j);
			current = format[i];

			switch(format[i])
			{
				case 'Y':
					which[C_YEAR]++;
					break;
				case 'M':
					which[C_MON]++;
					break;
				case 'D':
					which[C_DAY]++;
					break;
				case 'h':
					which[C_HOUR]++;
					break;
				case 'm':
					which[C_MIN]++;
					break;
				case 's':
					which[C_SEC]++;
					break;
				default:
					output[j++] = format[i];
					break;
			}
		}

		____time(value, which, output, &j);

		// Pop the function name and parameters from the stack
		IRIS_StackPop(3);

		// Push the result onto the stack
		IRIS_StackPush(output);
	}
	else
	{
		IRIS_StackPop(3);
		IRIS_StackPush(NULL);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()LEAP_ADJ
//
// DESCRIPTION:	Adjusts for leap year missing day. This is used when adding separate time values.
//				eg. one has hh:mm:ss and another has MM:DD and yet another has YY.
//				The system cannot realise that it is adding time to start with and even if it does
//				if the year does not exist, the leap day is not added for month calculations.
//				This is not required when adding hh:mm:ss to DD:MM:YY sincethe system accounts for
//				it automatically.
//				The need for this was required because of a deficiency within the AS2805 standard where
//				the year was not sent in the message and some acquirers passed it on by itself.
//
// PARAMETERS:	time	<=	time in seconds
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __leap_adj(void)
{
	char temp[20];
	char * value = IRIS_StackGet(0);
	time_t myTime;
	struct tm * myTM;

	if (value)
	{
		// Time in seconds
		myTime = atol(value);

		// Convert the seconds to a time structure.
		myTM = my_gmtime(&myTime);

		// If the current year is a leap year and the month is March onwards, then add one
		// day of seconds to the time suppied
		// Bo Yang bug fix for leap year 1/March issue : terminal set the wrong date to 29/Feb
		//if (myTM->tm_year % 4 == 0 && myTM->tm_mon >= 2)
		if (myTM->tm_year % 4 == 0 && ( myTM->tm_mon >= 2 || myTM->tm_mon == 1 && myTM->tm_mday == 29))
			myTime += (24UL * 60UL * 60UL);
	}

	IRIS_StackPop(2);
	IRIS_StackPush(value?ltoa(myTime, temp, 10):NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SLEEP
//
// DESCRIPTION:	Sleeps for the specified milliseconds
//
// PARAMETERS:	time	<=	time in milliseconds
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __sleep(void)
{
	char * value = IRIS_StackGet(0);

	if (value)
		SVC_WAIT(atoi(value));

	IRIS_StackPop(2);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TIMER_START
//
// DESCRIPTION:	Starts a timer in millisecond resolution. Returns the current ticks in milliseconds
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __timer_start(void)
{
	char temp[20];

#ifdef _DEBUG
	ticks = 0;
#else
	ticks = read_ticks();
#endif

	IRIS_StackPop(1);
	IRIS_StackPush(ltoa(ticks, temp, 10));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TIMER_STOP
//
// DESCRIPTION:	Stops the timer. Returns the time difference in milliseconds from the start of the timer.
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __timer_stop(void)
{
	char temp[20];

#ifdef _DEBUG
	unsigned long diff = 2;
#else
	unsigned long ticks2 = read_ticks();
	unsigned long diff;

	if (ticks2 < ticks)
		diff = 0xFFFFFFFF - ticks + ticks2 + 1;
	else
		diff = ticks2 - ticks;
#endif

	IRIS_StackPop(1);
	IRIS_StackPush(ltoa(diff, temp, 10));
}

