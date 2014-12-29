#include "conmon_examples.h"

#ifdef WIN32
#include "conio.h"
#else
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#endif

aud_bool_t g_running = AUD_TRUE;
aud_bool_t g_sockets_changed = AUD_TRUE;
conmon_client_request_id_t g_req_id = CONMON_CLIENT_NULL_REQ_ID;
aud_errbuf_t g_errbuf;

// Vendor id for the example clients, can be overwritten
conmon_vendor_id_t g_tx_vendor_id = {{'e', 'x', 'a', 'm', 'p', 'l', 'e', '\0'}};
conmon_vendor_id_t * g_rx_vendor_id = NULL;
aud_bool_t g_print_payloads = AUD_FALSE;


typedef struct
{
	uint16_t id;
	const char * name;
} name_map_t;


const name_map_t CHANNEL_NAMES[] = 
{
	{CONMON_CHANNEL_TYPE_CONTROL,   "control"},
	{CONMON_CHANNEL_TYPE_METERING,  "metering"},
	{CONMON_CHANNEL_TYPE_STATUS,    "status"},
	{CONMON_CHANNEL_TYPE_BROADCAST, "broadcast"},
	{CONMON_CHANNEL_TYPE_LOCAL,     "local"},
	{CONMON_CHANNEL_TYPE_SERIAL,    "serial"},
	{CONMON_CHANNEL_TYPE_KEEPALIVE, "keepalive"},
	{CONMON_CHANNEL_TYPE_VENDOR_BROADCAST, "vbroadcast"},
	{CONMON_CHANNEL_TYPE_MONITORING, "monitoring"}
};

AUD_INLINE const char *
channel_type_to_string(conmon_channel_type_t type)
{
	return CHANNEL_NAMES[type].name;
}

AUD_INLINE conmon_channel_type_t
channel_type_from_string(const char * name)
{
	conmon_channel_type_t i; 
	for (i = 0; i < CONMON_NUM_CHANNELS; i++)
	{
		if (!STRCASECMP(name, CHANNEL_NAMES[i].name))
		{
			return CHANNEL_NAMES[i].id;
		}
	}
	return CONMON_CHANNEL_TYPE_NONE;
}

AUD_INLINE const char *
channel_direction_to_string(conmon_channel_direction_t dir)
{
	return dir == CONMON_CHANNEL_DIRECTION_TX ? "TX" : "RX";
}

static conmon_client_connection_state_changed_fn handle_connection_state_changed;
static conmon_client_sockets_changed_fn handle_sockets_changed;
static conmon_client_handle_networks_changed_fn handle_networks_changed;
static conmon_client_handle_dante_device_name_changed_fn handle_dante_device_name_changed;
static conmon_client_handle_dns_domain_name_changed_fn handle_dns_domain_name_changed;
static conmon_client_handle_subscriptions_changed_fn handle_subscriptions_changed;

static conmon_client_handle_control_message_fn handle_control_message;
static conmon_client_handle_monitoring_message_fn handle_monitoring_message;

static conmon_client_response_fn 
	handle_connect_response,
	handle_response;

