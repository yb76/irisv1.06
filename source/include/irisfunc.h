#ifndef __IRISFUNC_H
#define __IRISFUNC_H

/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       irisfunc.h
**
** DATE CREATED:    31 January 2008
**
** AUTHOR:			Tareq Hafez
**
** DESCRIPTION:     Header file for the "time" module
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/
#define	C_NO_OF_IRIS_FUNCTIONS	149

/*
**-----------------------------------------------------------------------------
** Type definitions
**-----------------------------------------------------------------------------
*/
typedef struct
{
	char * name;
	uchar paramCount;
	bool array;
	void (*funcPtr)(void);
} T_IRIS_FUNC;

extern const T_IRIS_FUNC irisFunc[C_NO_OF_IRIS_FUNCTIONS];



/*
**-----------------------------------------------------------------------------
** Functions
**-----------------------------------------------------------------------------
*/

// Time functions
void __now(void);
void __time_set(void);
void __mktime(void);
void __time(void);
void __leap_adj(void);
void __sleep(void);
void __timer_start(void);
void __timer_stop(void);


// Math functions
void __math(void);
void __rand(void);
void __luhn(void);
void __fmt(void);
void __amount(void);
void __len(void);
void __strip(void);
void __pad(void);
void __substring(void);
void __sha1(void);
void __to_ascii_hex(void);
void __to_hex(void);
void __to_safe_hex(void);
void __checksum_xor(void);
void __checksum_sum(void);
void __csv(void);
void __xml(void);
void __xml_attr(void);
void __xml_save_context(void);
void __xml_restore_context(void);
void __dw_encode(void);
void __dw_decode(void);
void __crc_16(void);


// Communication functions
void __pstn_init(void);
void __pstn_connect(void);
void __pstn_send(void);
void __pstn_recv(void);
void __pstn_disconnect(void);
void __pstn_err(void);
void __pstn_clr_err(void);
void __pstn_wait(void);

void __ser_init(void);
void __ser_connect(void);
int __ser_reconnect(int myPort);
void __ser_set(void);
void __ser_send(void);
void __ser_recv(void);
void __ser_data(void);
void __ser_disconnect(void);
int ____ser_disconnect(int myPort);
void __ser_err(void);

void __tcp_init(void);
void __ip_connect(void);
void __tcp_connect(void);
void __tcp_send(void);
void __tcp_recv(void);
void __tcp_disconnect(void);
void __tcp_disconnect_now(void);
void __tcp_disconnect_do(void);
void __tcp_disconnect_ip_only(void);
void __tcp_disconnect_completely(void);
void __tcp_disconnect_check(void);
void __tcp_disconnect_extend(void);
void __tcp_err(void);
void __tcp_clr_err(void);
void __tcp_gprs_sts(void);
void __ping(void);


// AS2805 functions
void __as2805_get(void);
void __as2805_err(void);
void __as2805_ofb(void);
void __as2805_make(void);
void __as2805_make_custom(void);
void __as2805_break(void);
void __as2805_break_custom(void);
void __as2805_bcd_length(void);
void __as2805_ofb_param(void);


// Crypto Functions
void __des_store(void);
void __des_erase(void);
void __des_random(void);
void __des_transfer(void);
void __des_xor(void);
void __iv_set(void);
void __iv_clr(void);
void __crypt(void);
void __mac(void);
void __kvc(void);
void __owf(void);
void __owf_with_data(void);
void __derive_key(void);
void __3des_rsa_store(void);
void __rsa_store(void);
void __rsa_clear(void);
void __rsa_crypt(void);
void __rsa_derive_rsa(void);
void __rsa_derive_3des(void);
void __pinblock(void);
void __dformat1(void);
void __adj_odd_parity(void);


// Configuration functions
void __sec_init(void);
void __ppid(void);
void __ppid_update(void);
void __ppid_remove(void);
void __serial_no(void);
void __manufacturer(void);
void __model(void);


// I/O functions
void __input(void);
void __key(void);
void __errorbeep(void);
void __track2(void);
void __track1(void);
void __track_clear(void);
void __print(void);
void __print_cont(void);
void __println(void);
void __print_raw(void);
void __print_raw_cont(void);
void __print_err(void);
void __reboot(void);
void __shutdown(void);
void __backlight(void);
void __lowPower(void);

// Utility functions
void __locate(void);
void __text_table(void);
void __map_table(void);
void __new_object(void);
void __objects_check(void);
void __faulty_object(void);
void __faulty_group(void);
void __store_objects(void);
void __download_req(void);
void __upload_msg(void);
void __upload_obj(void);
void __download_obj(void);
void __remote(void);
void __prev_object(void);
void __next_object(void);
void __curr_object(void);
void __curr_group(void);
void __curr_version(void);
void __curr_event(void);
void __curr_event_value(void);
void __new_event_value(void);
void __clr_tmp(void);
void __rearm(void);
void __vmac_app(void);
void __battery_status(void);
void __dock_status(void);
void __env_get(void);
void __env_put(void);
void __force_next_object(void);

#endif /* __IRISFUNC_H */
