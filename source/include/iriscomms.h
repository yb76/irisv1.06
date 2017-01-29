#ifndef __IRISCOMMS_H
#define __IRISCOMMS_H

/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       iriscomms.h
**
** DATE CREATED:    31 January 2008
**
** AUTHOR:			Tareq Hafez
**
** DESCRIPTION:     Header file for the iris generic communication functions
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Type definitions
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Functions
**-----------------------------------------------------------------------------
*/

void IRIS_CommsSend(T_COMMS * comms, int * retVal);

void IRIS_CommsRecv(T_COMMS * comms, int bufLen, int * retVal);

void IRIS_CommsDisconnect(T_COMMS * comms, int retVal);

void IRIS_CommsErr(int retVal);


#endif /* __IRISCOMMS_H */
