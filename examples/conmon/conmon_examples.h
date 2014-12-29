/*
 * File     : $RCSfile$
 * Created  : July 2008
 * Updated  : $Date$
 * Author   : James Westendorp <james.westendorp@audinate.com>
 * Synopsis : Common information for the conmon example clients
 *
 * This software is copyright (c) 2004-2009 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */
#ifndef _CONMON_EXAMPLES_H
#define _CONMON_EXAMPLES_H

// include the main API header
#include "audinate/dante_api.h"
#include "conmon_aud_print_msg.h"
#include "conmon_aud_print_control_msg.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Some cross-platfrom niceties
#ifdef WIN32
#define SNPRINTF _snprintf
#define STRCASECMP _stricmp
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SNPRINTF snprintf
#define STRCASECMP strcasecmp
#endif


AUD_INLINE aud_error_t
conmon_example_sleep
(
	const aud_utime_t * at
) {
	if (at == NULL || at->tv_sec < 0 || at->tv_usec < 0)
	{
		return AUD_ERR_INVALIDPARAMETER;
	}
	else
	{
#ifdef WIN32
		DWORD result;
		int ms = (at->tv_sec * 1000) + ((at->tv_usec+999)/1000);
		result = SleepEx(ms, TRUE);
		if (result)
		{
			return aud_error_from_system_error(result);
		}
#else
		if ((at->tv_sec > 0) && sleep(at->tv_sec) > 0)
		{
			return AUD_ERR_INTERRUPTED;
		}
		if ((at->tv_usec > 0) && (usleep(at->tv_usec) < 0))
		{
			return aud_error_get_last();
		}
#endif
	}
	return AUD_SUCCESS;
}

#ifndef MAX
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#endif

#define ONE_SECOND_US 1000000

//----------------------------------------------------------
// Support functions for the example clients
//----------------------------------------------------------

AUD_INLINE aud_error_t
conmon_example_client_process
(
	conmon_client_t * client,
	dante_sockets_t * sockets,
	const aud_utime_t * timeout,
	aud_utime_t * next_action_timeout
) {
	dante_sockets_t temp_sockets = *sockets;
	//fd_set fd;
	int result;
	aud_utime_t temp = *timeout;
	
	result = select(temp_sockets.n, &temp_sockets.read_fds, NULL, NULL, &temp); // in win32, nfds is the NUMBER of sockets
	if (result < 0)
	{
		return aud_error_from_system_error(aud_system_error_get_last());
	}
	else if (result == 0)
	{
		return AUD_ERR_TIMEDOUT;
	}
	return conmon_client_process_sockets(client, &temp_sockets, next_action_timeout);
}

//----------------------------------------------------------
// Printing functions for the example clients
//----------------------------------------------------------

AUD_INLINE const char *
conmon_example_client_state_to_string
(
	conmon_client_state_t state
) {
	switch (state)
	{
	case CONMON_CLIENT_NO_CONNECTION:     return "NO_CONNECTION";
	case CONMON_CLIENT_CONNECTING:        return "CONNECTING";
	case CONMON_CLIENT_CONNECTED:         return "CONNECTED";
	case CONMON_CLIENT_RECONNECT_PENDING: return "RECONNECT_PENDING";
	default:                              return "???";
	}
}

AUD_INLINE const char *
conmon_example_channel_type_to_string
(
	conmon_channel_type_t channel_type
) {
	switch(channel_type)
	{
	case CONMON_CHANNEL_TYPE_CONTROL:    return "control";
	case CONMON_CHANNEL_TYPE_METERING:   return "metering";
	case CONMON_CHANNEL_TYPE_STATUS:     return "status";
	case CONMON_CHANNEL_TYPE_BROADCAST:  return "broadcast";
	case CONMON_CHANNEL_TYPE_LOCAL:      return "local";
	case CONMON_CHANNEL_TYPE_SERIAL:     return "serial";
	case CONMON_CHANNEL_TYPE_KEEPALIVE:  return "keepalive";
	case CONMON_CHANNEL_TYPE_VENDOR_BROADCAST: return "vendor_broadcast";
	case CONMON_CHANNEL_TYPE_MONITORING: return "monitoring";
	default:                             return "unknown";
	}
}

