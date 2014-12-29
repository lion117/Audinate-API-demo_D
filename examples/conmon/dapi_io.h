/*
 * Created  : August 2012
 * Author   : Andrew White
 * Synopsis : Parsing and writing utilities for dante api types
 *
 * This software is Copyright (c) 2004-2012, Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */

#ifndef _CONMON_DAPI_IO_H
#define _CONMON_DAPI_IO_H

#include "audinate/dante_api.h"

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------

/*
	Render a dante_id64_t as a hex string.

	@param src_id Source identifier
	@param buf Output string buffer. Must be at least 16 characters long.
		Does not include trailing null. If this is required, write one into
		17th character before calling.

	@return pointer to populated buffer
 */
const char *
dante_id64_to_str
(
	const dante_id64_t * src_id,
	char * buf
);

typedef char dante_id64_str_buf_t[DANTE_ID64_LEN * 2 + 1];


/*
	Parse a dante_id64_t from a hex string.

	@param dst_id Destination identifier. Contents undefined on failure.
	@param str Source string. May include leading 0x.  May contain embedded whitespace.
		Trailling characters beyond those needed for id64 are ignored.
 */
aud_error_t
dante_id64_from_hex
(
	dante_id64_t * dst_id,
	const char * str
);


/*
	Render a dante_id64_t as an ASCII string.

	NOTE: a dante identifier is not inherently ASCII.  Characters that cannot be
		represented in ASCII will be rendered as '.'.

	@param src_id Source identifier
	@param buf Output string buffer. Must be at least 8 characters long.
		Does not include trailing null. If this is required, write one into
		9th character before calling.

	@return pointer to populated buffer
 */
const char *
dante_id64_to_ascii
(
	const dante_id64_t * src_id,
	char * buf
);

typedef char dante_id64_ascii_buf_t[DANTE_ID64_LEN + 1];


/*
	Copy a dante_id64_t from an ASCII string.

	@param dst_id Destination identifier. Contents undefined on failure.
	@param str Source string.

	This function copies the first 8 characters of str to dst_id, as long as str
	is at least 8 characters long.  It will not drop leading whitespace, nor handle
	identifiers that contain embedded null characters.
 */
aud_error_t
dante_id64_from_ascii
(
	dante_id64_t * dst_id,
	const char * str
);


//----------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif
