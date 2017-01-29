//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       irisfunc.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module has a list of functions supported and their descriptors
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

//
// Local include files
//
#include "irisfunc.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//
const T_IRIS_FUNC irisFunc[C_NO_OF_IRIS_FUNCTIONS] =
{
	{"()STRIP",				1, false,	__strip},
	{"()PAD",				2, false,	__pad},
	{"()SUBSTRING",			2, false,	__substring},
	{"()TO_AHEX",			1, false,	__to_ascii_hex},
	{"()TO_HEX",			1, false,	__to_hex},
	{"()TO_SAFE_HEX",		1, false,	__to_safe_hex},
	{"()CHECKSUM_XOR",		5, false,	__checksum_xor},
	{"()CHECKSUM_SUM",		5, false,	__checksum_sum},
	{"()CSV",				5, false,	__csv},
	{"()XML",				4, false,	__xml},
	{"()XML_ATTR",			2, false,	__xml_attr},
	{"()XML_SAVE_CONTEXT",	0, false,	__xml_save_context},
	{"()XML_RESTORE_CONTEXT",0,false,	__xml_restore_context},
	{"()TEXT_TABLE",		2, false,	__text_table},
	{"()SHA1",				1, false,	__sha1},
	{"()DW_ENCODE",			1, false,	__dw_encode},
	{"()DW_DECODE",			1, false,	__dw_decode},
	{"()CRC_16",			1, false,	__crc_16},

	{"()NOW",				0, false,	__now},
	{"()TIME_SET",			1, false,	__time_set},
	{"()MKTIME",			2, false,	__mktime},
	{"()TIME",				2, false,	__time},
	{"()LEAP_ADJ",			1, false,	__leap_adj},
	{"()SLEEP",				1, false,	__sleep},
	{"()TIMER_START",		0, false,	__timer_start},
	{"()TIMER_STOP",		0, false,	__timer_stop},

	{"()MUL",				2, false,	__math},
	{"()DIV",				2, false,	__math},
	{"()MOD",				2, false,	__math},
	{"()SUM",				2, false,	__math},
	{"()SUB",				2, false,	__math},
	{"()RAND",				0, false,	__rand},
	{"()LUHN",				1, false,	__luhn},
	{"()FMT",				2, false,	__fmt},
	{"()AMOUNT",			2, false,	__amount},
	{"()LEN",				1, false,	__len},

	{"()LOCATE",			4, false,	__locate},
	{"()MAP_TABLE",			3, false,	__map_table},
	{"()NEW_OBJECT",		2, false,	__new_object},
	{"()OBJECTS_CHECK",		2, false,	__objects_check},
	{"()FAULTY_OBJECT",		0, false,	__faulty_object},
	{"()FAULTY_GROUP",		0, false,	__faulty_group},
	{"()STORE_OBJECTS",		2, false,	__store_objects},
	{"()DOWNLOAD_REQ",		1, false,	__download_req},
	{"()UPLOAD_MSG",		1, false,	__upload_msg},
	{"()UPLOAD_OBJ",		1, false,	__upload_obj},
	{"()DOWNLOAD_OBJ",		1, false,	__download_obj},
	{"()REMOTE",			0, false,	__remote},
	{"()PREV_OBJECT",		0, false,	__prev_object},
	{"()NEXT_OBJECT",		0, false,	__next_object},
	{"()CURR_OBJECT",		0, false,	__curr_object},
	{"()CURR_VERSION",		0, false,	__curr_version},
	{"()CURR_GROUP",		0, false,	__curr_group},
	{"()CURR_EVENT",		0, false,	__curr_event},
	{"()CURR_EVENT_VALUE",	0, false,	__curr_event_value},
	{"()NEW_EVENT_VALUE",	0, false,	__new_event_value},
	{"()CLR_TMP",			1, false,	__clr_tmp},
	{"()REARM",				0, false,	__rearm},
	{"()VMAC_APP",			2, false,	__vmac_app},
	{"()BATTERY_STATUS",	0, false,	__battery_status},
	{"()DOCK_STATUS",		0, false,	__dock_status},
	{"()ENV_GET",			1, false,	__env_get},
	{"()ENV_PUT",			2, false,	__env_put},
	{"()FORCE_NEXT_OBJECT",	1, false,	__force_next_object},

	{"()AS2805_GET",		0, false,	__as2805_get},
	{"()AS2805_ERR",		0, false,	__as2805_err},
	{"()AS2805_OFB",		2, false,	__as2805_ofb},
	{"()AS2805_MAKE",		2, true,	__as2805_make},
	{"()AS2805_MAKE_CUSTOM",2, true,	__as2805_make_custom},
	{"()AS2805_BREAK",		3, true,	__as2805_break},
	{"()AS2805_BREAK_CUSTOM",3,true,	__as2805_break_custom},
	{"()AS2805_BCD_LEN",	1, false,	__as2805_bcd_length},
	{"()AS2805_OFB_PARAM",	3, false,	__as2805_ofb_param},

	{"()DES_STORE",			3, false,	__des_store},
	{"()DES_ERASE",			2, false,	__des_erase},
	{"()DES_RANDOM",		2, false,	__des_random},
	{"()DES_COPY",			3, false,	__des_transfer},
	{"()DES_MOVE",			3, false,	__des_transfer},
	{"()DES_XOR",			4, false,	__des_xor},
	{"()IV_SET",			1, false,	__iv_set},
	{"()IV_CLR",			0, false,	__iv_clr},
	{"()ENC",				3, false,	__crypt},
	{"()ENCV",				4, false,	__crypt},
	{"()DEC",				3, false,	__crypt},
	{"()DECV",				4, false,	__crypt},
	{"()OFB",				3, false,	__crypt},
	{"()MAC",				3, false,	__mac},
	{"()KVC",				2, false,	__kvc},
	{"()OWF",				3, false,	__owf},
	{"()OWF_VARIANT",		3, false,	__owf},
	{"()OWF_WITH_DATA",		4, false,	__owf_with_data},
	{"()DERIVE_DES",		5, false,	__derive_key},
	{"()DERIVE_3DES",		5, false,	__derive_key},
	{"()3DES_RSA_STORE",	4, false,	__3des_rsa_store},
	{"()RSA_STORE",			2, false,	__rsa_store},
	{"()RSA_CLEAR",			1, false,	__rsa_clear},
	{"()RSA_CRYPT",			2, false,	__rsa_crypt},
	{"()RSA_DERIVE_RSA",	4, false,	__rsa_derive_rsa},
	{"()RSA_DERIVE_3DES",	4, false,	__rsa_derive_3des},
	{"()PINBLOCK",			5, false,	__pinblock},
	{"()DFORMAT1",			2, false,	__dformat1},
	{"()ADJ_ODD_PARITY",	1, false,	__adj_odd_parity},

	{"()SEC_INIT",			0, false,	__sec_init},
	{"()PPID",				0, false,	__ppid},
	{"()PPID_UPDATE",		1, false,	__ppid_update},
	{"()PPID_REMOVE",		0, false,	__ppid_remove},
	{"()SERIAL_NO",			0, false,	__serial_no},
	{"()MANUFACTURER",		0, false,	__manufacturer},
	{"()MODEL",				0, false,	__model},

	{"()PSTN_CONNECT",		11,false,	__pstn_connect},
	{"()PSTN_SEND",			1, false,	__pstn_send},
	{"()PSTN_RECV",			2, false,	__pstn_recv},
	{"()PSTN_DISCONNECT",	0, false,	__pstn_disconnect},
	{"()PSTN_ERR",			0, false,	__pstn_err},
	{"()PSTN_CLR_ERR",		0, false,	__pstn_clr_err},
	{"()PSTN_WAIT",			0, false,	__pstn_wait},

	{"()SER_CONNECT",		8, false,	__ser_connect},
	{"()SER_SET",			5, false,	__ser_set},
	{"()SER_SEND",			2, false,	__ser_send},
	{"()SER_RECV",			3, false,	__ser_recv},
	{"()SER_DATA",			1, false,	__ser_data},
	{"()SER_DISCONNECT",	1, false,	__ser_disconnect},
	{"()SER_ERR",			0, false,	__ser_err},

	{"()IP_CONNECT",		5, false,	__ip_connect},
	{"()TCP_CONNECT",		10, false,	__tcp_connect},
	{"()TCP_SEND",			1, false,	__tcp_send},
	{"()TCP_RECV",			2, false,	__tcp_recv},
	{"()TCP_DISCONNECT",	0, false,	__tcp_disconnect},
	{"()TCP_DISCONNECT_NOW",0, false,	__tcp_disconnect_now},
	{"()TCP_ERR",			0, false,	__tcp_err},
	{"()TCP_CLR_ERR",		0, false,	__tcp_clr_err},
	{"()TCP_GPRS_STS",		0, false,	__tcp_gprs_sts},
	{"()PING",				1, false,	__ping},

	{"()INPUT",				0, false,	__input},
	{"()KEY",				0, false,	__key},
	{"()ERRORBEEP",			0, false,	__errorbeep},
	{"()TRACK1",			0, false,	__track1},
	{"()TRACK2",			0, false,	__track2},
	{"()TRACK_CLEAR",		0, false,	__track_clear},
	{"()PRINT",				1, false,	__print},
	{"()PRINT_CONT",		1, false,	__print_cont},
	{"()PRINTLN",			1, false,	__println},
	{"()PRINT_RAW",			1, false,	__print_raw},
	{"()PRINT_RAW_CONT",	1, false,	__print_raw_cont},
	{"()PRINT_ERR",			0, false,	__print_err},
	{"()REBOOT",			0, false,	__reboot},
	{"()SHUTDOWN",			0, false,	__shutdown},
	{"()BACKLIGHT",			1, false,	__backlight},
	{"()LOW_POWER",			0, false,	__lowPower},
};

//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//