AUD_INLINE const char *
conmon_example_rxstatus_to_string
(
	conmon_rxstatus_t rxstatus
) {
	switch(rxstatus)
	{
	case CONMON_RXSTATUS_NONE:          return "none";
	case CONMON_RXSTATUS_PREPARING:     return "preparing";
	case CONMON_RXSTATUS_RESOLVED:      return "resolved";
	case CONMON_RXSTATUS_UNRESOLVED:    return "unresolved";
	case CONMON_RXSTATUS_UNICAST:       return "unicast";
	case CONMON_RXSTATUS_MULTICAST:     return "multicast";
	case CONMON_RXSTATUS_NO_CONNECTION: return "no connection";
	case CONMON_RXSTATUS_COMMS_ERROR:   return "comms error";
	case CONMON_RXSTATUS_INVALID_REPLY: return "invalid reply";
	case CONMON_RXSTATUS_TX_NO_CHANNEL: return "tx no channel";
	default:                            return "unknown";
	}
}

// A helper function to print a device id to a string buffer
AUD_INLINE char *
conmon_example_device_id_to_string
(
	const conmon_device_id_t * id,
	char * buf,
	size_t len
) {
	SNPRINTF(buf, len,
		"%02x%02x%02x%02x%02x%02x%02x%02x",
		id->data[0], id->data[1],
		id->data[2], id->data[3],
		id->data[4], id->data[5],
		id->data[6], id->data[7]);
	return buf;
}

// A helper function to print an instance id to a string buffer
AUD_INLINE char *
conmon_example_instance_id_to_string
(
	const conmon_instance_id_t * id,
	char * buf,
	size_t len
) {
	if (id)
	{
		SNPRINTF(buf, len,
			"%02x%02x%02x%02x%02x%02x%02x%02x/%04x",
			id->device_id.data[0], id->device_id.data[1],
			id->device_id.data[2], id->device_id.data[3],
			id->device_id.data[4], id->device_id.data[5],
			id->device_id.data[6], id->device_id.data[7],
			id->process_id);
	}
	else
	{
		SNPRINTF(buf, len, "[null]");
	}
	return buf;
}

// A helper function to print a vendor id to a string buffer
AUD_INLINE char *
conmon_example_vendor_id_to_string
(
	const conmon_vendor_id_t * id,
	char * buf,
	size_t len
) {
	SNPRINTF(buf, len,
		"%02x%02x%02x%02x%02x%02x%02x%02x",
		id->data[0], id->data[1],
		id->data[2], id->data[3],
		id->data[4], id->data[5],
		id->data[6], id->data[7]);
	return buf;
}

// A helper function to print a vendor id to a string buffer
AUD_INLINE char *
conmon_example_model_id_to_string
(
	const conmon_audinate_model_id_t * id,
	char * buf,
	size_t len
) {
	SNPRINTF(buf, len,
		"%02x%02x%02x%02x%02x%02x%02x%02x",
		id->data[0], id->data[1],
		id->data[2], id->data[3],
		id->data[4], id->data[5],
		id->data[6], id->data[7]);
	return buf;
}

// A helper function to print a set of endpoint addresses to a string buffer

AUD_INLINE char *
conmon_example_endpoint_addresses_to_string
(
	const conmon_endpoint_addresses_t * addresses,
	char * buf,
	size_t len
) {
	struct in_addr in;
	int i, offset = 0;
	buf[0] = '\0';
	if (addresses)
	{
		if (addresses->num_networks)
		{
			for (i = 0; i < addresses->num_networks; i++)
			{
				in.s_addr = addresses->addresses[i].host;
				offset += SNPRINTF(buf+offset, len-offset, "%c%s:%u",(i ? ',' : '['),
					inet_ntoa(in), addresses->addresses[i].port);
			}
			SNPRINTF(buf+offset, len-offset, "]");
		}
		else
		{
			SNPRINTF(buf, len, "[]");
		}
	}
	return buf;
}

