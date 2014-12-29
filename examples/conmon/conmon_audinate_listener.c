/*
 * File     : $RCSfile$
 * Created  : September 2008
 * Updated  : $Date$
 * Author   : Andrew White <andrew.white@audinate.com>
 * Synopsis : Passive conmon client that listens to Audinate events
 *
 * This software is copyright (c) 2004-2009 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */


//----------
// Include

#include "conmon_examples.h"

#ifdef WIN32
#else
	#include <libgen.h>
	#include <errno.h>
#endif

//----------
// Types and Constants

static char * g_progname;

enum
{
	LISTEN_MAX_TARGETS = 16,
	FILTER_MAX_FILTERS = 16
};

typedef enum message_filter_mode
{
	MESSAGE_FILTER_MODE_PASS = 0,
	MESSAGE_FILTER_MODE_FAIL
} message_filter_mode_t;

typedef enum print_mode_raw
{
	PRINT_MODE_RAW_NONE = 0,
		// Don't print raw messages
	PRINT_MODE_RAW_UNKNOWN,
		// Print raw messages if printing fails
	PRINT_MODE_RAW_ALWAYS
		// Always print raw messages
} print_mode_raw_t;

typedef struct conmon_info conmon_info_t;

struct conmon_info
{
	// conmon connectivity
	aud_env_t * env;
	uint16_t server_port;
	conmon_client_t * client;
	
	// runtime
	aud_bool_t running;
	int result;
	
	// Target devices
	aud_bool_t all;
	conmon_client_request_id_t all_req_id;

	// options
	aud_bool_t quiet;
	
	unsigned int n_targets;
	struct conmon_target
	{
		const char * name;
		const conmon_client_subscription_t * sub;
		conmon_rxstatus_t old_status;
		aud_bool_t found;

		conmon_client_request_id_t req_id;

		const conmon_instance_id_t * conmon_id;
		char id_buf [64];
	} targets [LISTEN_MAX_TARGETS];

	unsigned n_filters;
	message_filter_mode_t filter_mode;
	struct message_filter
	{
		conmon_audinate_message_type_t mtype;
	} filter [FILTER_MAX_FILTERS];

	struct conmon_info_raw
	{
		print_mode_raw_t mode;
		aud_bool_t offsets;
		unsigned n_filters;
		struct message_filter filter[FILTER_MAX_FILTERS];
	} raw;
};

typedef struct conmon_target cm_target_t;


//----------
// Local function prototypes

AUD_INLINE const char *
pname (void)
{
#ifdef WIN32
	return g_progname;
#else
	return basename (g_progname);
#endif
}


static void
timestamp_event (void);

static void
timestamp_error (void);


static int
handle_args (conmon_info_t * cm, unsigned int argc, char ** argv);

static aud_error_t
setup_conmon (conmon_info_t * cm);

static aud_error_t
shutdown_conmon (conmon_info_t * cm);

static aud_error_t
register_for_events (conmon_info_t * cm);

static aud_error_t
subscribe_to_remote_clients (conmon_info_t * cm);

static cm_target_t *
target_for_sub (conmon_info_t * cm, const conmon_client_subscription_t * sub);


//----------
// Callback prototypes

static conmon_client_response_fn
	conmon_cb_reg_connect, conmon_cb_reg_monitoring,
	conmon_cb_reg_sub, conmon_cb_reg_sub_all;

static conmon_client_handle_networks_changed_fn conmon_cb_network;
static conmon_client_handle_dante_device_name_changed_fn conmon_cb_dante_device_name;
static conmon_client_handle_dns_domain_name_changed_fn conmon_cb_dns_domain_name;
//static conmon_client_handle_subscriptions_changed_fn conmon_cb_subscriptions;

static conmon_client_handle_monitoring_message_fn conmon_cb_monitoring;

static conmon_client_handle_subscriptions_changed_fn conmon_cb_cm_sub_changed;


//----------
// Main

int
main (int argc, char ** argv)
{
	aud_error_t result;
	aud_errbuf_t ebuf;
	conmon_info_t info = { 0 };
	
	result = handle_args (& info, argc, argv);
	if (result != 0)
	{
		return result;
	}
	
	result = setup_conmon (& info);
	if (result != AUD_SUCCESS)
	{
		return 1;
	}
	
	info.running = AUD_TRUE;
	while (info.running)
	{
		int nfds;
		fd_set fdr;
		int count;

		aud_socket_t fd = conmon_client_get_socket (info.client);
		
		FD_ZERO (& fdr);
		FD_SET (fd, & fdr);

#ifdef WIN32
		nfds = 1;
#else
		nfds = fd + 1;
#endif
		count = select (nfds, & fdr, NULL, NULL, NULL);
		if (count > 0)
		{
			result = conmon_client_process (info.client);
			if (result != AUD_SUCCESS)
			{
				timestamp_error ();
				fprintf (stderr,
					"%s: failed processing fd %d: %s (%d)\n"
					, pname (), fd
					, aud_error_message (result, ebuf), result
				);
			}
		}
		else if (count == 0)
		{
			timestamp_error ();
			fprintf (stderr,
				"%s: unexpected timeout from select\n"
				, pname ()
			);
		}
		else
		{
			timestamp_error ();
			fprintf (stderr,
				"%s: select failed: %s (%d)\n"
				, pname ()
				, strerror (errno), errno
			);
		}
	}
	
	shutdown_conmon (& info);
	
	return info.result;
}


//----------
// Local functions

static void
timestamp_event ()
{
	aud_ctime_buf_t buf;
	printf ("#EVENT %s: ", aud_utime_ctime_no_newline (NULL, buf));
}


static void
timestamp_error ()
{
	aud_ctime_buf_t buf;
	fprintf (stderr, "#ERROR %s: ", aud_utime_ctime_no_newline (NULL, buf));
}