static void 
handle_message
(
	conmon_client_t * client,
	conmon_channel_type_t channel_type,
	conmon_channel_direction_t channel_direction,
	const conmon_message_head_t * head,
	const conmon_message_body_t * body
) {
	conmon_instance_id_t instance_id;
	const conmon_vendor_id_t * vid = conmon_message_head_get_vendor_id(head);
	char buf[BUFSIZ];

	AUD_UNUSED(body);

	if (g_rx_vendor_id && !conmon_vendor_id_equals(g_rx_vendor_id, vid))
	{
		return;
	}

	if (channel_type == CONMON_CHANNEL_TYPE_CONTROL)
	{
		printf("%s:\n", channel_type_to_string(channel_type));
	}
	else
	{
		printf("%s %s:\n", channel_direction_to_string(channel_direction), channel_type_to_string(channel_type));
	}

	conmon_message_head_get_instance_id(head, &instance_id);
	printf("    body_size=0x%04x seq=0x%04x class=0x%04x\n",
		conmon_message_head_get_body_size(head),
		conmon_message_head_get_seqnum(head), 
		conmon_message_head_get_message_class(head));

	printf("    source=%s (%s)\n",
		conmon_example_instance_id_to_string(&instance_id, buf, BUFSIZ),
		conmon_client_device_name_for_instance_id(client, &instance_id));
	printf("    vendor=%s\n", conmon_example_vendor_id_to_string(vid, buf, BUFSIZ));
	
	if (g_print_payloads)
	{
		uint16_t i, len = conmon_message_head_get_body_size(head);
		for (i = 0; i < len; i++)
		{
			if (!body->data[i] || !isprint(body->data[i]))
			{
				break;
			}
		}
		if (i == len)
		{
			printf("    body=\"");
			for (i = 0; i < len; i++)
			{
				printf("%c", body->data[i]);
			}
			printf("\"\n");
		}
		else
		{
			printf("    body=0x");
			for (i = 0; i < len; i++)
			{
				printf("%02x", body->data[i]);
			}
			printf("\n");

		}
	}
}

static void
print_subscription
(
	const conmon_client_subscription_t * sub
) {
	unsigned int n;
	char buf[64];
	uint16_t active = conmon_client_subscription_get_connections_active(sub);
	uint16_t available = conmon_client_subscription_get_connections_available(sub);
	printf("%s@%s: status=%s id=%s connections=",
		channel_type_to_string(conmon_client_subscription_get_channel_type(sub)),
		conmon_client_subscription_get_device_name(sub),
		conmon_example_rxstatus_to_string(conmon_client_subscription_get_rxstatus(sub)),
		conmon_example_instance_id_to_string(conmon_client_subscription_get_instance_id(sub), buf, 64)
		);
	for (n = 0; n < CONMON_MAX_NETWORKS; n++)
	{
		if (available & (1 << n))
		{
			printf("%c", (active & (1 << n)) ? '+' : '-');
		}
	}
}

static void
handle_connection_state_changed
(
	conmon_client_t * client
) {
	printf("  Connection state changed, now %s\n", conmon_example_client_state_to_string(conmon_client_state(client)));
}

static void
handle_networks_changed
(
	conmon_client_t * client
) {
	char buf[BUFSIZ];
	printf("  Networks changed, now %s\n", conmon_example_networks_to_string(conmon_client_get_networks(client), buf, sizeof(buf)));
}

static void
handle_sockets_changed
(
	conmon_client_t * client
) {
	printf("  Conmon client sockets changed, now \"%s\"\n", conmon_client_get_dante_device_name(client));
	g_sockets_changed = AUD_TRUE;
}

static void
handle_dante_device_name_changed
(
	conmon_client_t * client
) {
	printf("  Dante device name changed, now \"%s\"\n", conmon_client_get_dante_device_name(client));
}


static void handle_dns_domain_name_changed
(
	conmon_client_t * client
) {
	printf("  DNS domain name changed, now \"%s\"\n", conmon_client_get_dns_domain_name(client));
}


static void
handle_subscriptions_changed
(
	conmon_client_t * client,
	unsigned int num_changes,
	const conmon_client_subscription_t * const * changes
) {
	aud_utime_t now;
	unsigned int i;

	AUD_UNUSED(client);

	aud_utime_get(&now);
	printf("  %u.%u: Subscriptions changed:\n", (unsigned int) now.tv_sec, (unsigned int) now.tv_usec);
	for (i = 0; i < num_changes; i++)
	{
		printf("    ");
		print_subscription(changes[i]);
		printf("\n");
	}
}

static void
handle_control_message
(
	conmon_client_t * client,
	const conmon_message_head_t * head,
	const conmon_message_body_t * body
) {
	handle_message(client, CONMON_CHANNEL_TYPE_CONTROL, CONMON_CHANNEL_DIRECTION_RX, head, body);
}

static void 
handle_monitoring_message
(
	conmon_client_t * client,
	conmon_channel_type_t channel_type,
	conmon_channel_direction_t channel_direction,
	const conmon_message_head_t * head,
	const conmon_message_body_t * body
) {
	handle_message(client, channel_type, channel_direction, head, body);
}

static void
list_subscriptions(conmon_client_t * client)
{
	uint16_t i, max = conmon_client_max_subscriptions(client);
	printf ("Max subscriptions: %d\n", max);
	for (i = 0; i < max; i++)
	{
		const conmon_client_subscription_t * sub = conmon_client_subscription_at_index(client, i);
		if (sub)
		{
			printf("  %d: ", i);
			print_subscription(sub);
			printf("\n");
		}
	}
}

static void
print_commands(char c)
{
	printf("Usage:\n");
	if (c == '\0' || c == 'r' || c == 'd')
	{
		printf(" [r|d] control           register/deregister for control messages\n");
		printf(" [r|d] [t|r]x metering   register/deregister for tx/rx metering address information\n");
		printf(" [r|d] [t|r]x status     register/deregister for tx/rx messages on status channel\n");
		printf(" [r|d] [t|r]x broadcast  register/deregister for tx/rx messages on broadcast channel\n");
		printf(" [r|d] local             register/deregister for messages on the local channel\n");
		printf(" [r|d] serial            register/deregister for messages on the serial channel\n");
		printf(" [r|d] [t|r]x vbroadcast register/deregister for tx/rx messages on vendor broadcast channel\n");
	}
	if (c == '\0' || c == 's' || c == 'u')
	{
		printf(" [s|u] CHANNEL DEVICE  subscribe to / unsubscribe from channel CHANNEL on device DEVICE\n");
		printf(" [s|u] CHANNEL         subscribe to / unsubscribe from channel CHANNEL for ALL devices\n");
		printf(" s                     list subscriptions\n");
	}
	if (c == '\0' || c == 'e')
	{
		printf(" e +                   enable Tx Metering\n");
		printf(" e -                   disable Tx Metering\n");
	}
	if (c == '\0' || c == 'm')
	{
		printf(" m control DEVICE MSG  send a control message to device DEVICE with payload MSG\n");
		printf(" m CHANNEL MSG         send a message on status channel CHANNEL with payload MSG\n");
	}
	if (c == '\0' || c == 'l')
	{
		printf(" a [+|-]               enabled / disable remote control access\n");
	}
	if (c == '0' || c == 'i')
	{
		printf(" i DEVICE PROCESS      get name for instance id DEVICE/PROCESS\n");
		printf(" i NAME                get instance id for NAME\n");
	}
	if (c == '\0')
	{
		printf(" ?                     help\n");
		printf(" q                     quit\n");
	}
}

static conmon_client_request_id_t
process_line(char * buf, conmon_client_t * client)
{
	char c, d, sign;
	char channel[BUFSIZ];
	char device[BUFSIZ];
	conmon_channel_type_t channel_type;
	conmon_channel_direction_t channel_direction;
	conmon_name_t device_name;
	conmon_message_body_t body;
	unsigned int process_id;
	
	aud_error_t result = AUD_SUCCESS;
	conmon_client_request_id_t req_id = CONMON_CLIENT_NULL_REQ_ID;

	if (!buf)
	{
		return req_id;
	}
	while (isspace(*buf))
	{
		buf++;
	}
	if (*buf == '\0')
	{
		return req_id;
	}

	if (sscanf(buf, "%c %cx %s", &c, &d, channel) == 3 && (c == 'r' || c == 'd') && (d == 'r' || d == 't'))
	{
		channel_type = channel_type_from_string(channel);
		channel_direction = (d == 't' ? CONMON_CHANNEL_DIRECTION_TX : CONMON_CHANNEL_DIRECTION_RX);
		if (c == 'r')
		{
			printf("Registering for %cx messages on channel %s\n", d, channel);
			result =
				conmon_client_register_monitoring_messages (
					client, & handle_response, & req_id,
					channel_type, channel_direction, handle_monitoring_message
				);
		}
		else
		{
			printf("Deregistering for %cx messages on channel %s\n", d, channel);
			result =
				conmon_client_register_monitoring_messages (
					client, & handle_response, & req_id,
					channel_type, channel_direction, NULL
				);
		}
	}
	else if (sscanf(buf, "%c %s", &c, channel) == 2 && (c == 'r' || c == 'd') && !strcmp(channel, "serial"))
	{
		if (c == 'r')
		{
			printf("Registering for serial messages\n");
			result =
					conmon_client_register_monitoring_messages (
						client, & handle_response, & req_id,
						CONMON_CHANNEL_TYPE_SERIAL, CONMON_CHANNEL_DIRECTION_RX, handle_monitoring_message
					);
		}
		else
		{
			printf("Deregistering for serial messages\n");
			result =
					conmon_client_register_monitoring_messages (
						client, & handle_response, & req_id,
						CONMON_CHANNEL_TYPE_SERIAL, CONMON_CHANNEL_DIRECTION_RX, NULL
					);
		}
	}
	else if (sscanf(buf, "%c %s", &c, channel) == 2 && (c == 'r' || c == 'd') && !strcmp(channel, "local"))
	{
		if (c == 'r')
		{
			printf("Registering for local messages\n");
			result =
					conmon_client_register_monitoring_messages (
						client, & handle_response, & req_id,
						CONMON_CHANNEL_TYPE_LOCAL, CONMON_CHANNEL_DIRECTION_TX, handle_monitoring_message
					);
		}
		else
		{
			printf("Deregistering for local messages\n");
			result =
					conmon_client_register_monitoring_messages (
						client, & handle_response, & req_id,
						CONMON_CHANNEL_TYPE_LOCAL, CONMON_CHANNEL_DIRECTION_TX, NULL
					);
		}
	}
	else if (sscanf(buf, "%c %s", &c, channel) == 2 && (c == 'r' || c == 'd') && !strcmp(channel, "control"))
	{
		if (c == 'r')
		{
			printf("Registering for control messages\n");
			result =
				conmon_client_register_control_messages (
					client, & handle_response, & req_id,
					handle_control_message
				);
		}
		else
		{
			printf("Deregistering for control messages\n");
			result =
				conmon_client_register_control_messages (
					client, & handle_response, & req_id,
					NULL
				);
		}
	}
	else if (sscanf(buf, "m control %s %[^\r\n]*", device_name, body.data) == 2)
	{
		// create a control message and send it...
		uint16_t body_size = (uint16_t) strlen((char *) body.data);
		
		printf("Sending a control message of size %d to device %s\n", body_size, device_name);
		result =
			conmon_client_send_control_message (
				client, & handle_response, & req_id,
				device_name, CONMON_MESSAGE_CLASS_VENDOR_SPECIFIC, &g_tx_vendor_id,
				&body, body_size, NULL
			);
	}
	else if (sscanf(buf, "m %s %[^\r\n]*", channel, body.data) == 2)
	{
		channel_type = channel_type_from_string(channel);
		if (channel_type == CONMON_CHANNEL_TYPE_CONTROL)
		{
			// create a message and send it...
			uint16_t body_size = (uint16_t) strlen((char *) body.data);
			
			printf("Sending a control message of size %d to local device\n", body_size);
			result =
				conmon_client_send_control_message (
					client, & handle_response, & req_id,
					NULL, CONMON_MESSAGE_CLASS_VENDOR_SPECIFIC, &g_tx_vendor_id,
					&body, body_size, NULL
				);
		}
		else if (channel_type >= CONMON_CHANNEL_TYPE_METERING && channel_type < CONMON_NUM_CHANNELS)
		{
			// create a message and send it...
			uint16_t body_size = (uint16_t) (strlen((char *) body.data) + 1);

			printf("Sending a message of size %d to channel %s\n", body_size, channel);
			result =
				conmon_client_send_monitoring_message (
					client, & handle_response, & req_id,
					channel_type, CONMON_MESSAGE_CLASS_VENDOR_SPECIFIC, &g_tx_vendor_id,
					&body, body_size
				);
		}
		else
		{
			print_commands('c');
			return req_id;
		}
	}
	else if (sscanf(buf, "%c %s %s", &c, channel, device_name) == 3 && (c == 's' || c == 'u'))
	{
		channel_type = channel_type_from_string(channel);
		if (channel_type != CONMON_CHANNEL_TYPE_NONE && channel_type != CONMON_CHANNEL_TYPE_CONTROL)
		{
			if (c == 's')
			{
				printf("Subscribing to %s@%s\n", channel, device_name);
				result =
					conmon_client_subscribe (
						client, & handle_response, & req_id,
						channel_type, device_name
					);
			}
			else
			{
				printf("Unsubscribing from %s@%s\n", channel, device_name);
				result =
					conmon_client_unsubscribe (
						client, & handle_response, & req_id,
						channel_type, device_name
					);
			}
		}
		else
		{
			print_commands(c);
			return req_id;
		}
	}
	else if (sscanf(buf, "%c %s", &c, channel) == 2 && (c == 's' || c == 'u'))
	{
		channel_type = channel_type_from_string(channel);
		if (channel_type != CONMON_CHANNEL_TYPE_NONE && channel_type != CONMON_CHANNEL_TYPE_CONTROL)
		{
			if (c == 's')
			{
				printf("Subscribing to all for %s\n", channel);
				result =
					conmon_client_subscribe_global (
						client, & handle_response, & req_id,
						channel_type
					);
			}
			else
			{
				printf("Unsubscribing from all on %s\n", channel);
				result =
					conmon_client_unsubscribe_global (
						client, & handle_response, & req_id,
						channel_type
					);
			}
		}
		else
		{
			print_commands(c);
			return req_id;
		}
	}
	else if (sscanf(buf, "%c %c", &c, &d) == 2 && (c == 'a') && (d == '+' || d == '-'))
	{
		if (d == '+')
		{
			result = conmon_client_set_remote_control_allowed(client, &handle_response, &req_id, AUD_TRUE);
		}
		else if (d == '-')
		{
			result = conmon_client_set_remote_control_allowed(client, &handle_response, &req_id, AUD_FALSE);
		}
	}
	else if (sscanf(buf, "%c %s %u", &c, device, &process_id) == 3 && (c == 'i'))
	{
		// id -> name
		const char * name;
		conmon_instance_id_t instance_id;
		if (!conmon_device_id_from_str(&instance_id.device_id, device))
		{
			// invalid format
			printf("Invalid device id format\n");
		}
		instance_id.process_id = (conmon_process_id_t) process_id;
		name = conmon_client_device_name_for_instance_id(client, &instance_id);

		if (name)
		{
			printf("Device '%s' has instance id %s/%04d\n", name, device, process_id);
		}
		else
		{
			printf("No known device with instance id %s/%04d\n", device, process_id);
		}
		return req_id;
	}
	else if (sscanf(buf, "%c %c", &c, &sign) == 2 && (c == 'e') && (sign == '+'))
	{
		// enable tx metering
		conmon_client_set_tx_metering_enabled(client, &handle_response, &req_id, AUD_TRUE);
	}
	else if (sscanf(buf, "%c %c", &c, &sign) == 2 && (c == 'e') && (sign == '-'))
	{
		// disable tx metering
		conmon_client_set_tx_metering_enabled(client, &handle_response, &req_id, AUD_FALSE);
	}
	else if (sscanf(buf, "%c", &c) == 1)
	{
		if (c == 's')
		{
			list_subscriptions(client);
			return req_id;
		}
		else if (c == '?')
		{
			print_commands('\0');
			return req_id;
		}
		else if (c == 'q')
		{
			g_running = AUD_FALSE;
			return req_id;
		}
		else
		{
			print_commands('\0');
			return req_id;
		}
	}
	else
	{
		print_commands('\0');
		return req_id;
	}

	if (result == AUD_SUCCESS)
	{
		printf("Action has request id %p\n", req_id);
	}
	else
	{
		printf("Error performing action: %s\n" , aud_error_message(result, g_errbuf));
	}
	return req_id;
}

static const aud_utime_t ONE_MS = {0, 1000};

static aud_error_t
main_loop(conmon_client_t * client)
{
	aud_error_t result;
	aud_utime_t next_action_time = {0,0};
	dante_sockets_t all_sockets;
	aud_bool_t print_prompt = AUD_TRUE;

#ifdef  _WIN32
	// set to line buffered mode.
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT);
#endif

	conmon_client_get_next_action_time(client, &next_action_time);

	while(g_running)
	{
		dante_sockets_t select_sockets;
		char buf[BUFSIZ];
		int select_result;
		struct timeval timeout;

		// print prompt if needed
		if (print_prompt)
		{
			printf("\n>>> ");
			fflush(stdout);
			print_prompt = AUD_FALSE;
		}

		// update sockets if needed
		if (g_sockets_changed)
		{
			dante_sockets_clear(&all_sockets);
			conmon_client_get_sockets(client, &all_sockets);
#ifndef _WIN32
			dante_sockets_add_read(&all_sockets, 0); // 0 is always stdin
#endif
			g_sockets_changed = AUD_FALSE;
		}

		// prepare sockets and timeouts
		// default (and max) is half a second...
		select_sockets = all_sockets;

		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;
		if (next_action_time.tv_sec || next_action_time.tv_usec)
		{
			aud_utime_t now, temp;
			aud_utime_get(&now);
			temp = next_action_time;
			aud_utime_sub(&temp, &now);

			// don't go for more than 500 ms even if there's nothing to do
			// before then since we'll lost responsiveness
			if (temp.tv_sec == 0 && temp.tv_usec < 500000)
			{
				timeout = temp;
			}
		}

		// drive select loop
		if (select_sockets.n)
		{
			select_result = select(select_sockets.n, &select_sockets.read_fds, NULL, NULL, &timeout);
		}
		else
		{
			conmon_example_sleep(&timeout);
			select_result = 0;
		}

		if (select_result < 0)
		{
			result = aud_error_from_system_error(select_result);
			if (result == AUD_ERR_INTERRUPTED)
			{
				continue;
			}
			printf("Error select()ing: %s\n", aud_error_message(result, g_errbuf));
			return result;
		}
		else if (select_result > 0 || next_action_time.tv_sec || next_action_time.tv_usec)
		{
			result = conmon_client_process_sockets(client, &select_sockets, &next_action_time);
			if (result != AUD_SUCCESS)
			{
				return result;
			}
		}

		// and check stdin 
		buf[0] = '\0';
#ifdef _WIN32
		if (_kbhit())
		{
			DWORD len = 0;
			if (!ReadConsoleA(GetStdHandle(STD_INPUT_HANDLE),buf,BUFSIZ-1, &len, 0))
			{
				printf("Error reading console: %d\n", GetLastError());
			}
			else if (len > 0)
			{
				buf[len] = '\0';
			}
			print_prompt = AUD_TRUE;
		}
#else
		if (FD_ISSET(0, &select_sockets.read_fds)) // 0 is always stdin
		{
			if (fgets(buf, BUFSIZ, stdin) == NULL)
			{
				result = aud_error_get_last();
				if (feof(stdin))
				{
					printf("Exiting...\n");
					return AUD_SUCCESS;
				}
				else if (result == AUD_ERR_INTERRUPTED)
				{
					clearerr(stdin);
				}
				else
				{
					printf("Exiting with %s\n", aud_error_message(result, g_errbuf));
					return result;
				}
			}
			print_prompt = AUD_TRUE;
		}
#endif

		// if we got some input then process the line
		if (buf[0])
		{
		#ifdef _WIN32
			printf("\n");
		#endif
			g_req_id = process_line(buf, client);
			while (g_running && g_req_id != CONMON_CLIENT_NULL_REQ_ID)
			{
				conmon_example_sleep(&ONE_MS); // just to yield...
				conmon_client_process (client);
			}
		}
	}
	return AUD_SUCCESS;
}

