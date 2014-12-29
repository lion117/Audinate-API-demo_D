/*
 * Created  : August 2012
 * Author   : Andrew White <andrew.white@audinate.com>
 * Synopsis : Shared code for conmon_aud_print_message family
 *
 * This software is copyright (c) 2004-2012 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */

#ifndef _CONMON_AUD_PRINT_MESSAGE_INTERNAL_H
#define _CONMON_AUD_PRINT_MESSAGE_INTERNAL_H


//----------
// Include

#include "conmon_aud_print_msg.h"


//----------
// Types and Constants

typedef struct conmon_aud_print_msg
{
	uint16_t type;
	conmon_aud_print_msg_fn * print;
		// May be NULL
	const char * typename;
		// If NULL, indicates end of table
	const char * name;
		// May be NULL
} conmon_aud_print_msg_t;


//----------
// Functions

void
conmon_aud_print_id64
(
	const dante_id64_t * src_id
);

void
conmon_aud_print_upgrade_file_info
(
	const conmon_audinate_upgrade_source_file_t * file_info
);


//----------

#endif // _CONMON_AUD_PRINT_MESSAGE_INTERNAL_H
