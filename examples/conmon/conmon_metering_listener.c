/*
 * File     : $RCSfile$
 * Created  : September 2008
 * Updated  : $Date$
 * Author   : James Westendorp <james.westendorp@audinate.com>
 * Synopsis : An example conmon client capable of controlling a remote device
 *
 * This software is copyright (c) 2004-2009 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */
#include "conmon_examples.h"

//----------------------------------------------------------
// Signal handler to allow exit using CTRL-C
//----------------------------------------------------------

aud_bool_t running = AUD_TRUE;

static void
sig_handler(int sig)
{
	signal(SIGINT, sig_handler);
	running = AUD_FALSE;
}


//----------------------------------------------------------
// Synchronous communications for simplicity
//----------------------------------------------------------

dante_sockets_t client_sockets;

// how long to wait for a response from the server
const aud_utime_t comms_timeout = {2, 0};

// how long the server should try to send control messages before giving up
const aud_utime_t control_timeout = {1, 500000};

aud_bool_t communicating = AUD_FALSE;

aud_error_t last_result = AUD_SUCCESS;

static conmon_client_response_fn handle_response;

static void
handle_response
(
	conmon_client_t * client,
	conmon_client_request_id_t request_id,
	aud_error_t result
) {
	// TODO: should check request id codes to ensure correct matching
	aud_errbuf_t errbuf;
	printf ("Got response for request %p: %s\n",
		request_id, aud_error_message(result, errbuf));
	communicating = AUD_FALSE;
	last_result = result;
}

static aud_error_t
wait_for_response
(
	conmon_client_t * client,
	const aud_utime_t * timeout 
) {
	aud_utime_t now, then;
	aud_utime_get(&now);
	then = now;
	aud_utime_add(&then, timeout);
		
	communicating = AUD_TRUE;
	while(communicating && aud_utime_compare(&now, &then) < 0)
	{
		const aud_utime_t delay = {0, 100000};
		conmon_example_sleep(&delay);
		conmon_client_process_sockets(client, NULL, NULL);
		aud_utime_get(&now);
	}
	if (communicating)
	{
		return AUD_ERR_TIMEDOUT;
	}
	else
	{
		return last_result;
	}
}


//----------------------------------------------------------
// Message handler
//----------------------------------------------------------


static aud_bool_t g_print_raw_bytes = AUD_FALSE;
/**
 * Handle an incoming metering message. This function simply
 * checks that the message is valid (ie. it has the right vendor ID)
 * and then prints out the current levels to stdout
 */
conmon_client_handle_monitoring_message_fn handle_metering_message;

void
handle_metering_message
(
	conmon_client_t * client,
	conmon_channel_type_t type,
	conmon_channel_direction_t channel_direction,
	const conmon_message_head_t * head,
	const conmon_message_body_t * body
) {
	const conmon_metering_message_peak_t * tx_peaks = NULL;
	const conmon_metering_message_peak_t * rx_peaks = NULL;
	conmon_metering_message_version_t version;
	uint16_t num_txchannels;
	uint16_t num_rxchannels;
	conmon_instance_id_t instance_id;
	aud_error_t result;
	aud_errbuf_t errbuf;
	
	char buf[1024*2];	// Buffer size is for maximum 256 x 256. Need to increase for 512 x 512.

	const char * name;

	if (!conmon_vendor_id_equals(conmon_message_head_get_vendor_id(head), CONMON_VENDOR_ID_AUDINATE))
	{
		return;
	}
	
	conmon_message_head_get_instance_id(head, &instance_id);
	name = conmon_client_device_name_for_instance_id(client, &instance_id);


	result = conmon_metering_message_parse(body, &version, &num_txchannels, &num_rxchannels);
	if (result != AUD_SUCCESS)
	{
		printf("Error parsing metering message header: %s\n", aud_error_message(result, errbuf));
		return;
	}
	printf("version=%u ntx=%u nrx=%u\n", version, num_txchannels, num_rxchannels);
	tx_peaks = conmon_metering_message_get_peaks_const(body, CONMON_CHANNEL_DIRECTION_TX);
	rx_peaks = conmon_metering_message_get_peaks_const(body, CONMON_CHANNEL_DIRECTION_RX);
	
	// Print the raw packet
	if (g_print_raw_bytes)
	{
		unsigned int i, meter_head, size = conmon_message_head_get_body_size(head);

		if(version < 3)
			meter_head = sizeof(conmon_metering_message_v1_v2_t);
		else meter_head = sizeof(conmon_metering_message_v3_t);

		printf("PAYLOAD: size=%u ntx=%u nrx=%u\n", size, num_txchannels, num_rxchannels);
		for (i = 0; i < size; i++) 
		{
			if (((i-meter_head) % 16) == 0)
			{
				printf("\n");
			}
			printf(" %02X", body->data[i]);
		}
		printf("\n");
	}
	else
	{
		printf("RECV METERING(%s.%d): TX=%s\n",
			(name ? name : "???"), conmon_message_head_get_seqnum(head),
			conmon_example_metering_peaks_to_string(tx_peaks, num_txchannels, buf, sizeof(buf)));
		printf("RECV METERING(%s,%d): RX=%s\n",
			(name ? name : "???"), conmon_message_head_get_seqnum(head),
			conmon_example_metering_peaks_to_string(rx_peaks, num_rxchannels, buf, sizeof(buf)));
	}
}

