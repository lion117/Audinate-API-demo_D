/*
 * Created  : August 2012
 * Author   : Andrew White
 * Synopsis : Parsing and writing utilities for dante api types
 *
 * This software is Copyright (c) 2004-2012, Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */

//----------------------------------------------------------

#include "dapi_io.h"
#include <ctype.h>

//----------------------------------------------------------


AUD_INLINE char
nibble_to_hexchar(uint8_t nibble)
{
	return nibble + ((nibble <= 9) ? '0' : ('a' - 10));
}

AUD_INLINE void
byte_to_hex_str
(
	uint8_t byte,
	char * hex
)
{
	hex[0] = nibble_to_hexchar(byte >> 4);
	hex[1] = nibble_to_hexchar(byte & 0xF);
}

AUD_INLINE uint8_t
hexchar_to_nibble(char hex)
{
	if ('0' <= hex && hex <= '9')
		return hex - '0';
	else if ('a' <= hex && hex <= 'f')
		return hex - 'a' + 10;
	else if ('A' <= hex && hex <= 'F')
		return hex - 'A' + 10;
	else
		return 0xFF;
}


static const char *
drop_whitespace(const char * str_in)
{
	while(isspace(str_in[0]))
	{
		str_in ++;
	}
	return str_in;
}


const char *
dante_id64_to_str
(
	const dante_id64_t * src_id,
	char * buf
)
{
	unsigned i;
	if (buf)
	{
		for (i = 0; i < DANTE_ID64_LEN; i++)
		{
			byte_to_hex_str(src_id->data[i], buf + i * 2);
		}
	}

	return buf;
}

aud_error_t
dante_id64_from_hex
(
	dante_id64_t * dst_id,
	const char * str
)
{
	unsigned i;

	if (! (str && dst_id))
		return AUD_ERR_INVALIDPARAMETER;

	str = drop_whitespace(str);

	// drop leading 0x, if present
	if (str[0] == '0' && (tolower(str[1]) == 'x'))
	{
		str += 2;
	}

	for (i = 0; i < DANTE_ID64_LEN; i++)
	{
		uint8_t hi, lo;
		if (str[0] == 0)
			return AUD_ERR_TRUNCATED;

		if (isspace(str[0]))
			str = drop_whitespace(str);

		hi = hexchar_to_nibble(str[0]);
		lo = hexchar_to_nibble(str[1]);
		if (hi >= 0x10 || lo >= 0x10)
			return AUD_ERR_INVALIDDATA;
		dst_id->data[i] = (hi << 4) | lo;
		str += 2;
	}

	return AUD_SUCCESS;
}


const char *
dante_id64_to_ascii
(
	const dante_id64_t * src_id,
	char * buf
)
{
	unsigned i;
	if (buf)
	{
		for (i = 0; i < DANTE_ID64_LEN; i++)
		{
			int ch = (int) src_id->data[i];
			if (isprint(ch))
				buf[i] = (char) src_id->data[i];
			else
				buf[i] = '.';
		}
	}

	return buf;
}


aud_error_t
dante_id64_from_ascii
(
	dante_id64_t * dst_id,
	const char * str
)
{
	size_t len;

	if (! (str && dst_id))
		return AUD_ERR_INVALIDPARAMETER;

	len = strlen(str);

	if (len < DANTE_ID64_LEN)
		return AUD_ERR_INVALIDDATA;

	memcpy(dst_id->data, str, DANTE_ID64_LEN);

	return AUD_SUCCESS;
}


//----------------------------------------------------------
