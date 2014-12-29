/*
 * Created  : September 2008
 * Author   : Andrew White <andrew.white@audinate.com>
 * Synopsis : Print various conmon message bodies
 *
 * This software is copyright (c) 2004-2012 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */

#ifndef _CONMON_AUD_PRINT_MESSAGE_H
#define _CONMON_AUD_PRINT_MESSAGE_H


//----------
// Include

#include "audinate/dante_api.h"


//----------
// Types and Constants

typedef void
conmon_aud_print_msg_fn
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
);

//----------
// Functions

aud_bool_t
conmon_aud_print_msg(const conmon_message_body_t * aud_msg, uint16_t body_size);

conmon_aud_print_msg_fn
	conmon_aud_print_msg_idset,
	conmon_aud_print_msg_interface_status,
	conmon_aud_print_msg_switch_vlan_status,
	conmon_aud_print_msg_clocking_status,
	conmon_aud_print_msg_unicast_clocking_status,
	conmon_aud_print_msg_master_status,
	conmon_aud_print_msg_name_id,
	conmon_aud_print_msg_ifstats_status,
	conmon_aud_print_msg_upgrade_status,
	conmon_aud_print_msg_clear_config,
	conmon_aud_print_msg_config_control,
	conmon_aud_print_msg_access_status,
	conmon_aud_print_msg_manf_versions_status,
	conmon_aud_print_msg_edk_board_status,
	conmon_aud_print_msg_igmp_status,
	conmon_aud_print_msg_versions_status,
	conmon_aud_print_msg_srate_status,
	conmon_aud_print_msg_srate_control,
	conmon_aud_print_msg_srate_pullup_status,
	conmon_aud_print_msg_srate_pullup_control,
	conmon_aud_print_msg_enc_status,
	conmon_aud_print_msg_audio_interface_status,
	conmon_aud_print_msg_bool,
	conmon_aud_print_msg_routing_ready,
	conmon_aud_print_print_rx_error_thres_status,
	conmon_aud_print_msg_led_status,
	conmon_aud_print_msg_metering_status,
	conmon_aud_print_msg_serial_port_status,
	conmon_aud_print_msg_haremote_status,
	conmon_aud_print_msg_haremote_stats_status,
	conmon_aud_print_msg_dante_ready,
	conmon_aud_print_msg_ptp_logging_status,
	conmon_aud_print_msg_sys_reset_status,
	conmon_aud_print_msg_gpio_status;


//----------
// Other printing

/*
	Print conmon networks.
	
	If prefix is NULL, print on one line as list [ {a}, {b} ].
	Otherwise, print one per line with leading prefix.
 */
void
conmon_aud_print_networks
(
	const conmon_networks_t * networks,
	const char * prefix
);


//----------

#endif // _CONMON_AUD_PRINT_MESSAGE_H