//----------------------------------------------------------
// System callbacks
//----------------------------------------------------------

aud_bool_t g_sockets_changed = AUD_FALSE;

conmon_client_sockets_changed_fn handle_sockets_changed;
conmon_client_handle_networks_changed_fn handle_networks_changed;
conmon_client_handle_subscriptions_changed_fn handle_subscriptions_changed;

void
handle_sockets_changed
(
	conmon_client_t * client
) {
	g_sockets_changed = AUD_TRUE;
}

void
handle_networks_changed
(
	conmon_client_t * client
) {
	char buf[1024];
	const conmon_networks_t * networks = conmon_client_get_networks(client);

	conmon_example_networks_to_string(networks, buf, 1024);
	printf("NETWORKS CHANGED: %s\n", buf);
}

static void
client_subscription_to_string
(
	const conmon_client_subscription_t * sub,
	char * buf,
	size_t len
) {
	if (sub)
	{
		char id_buf[64];
		const char * channel = conmon_example_channel_type_to_string(conmon_client_subscription_get_channel_type(sub));
		const char * device = conmon_client_subscription_get_device_name(sub);
		const char * status = conmon_example_rxstatus_to_string(conmon_client_subscription_get_rxstatus(sub));
		conmon_example_instance_id_to_string(conmon_client_subscription_get_instance_id(sub), id_buf, 64);
		SNPRINTF(buf, len, "%s@%s: status=%s instance_id=%s", channel, device, status, id_buf);
	}
	else
	{
		SNPRINTF(buf, len, "[NULL]");
	}
	
}

void
handle_subscriptions_changed
(
	conmon_client_t * client,
	unsigned int num_changes,
	const conmon_client_subscription_t * const * changes
) {
	char buf[1024];
	unsigned int i;
	printf("SUBSCRIPTION CHANGES:\n");
	for (i = 0; i < num_changes; i++)
	{
		client_subscription_to_string(changes[i], buf, 1024);
		printf("%d: %s\n", i, buf);
	}
}

static void
usage(const char * bin)
{
	printf("Usage: %s [-p=PORT] [-d=DEVICE] [-raw]\n", bin);
	printf("  Listen to metering messages from a given device\n");
	printf("  If no transmitter specified then listen for local metering messages\n");
	printf("  -f allow metering channel configuration to fail (useful for debugging)\n");
	printf("  -p=PORT: specify the client's metering port as PORT\n");
	printf("  -d=DEVICE: listen to metering from device DEVICE\n");
	printf("  -raw: print raw packet data\n");
}


/**
 * The main function. The command line must provide the
 * name of the conmon device that will be controlled
 */