static aud_error_t
setup_conmon (conmon_info_t * cm)
{
	conmon_client_config_t * config = NULL;
	aud_error_t result;
	aud_errbuf_t ebuf;

	cm->env = NULL;
	cm->client = NULL;
	cm->running = NO;
	cm->result = 0;

	result = aud_env_setup (& cm->env);
	if (result != AUD_SUCCESS)
	{
		timestamp_error ();
		fprintf (stderr,
			"%s: conmon failed to initialise environment: %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
		return result;
	}

	//conmon_client_env_set_network_logging (
	//	cm->env, 9889, NULL
	//);

	config = conmon_client_config_new("demo_listener");
	if (!config)
	{
		timestamp_error ();
		fprintf (stderr,
			"%s: conmon failed to initialise client: NO_MEMORY\n"
			, pname ()
		);
		goto l__error;
	}
	if (cm->server_port)
	{
		conmon_client_config_set_server_port(config, cm->server_port);
	}
	result = conmon_client_new_config (cm->env, config, &cm->client);
	conmon_client_config_delete(config);
	config = NULL;

	if (result != AUD_SUCCESS)
	{
		timestamp_error ();
		fprintf (stderr,
			"%s: conmon failed to initialise client: %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
		goto l__error;
	}

	conmon_client_set_context (cm->client, cm);

	result =
		conmon_client_connect (
			cm->client,
			& conmon_cb_reg_connect,
			NULL
		);
	if (result != AUD_SUCCESS)
	{
		timestamp_error ();
		fprintf (stderr,
			"%s: conmon failed to initiate connection: %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
		goto l__error;
	}
	
	return AUD_SUCCESS;

l__error:
	shutdown_conmon (cm);
	
	return result;
}

static aud_error_t
retry_conmon (conmon_info_t * cm)
{
	aud_error_t result;
	aud_errbuf_t ebuf;

	result =
		conmon_client_connect (
			cm->client,
			& conmon_cb_reg_connect,
			NULL
		);
	if (result != AUD_SUCCESS)
	{
		timestamp_error ();
		fprintf (stderr,
			"%s: conmon failed to initiate connection: %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
		goto l__error;
	}
	
	return AUD_SUCCESS;

l__error:
	shutdown_conmon (cm);
	
	return result;
}

static aud_error_t
shutdown_conmon (conmon_info_t * cm)
{
	if (cm->client)
	{
		conmon_client_delete (cm->client);
		cm->client = NULL;
	}
	
	if (cm->env)
	{
		aud_env_release (cm->env);
		cm->env = NULL;
	}
	
	return AUD_SUCCESS;
}


static aud_error_t
register_for_events (conmon_info_t * cm)
{
	aud_error_t result;
	aud_bool_t has_success = AUD_FALSE;

	conmon_client_set_networks_changed_callback (
		cm->client, & conmon_cb_network
	);
	
	conmon_client_set_dante_device_name_changed_callback (
		cm->client, & conmon_cb_dante_device_name
	);
	conmon_client_set_dns_domain_name_changed_callback (
		cm->client, & conmon_cb_dns_domain_name
	);

	// Register for outgoing (TX) status messages
	result =
		conmon_client_register_monitoring_messages (
			cm->client, & conmon_cb_reg_monitoring, NULL,
			CONMON_CHANNEL_TYPE_STATUS, CONMON_CHANNEL_DIRECTION_TX,
			& conmon_cb_monitoring
		);
	if (result == AUD_SUCCESS)
	{
		has_success = AUD_TRUE;
	}
	else
	{
		aud_errbuf_t ebuf;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to register monitoring channel (status): %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
	}

	// Register for outgoing (TX) broadcast messages
	result =
		conmon_client_register_monitoring_messages (
			cm->client, & conmon_cb_reg_monitoring, NULL,
			CONMON_CHANNEL_TYPE_BROADCAST, CONMON_CHANNEL_DIRECTION_TX,
			& conmon_cb_monitoring
		);
	if (result == AUD_SUCCESS)
	{
		has_success = AUD_TRUE;
	}
	else
	{
		aud_errbuf_t ebuf;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to register monitoring channel (broadcast): %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
	}

	// Register for "outgoing" (TX) local messages
	result =
		conmon_client_register_monitoring_messages (
			cm->client, & conmon_cb_reg_monitoring, NULL,
			CONMON_CHANNEL_TYPE_LOCAL, CONMON_CHANNEL_DIRECTION_TX,
			& conmon_cb_monitoring
		);
	if (result == AUD_SUCCESS)
	{
		has_success = AUD_TRUE;
	}
	else
	{
		aud_errbuf_t ebuf;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to register monitoring channel (local): %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
	}

	// register for incoming (RX) status messages
	result =
		conmon_client_register_monitoring_messages (
			cm->client, & conmon_cb_reg_monitoring, NULL,
			CONMON_CHANNEL_TYPE_STATUS, CONMON_CHANNEL_DIRECTION_RX,
			& conmon_cb_monitoring
		);
	if (result == AUD_SUCCESS)
	{
		has_success = AUD_TRUE;
	}
	else
	{
		aud_errbuf_t ebuf;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to register external monitoring channel (status): %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
	}

	// register for incoming (RX) broadcast messages
	result =
		conmon_client_register_monitoring_messages (
			cm->client, & conmon_cb_reg_monitoring, NULL,
			CONMON_CHANNEL_TYPE_BROADCAST, CONMON_CHANNEL_DIRECTION_RX,
			& conmon_cb_monitoring
		);
	if (result == AUD_SUCCESS)
	{
		has_success = AUD_TRUE;
	}
	else
	{
		aud_errbuf_t ebuf;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to register external monitoring channel (rx broadcast): %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
	}

	// catch outbound messages as well (useful for
	// when we are a PTP master)
	/*
	result =
		conmon_client_register_monitoring_messages (
			cm->client, & conmon_cb_reg_status, NULL,
			CONMON_CHANNEL_TYPE_BROADCAST, CONMON_CHANNEL_DIRECTION_TX,
			& conmon_cb_status
		);
	if (result == AUD_SUCCESS)
	{
		has_success = AUD_TRUE;
	}
	else
	{
		aud_errbuf_t ebuf;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to register external monitoring channel (tx broadcast): %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
	}
	*/

	if (has_success)
	{
		return AUD_SUCCESS;
	}
	else
	{
		return result;
	}
}


static aud_error_t
subscribe_to_remote_clients (conmon_info_t * cm)
{
	aud_error_t result;
	unsigned int i;
	
	if (cm->all)
	{
		result =
			conmon_client_subscribe_global (
				cm->client,
				conmon_cb_reg_sub_all,
				& (cm->all_req_id),
				CONMON_CHANNEL_TYPE_STATUS
			);
		if (result != AUD_SUCCESS)
		{
			return result;
		}
	}
	
	for (i = 0; i < cm->n_targets; i++)
	{
		if (cm->targets[i].name == NULL)
		{
			continue;
		}
		// don't try and subscribe to ourself
		if (!strcmp(conmon_client_get_dante_device_name(cm->client), cm->targets[i].name))
		{
			continue;
		}
		result =
			conmon_client_subscribe (
				cm->client,
				conmon_cb_reg_sub,
				& (cm->targets [i].req_id),
				CONMON_CHANNEL_TYPE_STATUS,
				cm->targets [i].name
			);
		if (result != AUD_SUCCESS)
		{
			return result;
		}
	}
	
	return AUD_SUCCESS;
		// XXX: should check eventually
}


static cm_target_t *
target_for_sub (conmon_info_t * cm, const conmon_client_subscription_t * sub)
{
	unsigned int i;
	const unsigned int n = cm->n_targets;
	
	for (i = 0; i < n; i++)
	{
		cm_target_t * target = cm->targets + i;
		if (target->name && (target->sub == sub))
		{
			return target;
		}
	}
	
	return NULL;
}


static void
print_raw_body
(
	const conmon_message_body_t * aud_msg,
	size_t body_size,
	const conmon_info_t * info
)
{
	unsigned i;

	if (info->raw.n_filters)
	{
		uint16_t aud_type = conmon_audinate_message_get_type(aud_msg);

		for (i = 0; i < info->raw.n_filters; i++)
		{
			if (aud_type == info->raw.filter[i].mtype)
			{
				goto l__pass_filter;
			}
		}
		return;
	}
	l__pass_filter:

	printf("> Body length: %u bytes:", (unsigned) body_size);
	for (i = 0; i < body_size; i++)
	{
		if ((i & 0xf) == 0)
		{
			if (info->raw.offsets)
			{
				printf("\n %04x: ", i);
			}
			else
			{
				fputs("\n\t", stdout);
			}
		}
		else if ((i & 0x3) == 0)
		{
			putchar(' ');
		}
		printf("%02x", aud_msg->data[i]);
	}
	putchar('\n');
}


//----------
// Callbacks

static void
conmon_cb_reg_connect (
	conmon_client_t * client,
	conmon_client_request_id_t request_id,
	aud_error_t result
)
{
	conmon_info_t * info = conmon_client_context (client);

	(void) request_id;

	if (result == AUD_SUCCESS)
	{
		const conmon_networks_t * networks;

		timestamp_event ();
		puts ("Conmon connection successful");
		
		if (info)
		{
			register_for_events (info);
			subscribe_to_remote_clients (info);
			conmon_client_set_subscriptions_changed_callback (
				client, conmon_cb_cm_sub_changed
			);
		}
		else
		{
			timestamp_error ();
			fprintf (stderr,
				"%s: no context!\n"
				, pname ()
			);
		}

		puts ("Networks:");
		networks = conmon_client_get_networks (client);
		conmon_aud_print_networks (networks, " ");

		fprintf (stdout,
		"Dante device name '%s'\n"
		, conmon_client_get_dante_device_name (client));

		fprintf (stdout,
		"DNS domain name '%s'\n"
		, conmon_client_get_dns_domain_name (client));
	}
	else
	{
		aud_errbuf_t ebuf;
		aud_utime_t interval;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to connect to conmon: %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
		// Retry connect
		interval.tv_sec = 5;
		interval.tv_usec = 0;
		conmon_example_sleep(&interval);
		conmon_client_disconnect(client);
		retry_conmon(info);
	}
}


static void
conmon_cb_reg_monitoring (
	conmon_client_t * client,
	conmon_client_request_id_t request_id,
	aud_error_t result
)
{
	(void) client;
	(void) request_id;
	
	if (result == AUD_SUCCESS)
	{
		//conmon_info_t * info = conmon_client_context (client);

		timestamp_event ();
		puts ("Conmon status registration successful");
	}
	else
	{
		aud_errbuf_t ebuf;
		
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to subscribe to conmon status channel: %s (%d)\n"
			, pname ()
			, aud_error_message (result, ebuf), result
		);
	}
}


static void
conmon_cb_reg_sub (
	conmon_client_t * client,
	conmon_client_request_id_t request_id,
	aud_error_t result
)
{
	struct conmon_target * target;
	
	conmon_info_t * info = conmon_client_context (client);

	aud_errbuf_t ebuf;

	if (request_id != CONMON_CLIENT_NULL_REQ_ID)
	{
		const unsigned int n = info->n_targets;
		const char * p_name;
		unsigned int i;
		
		// find the remote target we were trying to subscribe
		for (i = 0; i < n; i++)
		{
			if (request_id == info->targets [i].req_id)
			{
				target = info->targets + i;
				goto l__found_target;
			}
		}
		
		timestamp_error ();
		fprintf (stderr,
			"%s: invalid subscription request ID: 0x%p\n"
			, (p_name = pname ()), request_id
		);
		if (result != AUD_SUCCESS)
		{
			fprintf (stderr,
				"%s: error %s (%d)\n"
				, p_name
				, aud_error_message (result, ebuf), result
			);
		}
	}
	else
	{
		fprintf (stderr,
			"%s: unexpected subscription response\n"
			, pname ()
		);
	}
	
	return;
	
l__found_target:
	if (result == AUD_SUCCESS)
	{
		target->sub =
			conmon_client_get_subscription (
				info->client,
				CONMON_CHANNEL_TYPE_STATUS,
				target->name
			);
		if (! target->sub)
		{
			timestamp_error ();
			fprintf (stderr,
				"Conmon subscription registration to %s - server error\n"
				, target->name
			);
		}
	}
	else
	{
		timestamp_error ();
		fprintf (stderr,
			"%s: failed to subscribe to remote conmon device %s: %s (%d)\n"
			, pname ()
			, target->name
			, aud_error_message (result, ebuf), result
		);
	}
	
	target->req_id = CONMON_CLIENT_NULL_REQ_ID;
}


static void
conmon_cb_reg_sub_all (
	conmon_client_t * client,
	conmon_client_request_id_t request_id,
	aud_error_t result
)
{
	conmon_info_t * info = conmon_client_context (client);

	aud_errbuf_t ebuf;
		
	if (request_id != CONMON_CLIENT_NULL_REQ_ID)
	{
		// find the remote target we were trying to subscribe
		if (request_id == info->all_req_id)
		{
			if (result != AUD_SUCCESS)
			{
				timestamp_error ();
				fprintf (stderr,
					"%s: failed to subscribe to all devices: %s (%d)\n"
					, pname ()
					, aud_error_message (result, ebuf), result
				);
			}

			info->all_req_id = CONMON_CLIENT_NULL_REQ_ID;
			return;
		}
	}

	fprintf (stderr,
		"%s: unexpected subscription response\n"
		, pname ()
	);

	return;
}


void
conmon_cb_cm_sub_changed (
	conmon_client_t * client,
	unsigned int num_changes,
	const conmon_client_subscription_t * const * changes
)
{
	unsigned int i;
	conmon_info_t * cm = conmon_client_context (client);
	
	for (i = 0; i < num_changes; i++)
	{
		const conmon_client_subscription_t * sub = changes [i];
		
		cm_target_t * target = target_for_sub (cm, sub);
		if (target)
		{
			conmon_rxstatus_t rxstatus =
				conmon_client_subscription_get_rxstatus (sub);

			if (target->old_status == rxstatus)
			{
				continue;
			}
			target->old_status = rxstatus;

			switch (rxstatus)
			{
			case CONMON_RXSTATUS_UNICAST:
			case CONMON_RXSTATUS_MULTICAST:
				if (!target->found)
				{
					target->found = YES;
					target->conmon_id = conmon_client_subscription_get_instance_id (sub);
					conmon_example_instance_id_to_string (
						target->conmon_id,
						target->id_buf,
						sizeof(target->id_buf)
					);
					timestamp_event ();
					printf ("Subscription to '%s' active, id=%s\n"
						, target->name
						, target->id_buf
					);
				}
				break;

			case CONMON_RXSTATUS_UNRESOLVED:
				target->found = NO;
				timestamp_event ();
				printf ("Subscription to '%s' is now UNRESOLVED\n", target->name);
				break;

			// transient states, don't print anything
			case CONMON_RXSTATUS_PREPARING:
			case CONMON_RXSTATUS_RESOLVED:
				target->found = NO;
				target->id_buf [0] = 0;
				target->conmon_id = NULL;

				timestamp_event ();
				printf ("Subscription to '%s' has entered transient state 0x%04x (%s)\n", 
					target->name, rxstatus, conmon_example_rxstatus_to_string(rxstatus)
				);
				break;

			default:
				target->found = NO;
				target->id_buf [0] = 0;
				target->conmon_id = NULL;

				timestamp_event ();
				printf ("Subscription to '%s' has entered error state 0x%04x (%s)\n", 
					target->name, rxstatus, conmon_example_rxstatus_to_string(rxstatus)
				);
			}
		}
		// else we ignore because we're not interested in this subscription
	}
}


static void
conmon_cb_network (conmon_client_t * client)
{
	//conmon_info_t * info = conmon_client_context (client);
	const conmon_networks_t * networks;

	timestamp_event ();
	puts ("Addresses changed");

	puts ("Networks:");
	networks = conmon_client_get_networks (client);
	conmon_aud_print_networks (networks, " ");
}

// unused
/*
static void
conmon_cb_subscriptions
(
	conmon_client_t * client,
	unsigned int num_changes,
	const conmon_client_subscription_t * const * changes
) {
	char id_buf[64];
	unsigned int i;

	(void) client;

	puts("Subscriptions changed:");
	for (i = 0; i < num_changes; i++)
	{
		printf("%s@%s: status=%s instance_id=%s\n",
			conmon_example_channel_type_to_string (
				conmon_client_subscription_get_channel_type(changes[i])
			),
			conmon_client_subscription_get_device_name (changes[i]),
			conmon_example_rxstatus_to_string (
				conmon_client_subscription_get_rxstatus(changes[i])
			),
			conmon_example_instance_id_to_string (
				conmon_client_subscription_get_instance_id(changes[i]),
				id_buf, sizeof(id_buf)
			)
		);
	}
}
*/

static void
conmon_cb_dante_device_name (conmon_client_t * client)
{
	timestamp_event ();
	fprintf (stdout,
		"Dante device name changed to '%s'\n"
		, conmon_client_get_dante_device_name (client)
	);
}

static void
conmon_cb_dns_domain_name (conmon_client_t * client)
{
	timestamp_event ();
	fprintf (stdout,
		"DNS domain name changed to '%s'\n"
		, conmon_client_get_dns_domain_name (client)
	);
}


void 
conmon_cb_monitoring
(
	conmon_client_t * client,
	conmon_channel_type_t channel_type,
	conmon_channel_direction_t channel_direction,
	const conmon_message_head_t * head,
	const conmon_message_body_t * body
)
{
	aud_bool_t want_to_print = AUD_FALSE;
	//conmon_info_t * info = conmon_client_context (client);
	conmon_instance_id_t id;
	char id_buf[64];
	//const conmon_audinate_message_head_t * aud_msg = (void *) body;
	uint16_t aud_type = conmon_audinate_message_get_type(body);

	conmon_info_t * info = conmon_client_context(client);

	conmon_message_head_get_instance_id(head, &id);

	if (info->n_filters)
	{
		unsigned i;
		for (i = 0; i < info->n_filters; i++)
		{
			if (aud_type == info->filter[i].mtype)
			{
				if (info->filter_mode == MESSAGE_FILTER_MODE_PASS)
					goto l__pass_filter;
				else
					return;
			}
		}
		if (info->filter_mode == MESSAGE_FILTER_MODE_PASS)
			return;
	}
	l__pass_filter:

	if (info->all)
	{
		want_to_print = AUD_TRUE;
	}
	else if (!info->quiet)
	{
		want_to_print = AUD_TRUE;
	}
	else
	{
		// in 'quiet' mode we only print things we know we have specifically requested
		const char * name = conmon_client_device_name_for_instance_id(client, &id);
		if (name)
		{
			if (info->n_targets)
			{
				unsigned int i;
				for (i = 0; i < info->n_targets; i++)
				{
					if (!STRCASECMP(name, info->targets[i].name))
					{
						want_to_print = AUD_TRUE;
						break;
					}
				}
			}
			else if (!STRCASECMP(name, conmon_client_get_dante_device_name(info->client)))
			{
				want_to_print = AUD_TRUE;
			}
		}
	}
	if (!want_to_print)
	{
		return;
	}

	timestamp_event ();
	
	
	{
		aud_bool_t printed;
		uint16_t body_size = conmon_message_head_get_body_size(head);
		const char * device_name = conmon_client_device_name_for_instance_id(client, &id);
		uint16_t aud_version = conmon_audinate_message_get_version(body);
		printf (
			"Received status message from %s (%s)\n:"
			"  chan=%s (%s) size=%d aud-version=0x%04x aud-type=0x%04x\n"
			, conmon_example_instance_id_to_string(&id, id_buf, sizeof(id_buf))
			, (device_name ? device_name : "[unknown device]")
			, conmon_example_channel_type_to_string(channel_type)
			, (channel_direction == CONMON_CHANNEL_DIRECTION_TX ? "tx" : "rx")
			, conmon_message_head_get_body_size(head)
			, (unsigned int) aud_version
			, (unsigned int) aud_type
		);
		
		printed = conmon_aud_print_msg (body, body_size);
		if (info->raw.mode == PRINT_MODE_RAW_ALWAYS ||
			(info->raw.mode == PRINT_MODE_RAW_UNKNOWN && !printed)
		)
		{
			print_raw_body(body, body_size, info);
		}
	}
}


//----------
// Args handling

static int
usage (const char *);

static int
handle_args (conmon_info_t * cm, unsigned int argc, char ** argv)
{
	unsigned int i;
	unsigned int curr_arg_index = 1;
	unsigned int more_opts = 1;

	g_progname = argv [0];
	
	while (more_opts && curr_arg_index < argc)
	{
		char * arg = argv [curr_arg_index];

		if (arg [0] == '-')
		{
			message_filter_mode_t fmode = MESSAGE_FILTER_MODE_PASS;
				// for f/x case below

			curr_arg_index ++;

			switch (arg [1])
			{
			case '-':
				more_opts = 0;
				break;

			case 'p':
				if (curr_arg_index >= argc)
				{
					return usage("Missing argument to -p");
				}
				else
				{
					cm->server_port = (uint16_t) atoi(argv[curr_arg_index++]);
				}
				break;

			case 'q':
				cm->quiet = AUD_TRUE;
				break;
			
			case 'a':
				cm->all = AUD_TRUE;
				break;
			
			case 'x':
				fmode = MESSAGE_FILTER_MODE_FAIL;
			case 'f':
				if (cm->n_filters)
				{
					if (fmode != cm->filter_mode)
						return usage("Cannot use both -f and -x");
				}
				else
				{
					cm->filter_mode = fmode;
				}
				if (curr_arg_index >= argc)
				{
					return usage("Missing argument to -f");
				}
				else if (cm->n_filters >= FILTER_MAX_FILTERS)
				{
					return usage("Too many filters");
				}
				else
				{
					char * optarg = argv[curr_arg_index++];
					unsigned long argval = strtoul(optarg, NULL, 0);
					if ((! argval) || argval > 0xFFFF)
					{
						return usage("Invalid filter argument");
					}

					cm->filter[cm->n_filters++].mtype = (conmon_audinate_message_type_t)argval;
				}
				break;

			case 'R':
				if (cm->raw.mode < PRINT_MODE_RAW_ALWAYS)
					cm->raw.mode++;
				else
					return usage("Unknown raw mode (use -R at most twice)");
				break;

			case 'F':
				cm->raw.mode = PRINT_MODE_RAW_ALWAYS;
				if (cm->raw.n_filters >= FILTER_MAX_FILTERS)
				{
					return usage("Too many raw print filters");
				}
				else
				{
					char * optarg = argv[curr_arg_index++];
					unsigned long argval = strtoul(optarg, NULL, 0);
					if ((! argval) || argval > 0xFFFF)
					{
						return usage("Invalid raw print filter argument");
					}

					cm->raw.filter[cm->raw.n_filters++].mtype = (conmon_audinate_message_type_t)argval;
				}
				break;

			case 'O':
				cm->raw.offsets = AUD_TRUE;
				break;

			default:
				fprintf (stderr, "%s: Unknown option '%s'\n"
					, pname (), arg
				);
				return usage (NULL);
			}
		}
		else
		{
			break;
		}
	}
	
	cm->n_targets = argc - curr_arg_index;
	for (i = 0; i < cm->n_targets; i++)
	{
		cm->targets [i].name = argv [curr_arg_index ++];
		cm->targets [i].req_id = CONMON_CLIENT_NULL_REQ_ID;
		cm->targets [i].found = NO;
	}
	
	return 0;
}


static int
usage (const char * msg)
{
	const char * name = pname ();

	if (msg)
	{
		fprintf (stderr, "%s: %s\n", name, msg);
	}

	fprintf (stderr,
		"Usage: %s [-p port] [-q] -a [-x|f msg_type ...] [device ...]\n"
		, name
	);
	
	return 2;
}


//----------