static void usage(char * bin)
{
	printf("Usage: %s OPTIONS\n", bin);
	printf("  -a auto-connect\n");
	printf("  -n=NAME set client name\n");
	printf("  -i=FILE read FILE as input\n");
	printf("  -p=PORT set server port\n");
	printf("  -vid=ID set vendor id. Used for transmitted packets\n");
	printf("  -vba=A.B.C.D Set vendor broadcast channel address\n");
	printf("  -vrx Filter rx packets by transmit vendor id\n");
	printf("  -pp print payloads\n");
}

int main(int argc, char * argv[])
{
	aud_error_t result;
	const char * client_name = "console_client";
	char * input_filename = NULL;
	int a;
	uint16_t server_port = 0;
	aud_bool_t auto_connect = AUD_FALSE;

	uint32_t vendor_broadcast_address = 0;

	conmon_client_config_t * config = NULL;
	conmon_client_t * client = NULL;
	aud_env_t * env = NULL;

	printf("console client, build timestamp %s %s\n", __DATE__, __TIME__);

	result = aud_env_setup (& env);
	if (result != AUD_SUCCESS)
	{
		printf("Error initialising conmon client library: %s\n",
			aud_error_message(result, g_errbuf));
		goto cleanup;
	}
	//aud_log_set_threshold(env->log, AUD_LOGTYPE_STDERR, AUD_LOG_DEBUG);

	for (a = 1; a < argc; a++)
	{
		char * arg = argv[a];
		if (!strcmp(arg, "-a"))
		{
			auto_connect = AUD_TRUE;
		}
		else if (!strncmp(arg, "-n=",3) && strlen(arg) > 3)
		{
			client_name = arg + 3;
		}
		else if (!strncmp(arg, "-i=", 3) && strlen(arg) > 3)
		{
			input_filename = argv[a] + 3;
		}
		else if (!strncmp(arg, "-p=",3) && strlen(arg) > 3)
		{
			server_port = (uint16_t) atoi(argv[a] + 3);
		}
		else if (!strncmp(arg, "-vid=",5) && strlen(arg) > 5)
		{
			unsigned int hex[8];
			int i;
			if (sscanf(arg+5, "%02x%02x%02x%02x%02x%02x%02x%02x",
				 hex + 0, hex + 1, hex + 2, hex + 3,
				 hex + 4, hex + 5, hex + 6, hex + 7) < 8)
			{
				usage(argv[0]);
				exit(1);
			}
			for (i = 0; i < 8; i++)
			{
				g_tx_vendor_id.data[i] = (uint8_t) hex[i];
			}
		}
		else if (!strncmp(arg, "-vba=", 5) && strlen(arg) > 5)
		{
			unsigned int ip[4];
			uint8_t * ip8 = (uint8_t *) &vendor_broadcast_address;
			if (sscanf(arg+5, "%u.%u.%u.%u", ip+0, ip+1, ip+2, ip+3) < 4)
			{
				usage(argv[0]);
				exit(1);
			}
			ip8[0] = (uint8_t) ip[0];
			ip8[1] = (uint8_t) ip[1];
			ip8[2] = (uint8_t) ip[2];
			ip8[3] = (uint8_t) ip[3];
		}
		else if (!strcmp(arg, "-vrx"))
		{
			g_rx_vendor_id = &g_tx_vendor_id;
		}
		else if (!strcmp(arg, "-pp"))
		{
			g_print_payloads = AUD_TRUE;
		}
		else
		{
			usage(argv[0]);
			exit(1);
		}
	}

	config = conmon_client_config_new(client_name);
	if (!config)
	{
		printf("Error creating client config: NO_MEMORY\n");
		goto cleanup;
	}
	conmon_client_config_set_server_port(config, server_port);
	if (vendor_broadcast_address)
	{
		conmon_client_config_set_vendor_broadcast_channel_enabled(config, AUD_TRUE);
		conmon_client_config_set_vendor_broadcast_channel_address(config, vendor_broadcast_address);
	}
	result = conmon_client_new_config(env, config, &client);
	conmon_client_config_delete(config);
	config = NULL;

	if (result != AUD_SUCCESS)
	{
		printf("Error creating client: %s\n", aud_error_message(result, g_errbuf));
		goto cleanup;
	}

	conmon_client_set_connection_state_changed_callback(client, handle_connection_state_changed);
	conmon_client_set_sockets_changed_callback(client, handle_sockets_changed);
	conmon_client_set_networks_changed_callback(client, handle_networks_changed);
	conmon_client_set_dante_device_name_changed_callback(client, handle_dante_device_name_changed);
	conmon_client_set_dns_domain_name_changed_callback(client, handle_dns_domain_name_changed);
	conmon_client_set_subscriptions_changed_callback(client,handle_subscriptions_changed);

	if (auto_connect)
	{
		result = conmon_client_auto_connect(client);
		if (result != AUD_SUCCESS)
		{
			printf("Error enabling auto-connect: %s\n", aud_error_message(result, g_errbuf));
			goto cleanup;
		}

		printf("Auto-connecting, request id is %p\n", g_req_id);
		while (g_running && conmon_client_state(client) != CONMON_CLIENT_CONNECTED)
		{
			conmon_example_sleep(&ONE_MS); // just to yield...
			conmon_client_process(client);
		}
	}
	else
	{
		result = conmon_client_connect (client, & handle_connect_response, &g_req_id);
		if (result != AUD_SUCCESS)
		{
			printf("Error connecting client: %s\n", aud_error_message(result, g_errbuf));
			goto cleanup;
		}

		printf("Connecting, request id is %p\n", g_req_id);
		while (g_running && g_req_id != CONMON_CLIENT_NULL_REQ_ID)
		{
			conmon_example_sleep(&ONE_MS); // just to yield...
			conmon_client_process (client);
		}
	}
	if (conmon_client_state(client) == CONMON_CLIENT_CONNECTED)
	{
		const dante_version_t * version = conmon_client_get_server_version(client);
		const dante_version_build_t * version_build = conmon_client_get_server_version_build(client);
		printf("Connected, server version is %u.%u.%u.%u\n",
			version->major, version->minor, version->bugfix, version_build->build_number);
	}
	else
	{
		printf("Error connecting, aborting\n");
		goto cleanup;
	}
	

	if (input_filename)
	{
		char line[1024];
		FILE * fp = fopen(input_filename, "r");
		if (!fp)
		{
			printf("Error opening file '%s'\n", input_filename);
			goto cleanup;
		}

		while (g_running && fgets(line, sizeof(line), fp))
		{
			printf("\nCMD: %s", line);
			g_req_id = process_line(line, client);
			while (g_running && g_req_id != CONMON_CLIENT_NULL_REQ_ID)
			{
				conmon_example_sleep(&ONE_MS); // just to yield...
				conmon_client_process (client);
			}
		}
	}

	main_loop(client);
	
cleanup:
	if (client)
	{
		conmon_client_delete(client);
	}
	if (env)
	{
		aud_env_release (env);
	}
	return result;
}


static void
handle_connect_response
(
	conmon_client_t * client,
	conmon_client_request_id_t request_id,
	aud_error_t result
) {
	aud_errbuf_t errbuf;
	printf ("  Got response for connection request %p: %s\n",
		request_id, aud_error_message(result, errbuf));
	if (result == AUD_SUCCESS)
	{
		char buf[1024];
		printf("  Networks are %s\n", conmon_example_networks_to_string(conmon_client_get_networks(client), buf, sizeof(buf)));
		printf("  Dante device name is \"%s\"\n", conmon_client_get_dante_device_name(client));
		printf("  DNS domain name is \"%s\"\n", conmon_client_get_dns_domain_name(client));
	}
	if (g_req_id == request_id)
	{
		g_req_id = CONMON_CLIENT_NULL_REQ_ID;
	}
}


static void
handle_response
(
	conmon_client_t * client,
	conmon_client_request_id_t request_id,
	aud_error_t result
) {
	aud_errbuf_t errbuf;

	AUD_UNUSED(client);

	printf ("  Got response for request %p: %s\n", request_id, aud_error_message(result, errbuf));
	if (g_req_id == request_id)
	{
		g_req_id = CONMON_CLIENT_NULL_REQ_ID;
	}
}
