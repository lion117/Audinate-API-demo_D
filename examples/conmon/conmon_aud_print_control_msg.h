/*
 * Created  : August 2012
 * Author   : Andrew White <andrew.white@audinate.com>
 * Synopsis : Print conmon control message bodies
 *
 * This software is copyright (c) 2004-2012 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */

#ifndef _CONMON_AUD_PRINT_CONTROL_MESSAGE_H
#define _CONMON_AUD_PRINT_CONTROL_MESSAGE_H


//----------
// Include

#include "conmon_aud_print_msg.h"


//----------
// Types and Constants


//----------
// Functions

void
conmon_aud_print_control_msg(const conmon_message_body_t * aud_msg, const uint16_t body_size);

conmon_aud_print_msg_fn
	conmon_aud_print_msg_upgrade3_control
	;


//----------

#endif // _CONMON_AUD_PRINT_CONTROL_MESSAGE_H