// A helper function to print a set of endpoint addresses to a string buffer
AUD_INLINE char *
conmon_example_networks_to_string
(
	const conmon_networks_t * networks,
	char * buf,
	size_t len
) {
	struct in_addr address;
	struct in_addr netmask;
	struct in_addr dns_server;
	struct in_addr gateway;

	int i, offset = 0;
	buf[0] = '\0';
	if (networks->num_networks)
	{
		for (i = 0; i < networks->num_networks; i++)
		{
			const conmon_network_t * n = networks->networks + i;
			address.s_addr = n->ip_address;
			netmask.s_addr = n->netmask;
			dns_server.s_addr = n->dns_server;
			gateway.s_addr = n->gateway;
			offset += SNPRINTF(buf+offset, len-offset, "%c%d:",
				(i ? ',' : '['),
				n->interface_index);
			if (n->flags & CONMON_NETWORK_FLAG_STATIC)
			{
				offset += SNPRINTF(buf+offset, len-offset, " STATIC");
			}
			offset += SNPRINTF(buf+offset, len-offset, " %s %d %02x:%02x:%02x:%02x:%02x:%02x",
				n->is_up ? "up" : "down", n->link_speed,
				n->mac_address[0], n->mac_address[1], n->mac_address[2],
				n->mac_address[3], n->mac_address[4], n->mac_address[5]);
			offset += SNPRINTF(buf+offset, len-offset, " addr=%s", inet_ntoa(address));
			offset += SNPRINTF(buf+offset, len-offset, " mask=%s", inet_ntoa(netmask));
			offset += SNPRINTF(buf+offset, len-offset, " dns=%s", inet_ntoa(dns_server));
			offset += SNPRINTF(buf+offset, len-offset, " gateway=%s", inet_ntoa(gateway));
		}
		offset += SNPRINTF(buf+offset, len-offset, "]");
	}
	else
	{
		SNPRINTF(buf, len, "[]");
	}
	return buf;
}

AUD_INLINE char *
conmon_example_subscription_infos_to_string
(
	uint16_t num_subscriptions,
	const conmon_subscription_info_t ** subscriptions,
	char * buf,
	size_t len
) {
	int i, offset = 0;
	buf[0] = '\0';
	for (i = 0; i < num_subscriptions; i++)
	{
		char id[128], addrs[1024];
		const conmon_subscription_info_t * s = subscriptions[i];
		conmon_example_instance_id_to_string(&s->instance_id, id, 128);
		conmon_example_endpoint_addresses_to_string(&s->addresses, addrs, 1024);
		offset += SNPRINTF(buf+offset, len-offset, "  %d: %s %s\n", i, id, addrs);
	}
	return buf;
}

AUD_INLINE char *
conmon_example_client_subscriptions_to_string
(
	uint16_t num_subscriptions,
	const conmon_client_subscription_t * const * subscriptions,
	char * buf,
	size_t len
) {
	int i, offset = 0;
	buf[0] = '\0';
	for (i = 0; i < num_subscriptions; i++)
	{
		char id[128], addrs[1024];
		const conmon_client_subscription_t * s = subscriptions[i];
		const char * type  = conmon_example_channel_type_to_string(conmon_client_subscription_get_channel_type(s));
		const char * device = conmon_client_subscription_get_device_name(s);
		const char * rxstatus = conmon_example_rxstatus_to_string(conmon_client_subscription_get_rxstatus(s));

		conmon_example_instance_id_to_string(conmon_client_subscription_get_instance_id(s), id, 128);
		conmon_example_endpoint_addresses_to_string(conmon_client_subscription_get_addresses(s), addrs, 1024);
		offset += SNPRINTF(buf+offset, len-offset, "  %d: %s@%s (%s %s) %s\n", i, type, device, id, addrs, rxstatus);
	}
	return buf;
}

AUD_INLINE char *
conmon_example_metering_peaks_to_string
(
	const conmon_metering_message_peak_t * peaks,
	uint16_t num_peaks,
	char * buf,
	size_t len
) {
	uint16_t c;
	size_t offset = 0;

	buf[0] = '\0';
	for (c = 0; c < num_peaks; c++)
	{
		switch (peaks[c])
		{
		case CONMON_METERING_PEAK_CLIP:
			offset += SNPRINTF(buf+offset, len-offset, " CLIP");
			break;
		case CONMON_METERING_PEAK_MUTE:
			offset += SNPRINTF(buf+offset, len-offset, " MUTE");
			break;
		case CONMON_METERING_PEAK_START_OF_MESSAGE:
			offset += SNPRINTF(buf+offset, len-offset, " SOM");
			break;
		default:
			offset += SNPRINTF(buf+offset, len-offset, " %4.1f",
				conmon_metering_message_peak_to_float(peaks[c]));
		}
	}
	return buf;
}

#endif