int main(int argc, char * argv[])
{
	aud_error_t result;
	aud_errbuf_t errbuf;
	aud_env_t * env = NULL;

	int a;

	aud_bool_t allow_metering_failure = AUD_FALSE;
	uint16_t metering_port = 0;
	const char * device = NULL;

	conmon_client_config_t * config = NULL;
	conmon_client_t * client = NULL;
	conmon_client_request_id_t req_id;

	printf("conmon metering listener, build timestamp %s %s\n", __DATE__, __TIME__);

	for (a = 1; a < argc; a++)
	{
		const char * arg = argv[a];
		if (!strncmp(arg, "-d=", 3) && strlen(arg) > 3)
		{
			device = arg + 3;
		}
		else if (!strncmp(arg, "-p=", 3) && strlen(arg) > 3)
		{
			metering_port = (uint16_t) atoi(arg+3);
		}
		else if (!strcmp(arg, "-f"))
		{
			allow_metering_failure = AUD_TRUE;
		}
		else if (!strcmp(arg, "-raw"))
		{
			g_print_raw_bytes = AUD_TRUE;
		}
		else
		{
			usage(argv[0]);
			exit(1);
		}
	}

	result = aud_env_setup (&env);
	if (result != AUD_SUCCESS)
	{
		printf("Error initialising conmon client library: %s\n",
			aud_error_message(result, errbuf));
		goto cleanup;
	}

	config = conmon_client_config_new("metering_listener");
	if (!config)
	{
		result = AUD_ERR_NOMEMORY;
		printf("Error initialising conmon client config: %s\n",
			aud_error_message(result, errbuf));
		goto cleanup;
	}
	conmon_client_config_set_metering_channel_enabled(config, AUD_TRUE);
	if (metering_port)
	{
		conmon_client_config_set_metering_channel_port(config, metering_port);
	}
	if (allow_metering_failure)
	{
		conmon_client_config_allow_metering_channel_init_failure(config, AUD_TRUE);
	}
	result = conmon_client_new_config(env, config, &client);
	if (client == NULL)
	{
		printf("Error creating client: %s\n", aud_error_message(result, errbuf));
		goto cleanup;
	}

	if (!conmon_client_is_metering_channel_active(client))
	{
		printf("Metering channel configuration failed\n");
	}

	// set before connecting to avoid possible race conditions / missed notifications
	conmon_client_set_networks_changed_callback(client, handle_networks_changed);
	conmon_client_set_subscriptions_changed_callback(client, handle_subscriptions_changed);

	result = conmon_client_connect (client, &handle_response, &req_id);
	if (result == AUD_SUCCESS)
	{
		printf("Connecting, request id is %p\n", req_id);
	}
	else
	{
		printf("Error connecting client(request): %s\n", aud_error_message(result, errbuf));
		goto cleanup;
	}
	result = wait_for_response(client, &comms_timeout);
	if (result != AUD_SUCCESS)
	{
		printf("Error connecting client(response): %s\n", aud_error_message(result, errbuf));
		goto cleanup;
	}

	// set up metering
	if (device)
	{
		result = conmon_client_register_monitoring_messages(client,
			&handle_response, &req_id,
			CONMON_CHANNEL_TYPE_METERING, CONMON_CHANNEL_DIRECTION_RX,
			handle_metering_message);
		if (result == AUD_SUCCESS)
		{
			printf("Registering for rx metering messages, request id is %p\n",
				req_id);
		}
		else
		{
			printf("Error registering for rx metering messages(request): %s\n",
				aud_error_message(result, errbuf));
			goto cleanup;
		}
		result = wait_for_response(client, &comms_timeout);
		if (result != AUD_SUCCESS)
		{
			printf("Error registering for rx metering messages(response): %s\n",
				aud_error_message(result, errbuf));
			goto cleanup;
		}

		result = conmon_client_subscribe(client,
			&handle_response, &req_id,
			CONMON_CHANNEL_TYPE_METERING, device);
		if (result == AUD_SUCCESS)
		{
			printf("Subscribing, request id is %p\n", req_id);
		}
		else
		{
			printf ("Error subscribing to metering channel(request): %s\n",
				aud_error_message(result, errbuf));
			goto cleanup;
		}
		result = wait_for_response(client, &comms_timeout);
		if (result != AUD_SUCCESS)
		{
			printf("Error subscribing to metering channel(response): %s\n",
				aud_error_message(result, errbuf));
			goto cleanup;
		}
	}
	else
	{
		result = conmon_client_register_monitoring_messages(client,
			&handle_response, &req_id,
			CONMON_CHANNEL_TYPE_METERING, CONMON_CHANNEL_DIRECTION_TX,
			handle_metering_message);
		if (result != AUD_SUCCESS)
		{
			printf ("Error registering for tx metering messages(request): %s\n",
				aud_error_message(result, errbuf));
			goto cleanup;
		}
		result = wait_for_response(client, &comms_timeout);
		if (result != AUD_SUCCESS)
		{
			printf("Error registering for tx metering messages(response): %s\n",
				aud_error_message(result, errbuf));
			goto cleanup;
		}
	}


	// We're all setup so run the main loop
	{
		const aud_utime_t ONE_SECOND = {1,0};
		signal(SIGINT, sig_handler);


		dante_sockets_clear(&client_sockets);
		result = conmon_client_get_sockets(client, &client_sockets);
		if (result != AUD_SUCCESS)
		{
			printf("Error getting sockets: %s\n", aud_error_message(result, errbuf));
		}

		// if we got here then we're all set up and can start the main processing loop
		// The loop runs until the user hits CTRL-C
		while(running)
		{
			conmon_example_client_process(client, &client_sockets, &ONE_SECOND, NULL);

			// update sockets if needed
			if (g_sockets_changed)
			{
				g_sockets_changed = AUD_FALSE;
				dante_sockets_clear(&client_sockets);
				conmon_client_get_sockets(client, &client_sockets);
			}
		}
	}

cleanup:
	// Now cleanup the metering channel and the client and shutdown
	if (client)
	{
		conmon_client_delete(client);
	}
	if (config)
	{
		conmon_client_config_delete(config);
	}
	if (env)
	{
		aud_env_release (env);
	}
	return result;
}
