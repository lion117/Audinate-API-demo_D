/*
 * File     : $RCSfile$
 * Created  : 25 Nov 2010 15:51:56
 * Updated  : $Date$
 * Author   : Varuni Witana <varuni.witana@audinate.com>
 * Synopsis : Test program to send conmon audinate messages
 *
 * This software is copyright (c) 2004-2008 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */

#include "conmon_audinate_controller.h"
#include "dapi_io.h"


#define CONGESTION_DELAY ONE_SECOND_US

static conmon_audinate_control_handler_fn 
	conmon_audinate_interface_control_handler,
	conmon_audinate_switch_vlan_control_handler,
	conmon_audinate_clocking_control_handler,
	conmon_audinate_unicast_clocking_control_handler,
	conmon_audinate_name_id_control_handler,
	conmon_audinate_igmp_vers_control_handler,
	conmon_audinate_srate_control_handler,
	conmon_audinate_srate_pullup_control_handler,
	conmon_audinate_enc_control_handler,
	conmon_audinate_access_control_handler,
	conmon_audinate_edk_board_control_handler,
	conmon_audinate_ifstats_control_handler,
	conmon_audinate_rx_error_threshold_control_handler,
	conmon_audinate_sys_reset_handler,
	conmon_audinate_metering_control_handler,
	conmon_audinate_serial_port_control_handler,
	conmon_audinate_haremote_control_handler,
	conmon_audinate_upgrade_control_handler,
	conmon_audinate_upgrade_v3_control_handler,
	conmon_audinate_clear_config_control_handler,
	conmon_audinate_gpio_control_handler,
	conmon_audinate_ptp_logging_control_handler;


typedef struct conmon_audinate_control_handler_map
{
	conmon_audinate_message_type_t type;
	char cmd[16];
	conmon_audinate_control_handler_fn * handler;
} conmon_audinate_control_handler_map;

static const conmon_audinate_control_handler_map
audinate_control_map[] =
{
	{CONMON_AUDINATE_MESSAGE_TYPE_INTERFACE_CONTROL, "interface", conmon_audinate_interface_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_SWITCH_VLAN_CONTROL, "switch_vlan", conmon_audinate_switch_vlan_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_CLOCKING_CONTROL, "clocking",  conmon_audinate_clocking_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_UNICAST_CLOCKING_CONTROL, "uclocking",  conmon_audinate_unicast_clocking_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_MASTER_QUERY, "master", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_NAME_ID_CONTROL, "name_id", conmon_audinate_name_id_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_IFSTATS_QUERY, "ifstats", conmon_audinate_ifstats_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_IGMP_VERS_CONTROL, "igmp", conmon_audinate_igmp_vers_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_VERSIONS_QUERY, "versions", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_SRATE_CONTROL, "srate", conmon_audinate_srate_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_SRATE_PULLUP_CONTROL, "pullup", conmon_audinate_srate_pullup_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_ENC_CONTROL, "enc", conmon_audinate_enc_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_AUDIO_INTERFACE_QUERY, "audio_interface", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_SYS_RESET, "sysreset", conmon_audinate_sys_reset_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_EDK_BOARD_CONTROL, "edk", conmon_audinate_edk_board_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_ACCESS_CONTROL, "access", conmon_audinate_access_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_MANF_VERSIONS_QUERY, "manf_versions", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_IDENTIFY_QUERY, "identify",  NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_RX_ERROR_THRES_CONTROL, "errthres", conmon_audinate_rx_error_threshold_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_RX_CHANNEL_RX_ERROR_QUERY, "rxerrq", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_METERING_CONTROL, "metering", conmon_audinate_metering_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_SERIAL_PORT_CONTROL, "serial", conmon_audinate_serial_port_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_UPGRADE_CONTROL, "upgrade", conmon_audinate_upgrade_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_UPGRADE_V3_CONTROL, "upgrade3", conmon_audinate_upgrade_v3_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_CLEAR_CONFIG_CONTROL, "clear_config", conmon_audinate_clear_config_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_ROUTING_READY_QUERY, "routing_ready", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_HAREMOTE_CONTROL, "haremote", conmon_audinate_haremote_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_HAREMOTE_STATS_QUERY, "haremote_stats", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_DANTE_READY_QUERY, "dante_ready", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_PTP_LOGGING_CONTROL, "ptp_logging", conmon_audinate_ptp_logging_control_handler},
	{CONMON_AUDINATE_MESSAGE_TYPE_CLOCKING_UNMUTE_CONTROL, "clock_unmute", NULL},
	{CONMON_AUDINATE_MESSAGE_TYPE_GPIO_QUERY, "gpio", conmon_audinate_gpio_control_handler},

	{0, "", 0}
		// Terminator
};



// how long to wait for a response from the server
const aud_utime_t comms_timeout = {2, 0};

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

	(void) client;

	printf ("Got response for request 0x%p: %s\n",
		request_id, aud_error_message(result, errbuf));
	communicating = AUD_FALSE;
	last_result = result;
}

aud_error_t
wait_for_response
(
	conmon_client_t * client,
	const aud_utime_t * timeout
);

aud_error_t
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
		conmon_client_process(client);
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

static conmon_client_handle_networks_changed_fn handle_networks_changed;

static void
handle_networks_changed
(
	conmon_client_t * client
) {
	char buf[1024];
	const conmon_networks_t * networks = conmon_client_get_networks(client);

	conmon_example_networks_to_string(networks, buf, 1024);
	printf("NETWORKS CHANGED: %s\n", buf);
}

static void usage(char * cmd)
{
	int i;
	fprintf(stderr,"%s controlled_device ", cmd);

	fprintf(stderr,"%s",audinate_control_map[0].cmd);
	for (i = 1; audinate_control_map [i].cmd [0]; i++)
	{
		fprintf(stderr,"|%s",audinate_control_map[i].cmd);
	}
	fputc ('\n', stderr);
	exit(1);
}

static aud_error_t parse_args
(
	int argc, 
	char **argv, 
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	
	int i;
	for (i = 0; audinate_control_map [i].cmd [0]; i++)
	{
		if(strcmp(argv[2], audinate_control_map[i].cmd) == 0)
		{
			conmon_audinate_control_handler_fn * handler;
			handler = audinate_control_map[i].handler;
			//conmon_audinate_message_head_initialise((conmon_audinate_message_head_t*)body,
			//	audinate_control_map[i].type, ONE_SECOND_US);

			if(handler) 
			{
				return (* handler)(argc, argv, body, body_size);					
			} 
			else 
			{
				conmon_audinate_init_query_message(body, audinate_control_map[i].type, CONGESTION_DELAY);
				*body_size = conmon_audinate_query_message_get_size(body);
				return AUD_SUCCESS;
			}
		}
	}
	return AUD_ERR_INVALIDPARAMETER;
}

int main(int argc, char **argv)
{
	aud_error_t result;
	aud_errbuf_t errbuf;
	conmon_client_request_id_t req_id;
	
	aud_env_t * env = NULL;
	conmon_client_t * client = NULL;

	conmon_name_t controlled_name;
	aud_utime_t control_timeout = {5, 0};

	conmon_message_body_t body;
	uint16_t body_size = 0; // = sizeof(conmon_audinate_message_head_t); // message with no payload

	if(argc < 3) 
	{
		usage(argv[0]);
	}
	result = parse_args(argc, argv, &body, &body_size);
	if (result == AUD_ERR_INVALIDPARAMETER)
	{
		usage(argv[0]);
	} 
	else 
	{	

	// get the name of the device to be controlled
		if (!strcmp(argv[1], "localhost"))
		{
			controlled_name[0] = '\0';
		}
		else if (!strcmp(argv[1], "broadcast"))
		{
			controlled_name[0] = '\0';
		}
		else
		{
			SNPRINTF(controlled_name, CONMON_NAME_LENGTH, "%s", argv[1]);
		}

		
		result = aud_env_setup (&env);
		if (result != AUD_SUCCESS)
		{
			printf("Error initialising conmon client library: %s\n",
				aud_error_message(result, errbuf));
			goto cleanup;
		}
	
		result = conmon_client_new (env, & client, "conmon_audinate_controller");
		if (client == NULL)
		{
			printf("Error creating client: %s\n", aud_error_message(result, errbuf));
			goto cleanup;
		}
	
		// set before connecting to avoid possible race conditions / missed notifications
		conmon_client_set_networks_changed_callback(client, handle_networks_changed);
	
		result = conmon_client_connect (client, & handle_response, & req_id); // store client at pos 0 of array
		if (result == AUD_SUCCESS)
		{
			printf("Connecting, request id is 0x%p\n", req_id);
		}
		else
		{
			printf("Error connecting client: %s\n", aud_error_message(result, errbuf));
			goto cleanup;
		}
		result = wait_for_response(client, &comms_timeout);
		if (result != AUD_SUCCESS)
		{
			printf("Error connecting client: %s\n", aud_error_message(result, errbuf));
			goto cleanup;
		}
	
		if((!strncmp(argv[2],"access", 6))||(!strncmp(argv[2],"dante_ready",11)))
		{
			result = conmon_client_send_monitoring_message(client,
				handle_response, &req_id,
				CONMON_CHANNEL_TYPE_LOCAL,
				CONMON_MESSAGE_CLASS_VENDOR_SPECIFIC, CONMON_VENDOR_ID_AUDINATE,
				&body, body_size);
			if (result != AUD_SUCCESS)
			{
				printf ("Error sending local control message (request): %s\n", aud_error_message(result, errbuf));
			}
			else
			{
				printf("sending local control message with request_id %p\n", req_id);
				result = wait_for_response(client, &comms_timeout);
				if (result != AUD_SUCCESS)
				{
					printf ("Error sending local control message (response): %s\n", aud_error_message(result, errbuf));
				}
			}
		}
		else if (!strncmp(argv[2], "master",6) || !strncmp(argv[2], "name_id", 7))
		{
			result = conmon_client_send_monitoring_message(client,
				handle_response, &req_id,
				CONMON_CHANNEL_TYPE_BROADCAST,
				CONMON_MESSAGE_CLASS_VENDOR_SPECIFIC, CONMON_VENDOR_ID_AUDINATE,
				&body, body_size);
			if (result != AUD_SUCCESS)
			{
				printf ("Error sending broadcast message (request): %s\n", aud_error_message(result, errbuf));
			}
			else
			{
				printf("sent broadcast message with request id %p\n", req_id);
				result = wait_for_response(client, &comms_timeout);
				if (result != AUD_SUCCESS)
				{
					printf ("Error sending broadcast message (response): %s\n", aud_error_message(result, errbuf));
				}
			}	
		}
		else
		{
			// we're connected, send control message
			result = conmon_client_send_control_message(client,
					handle_response, &req_id,
					controlled_name, CONMON_MESSAGE_CLASS_VENDOR_SPECIFIC, CONMON_VENDOR_ID_AUDINATE,
				&body, body_size,
				&control_timeout); // the server will give up trying after this long, so that we don't wait forever for a response
			if (result != AUD_SUCCESS)
			{
				printf ("Error sending control message (request): %s\n", aud_error_message(result, errbuf));
			}
			else
			{
				printf("sent control message with request id %p\n", req_id);
				result = wait_for_response(client, &comms_timeout);
				if (result != AUD_SUCCESS)
				{
					printf ("Error sending control message (response): %s\n", aud_error_message(result, errbuf));
				}
			}
		}
	}

cleanup:
	if (client)
	{
		conmon_client_delete(client);
			// Note: this calls conmon_client_disconnect as part of clean-up
	}
	if (env)
	{
		aud_env_release (env);
	}
	return result;
}

static uint32_t parse_address(const char * msg)
{
	uint32_t addr;
	int v1, v2, v3, v4;

	if (sscanf(msg, "%d.%d.%d.%d", &v1, &v2, &v3, &v4) < 4)
	{
		return 0;
	}
	if (v1 < 0 || v1 > 255 || v2 < 0 || v2 > 255 || v3 < 0 || v3 > 255 || v3 < 0 || v3 > 255)
	{
		return 0;
	}
	addr = v1 << 24 | v2 << 16 | v3 << 8 | v4;
	return addr;
}

aud_error_t conmon_audinate_interface_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	char * str;
	const char * domain_name = NULL;
	conmon_audinate_init_interface_control(body, 0);
	
	if(argc == 3)
	{
		// no params send null message
		printf("sending query message\n");
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 5 && !strcmp(argv[3], "redundancy") && !strcmp(argv[4], "true"))
	{
		conmon_audinate_interface_control_set_switch_redundancy(body, AUD_TRUE);
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 5 && !strcmp(argv[3], "redundancy") && !strcmp(argv[4], "false"))
	{
		conmon_audinate_interface_control_set_switch_redundancy(body, AUD_FALSE);
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 5 && !strcmp(argv[4], "dhcp"))
	{
		uint16_t index = (uint16_t) atoi(argv[3]);
		conmon_audinate_interface_control_set_interface_address_dynamic(body, index);
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 6 && !strcmp(argv[4], "ll") && !strcmp(argv[5], "true"))
	{
		uint16_t index = (uint16_t) atoi(argv[3]);
		conmon_audinate_interface_control_set_link_local_enabled(body, index, AUD_TRUE);
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 6 && !strcmp(argv[4], "ll") && !strcmp(argv[5], "false"))
	{
		uint16_t index = (uint16_t) atoi(argv[3]);
		conmon_audinate_interface_control_set_link_local_enabled(body, index, AUD_FALSE);
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 6 && !strcmp(argv[4], "ll_delay") && !strcmp(argv[5], "true"))
	{
		uint16_t index = (uint16_t) atoi(argv[3]);
		conmon_audinate_interface_control_set_link_local_delayed(body, index, AUD_TRUE);
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 6 && !strcmp(argv[4], "ll_delay") && !strcmp(argv[5], "false"))
	{
		uint16_t index = (uint16_t) atoi(argv[3]);
		conmon_audinate_interface_control_set_link_local_delayed(body, index, AUD_FALSE);
		*body_size = conmon_audinate_interface_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc >= 7 && !strcmp(argv[4], "static"))
	{
		uint16_t index = (uint16_t) atoi(argv[3]);
		uint32_t ip_address = htonl(parse_address(argv[5]));
		uint32_t netmask = htonl(parse_address(argv[6]));
		uint32_t dns_server  = 0;
		uint32_t gateway  = 0;
		if(argc >= 8)
		{
			dns_server = htonl(parse_address(argv[7]));
		}
		if(argc >= 9)
		{
			gateway = htonl(parse_address(argv[8]));
		}
		if(argc >= 0x0a && strncmp(argv[9], "domain", 6) == 0 && strlen(argv[9]) > 6)
		{
			str = argv[9] + 7;
			if (str[0])
			{
				domain_name = str;
			}
		}
		if (ip_address && netmask)
		{
			conmon_audinate_interface_control_set_interface_address_static(body, index,
				ip_address, netmask, dns_server, gateway);
			*body_size = conmon_audinate_interface_control_get_size(body);
			if (domain_name)
			{
				conmon_message_size_info_t size = { CONMON_MESSAGE_MAX_BODY_SIZE };	
				size.curr = *body_size;
				conmon_audinate_interface_control_set_interface_domain_name(body,&size ,index,domain_name);
				*body_size = (uint16_t)size.curr;
			}
			return AUD_SUCCESS;
		}
	}
	fprintf(stderr, "%s controlled_device interface [redundancy BOOL | index dhcp | index ll BOOL | index ll_delay BOOL | index static address netmask dns_server gateway domain=]\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}


aud_error_t conmon_audinate_switch_vlan_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	conmon_audinate_init_switch_vlan_control(body, 0);
	if(argc == 3)
	{
		printf("sending query message\n");
		*body_size = conmon_audinate_switch_vlan_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 4 && !strncmp(argv[3], "id=", 3) && strlen(argv[3]) > 3)
	{
		uint16_t id = (uint16_t) atoi(argv[3]+3);
		conmon_audinate_switch_vlan_control_set_config_id(body, id);
		*body_size = conmon_audinate_switch_vlan_control_get_size(body);
		return AUD_SUCCESS;
	}
	fprintf(stderr, "%s controlled_device switch_vlan [id=ID]\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;

}

aud_error_t conmon_audinate_upgrade_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {

	char *str;
	uint32_t server = 0;
	uint16_t port = 0;

	if (argc == 6)
	{
		if(!strncmp(argv[3], "server", 6) && strlen(argv[3]) > 6)
		{
			str = argv[3] + 7;
			server = parse_address(str);
		}
		if(!strncmp(argv[4], "port", 4) && strlen(argv[4]) > 4)
		{
			str = argv[4] + 5;
			port = (uint16_t) atoi(str);
		}
		if(!strncmp(argv[5], "path", 4) && strlen(argv[5]) > 4)
		{
			str = argv[5] + 5;
			if((str != NULL) && server)
			{
				conmon_audinate_init_upgrade_control(body, 0, server, port, str);
				*body_size = conmon_audinate_upgrade_control_get_size(body);
				return AUD_SUCCESS;
			}
		}
	}
	fprintf(stderr, "%s controlled_device upgrade server=server_ip port=port path=path\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}


static char *
find_char
(
	char * src,
	char ch
)
{
	unsigned i;
	for (i = 0; src[i]; i++)
	{
		if (src[i] == ch)
		{
			return &src[i];
		}
	}

	return NULL;
}


static dante_id64_t *
parse_id64
(
	const char * str_in,
	dante_id64_t * dst_id
)
{
	// If it's quoted, then treat as string form
	const char * str = str_in;
	char ch = str[0];
	if (ch == '"' || ch == '\'')
	{
		const char * end;
		str++;
		end = find_char((char *) str, ch);
		if (end && (end - str) >= 8)
		{
			if (dante_id64_from_ascii(dst_id, str) == AUD_SUCCESS)
				return dst_id;
		}
	}
	else
	{
		if (dante_id64_from_hex(dst_id, str) == AUD_SUCCESS)
			return dst_id;
	}
	fprintf(stderr, "Invalid identifier '%s'\n", str_in);
	return NULL;
}


aud_error_t
conmon_audinate_upgrade_v3_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	aud_error_t result;
	conmon_audinate_upgrade_source_file_t src_file = { 0 };
	char * arg;
	char * str;
	char * bin = argv[0];
	dante_id64_t mf, model, *mfp = NULL, *modelp = NULL;

	conmon_message_size_info_t size = { 0 };

	conmon_audinate_init_upgrade_control_v3(body, &size, 0);

	argc -= 3;
	argv += 3;

	if (!argc)
		goto l__error;

	arg = argv[0];
	if (strncmp(arg, "tftp=", 5) == 0)
	{
		arg += 5;
		str = find_char(arg, ':');
		if (str)
		{
			str[0] = 0;
			str++;
			src_file.port = (uint16_t) atoi(str);
			if (! src_file.port)
			{
				fprintf(stderr, "Invalid port '%s'\n", str);
				goto l__error;
			}
		}
		else
		{
			src_file.port = 69;
		}
		src_file.protocol = CONMON_AUDINATE_UPGRADE_PROTOCOL_TFTP_GET;
		src_file.addr_inet.s_addr = inet_addr(arg);
		if (! src_file.addr_inet.s_addr)
		{
			fprintf(stderr, "Invalid TFTP server address '%s'\n", arg);
			goto l__error;
		}

		argc--;
		argv++;
	}
	else if (strcmp(arg, "local") == 0)
	{
		argc--;
		argv++;
	}
	else if (strcmp(arg, "query") == 0)
	{
		argc--;
		argv++;
		if (argc > 0)
		{
			goto l__error;
		}
		* body_size = (uint16_t) size.curr;
		return AUD_SUCCESS;
	}
	else
	{
		goto l__error;
	}

	if (argc)
	{
		arg = argv[0];
		if (strncmp(arg, "path=", 5) == 0)
		{
			arg += 5;
			src_file.filename = arg;

			argc--;
			argv++;
		}
		else
		{
			goto l__error;
		}
	}
	else
	{
		goto l__error;
	}

	if (argc)
	{
		arg = argv[0];
		if (strncmp(arg, "len=", 4) == 0)
		{
			arg += 4;
			src_file.file_len = atoi(arg);

			argc--;
			argv++;
		}
	}

	result =
		conmon_audinate_upgrade_control_set_source_file(
			body, &size, &src_file
		);
	if (result != AUD_SUCCESS)
	{
		return result;
	}

	while (argc)
	{
		arg = argv[0];
		if (strncmp(arg, "mf=", 3) == 0)
		{
			mfp = parse_id64(arg+3, &mf);
			if (! mfp)
				goto l__error;
		}
		else if (strncmp(arg, "model=", 6) == 0)
		{
			modelp = parse_id64(arg+6, &model);
			if (! modelp)
				goto l__error;
		}
		else
		{
			goto l__error;
		}

		argc--;
		argv++;
	}

	conmon_audinate_upgrade_control_set_override(body, mfp, modelp);

	* body_size = (uint16_t) size.curr;
	return AUD_SUCCESS;

l__error:
	fprintf(stderr, "%s controlled_device upgrade3 tftp=server_ip[:port]|local path=path [len=len] [mf=manufacturer_id] [model=model_id]\n", bin);
	fprintf(stderr, "%s controlled_device upgrade3 query\n", bin);
	return AUD_ERR_INVALIDPARAMETER;
}

aud_error_t
conmon_audinate_clear_config_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	conmon_message_size_info_t size = { 0 };
	conmon_audinate_clear_config_action_t action = CONMON_AUDINATE_CLEAR_CONFIG_QUERY;

	conmon_audinate_init_clear_config_control(body, &size, 0);
	
	if ((argc == 4) && !strcmp(argv[3], "keep_ip"))
		action = CONMON_AUDINATE_CLEAR_CONFIG_KEEP_IP;
	else if ((argc == 4) && !strcmp(argv[3], "all"))
		action = CONMON_AUDINATE_CLEAR_CONFIG_CLEAR_ALL;
	else if ((argc == 4) && !strcmp(argv[3], "query"))
		action = CONMON_AUDINATE_CLEAR_CONFIG_QUERY;
	else if (argc >= 3)
	{
		fprintf(stderr, "%s controlled_device %s [keep_ip|all|query]\n", argv[0], argv[2]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	
	conmon_audinate_clear_config_control_set_action(body, &size, action);

	* body_size = (uint16_t) size.curr;
	return AUD_SUCCESS;
}

aud_error_t
conmon_audinate_gpio_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	uint16_t action = CONMON_AUDINATE_GPIO_CONTROL_FIELD_QUERY_STATE;
	uint32_t output_state_valid_mask;
	uint32_t output_state_values;
	char * str;

	output_state_valid_mask = output_state_values = 0;

	if ((argc == 4) && !strcmp(argv[3], "query"))
		action = CONMON_AUDINATE_GPIO_CONTROL_FIELD_QUERY_STATE;
	else if ((argc == 6) && !strcmp(argv[3], "output"))
	{
		action = CONMON_AUDINATE_GPIO_CONTROL_FIELD_OUTPUT_STATE;

		if(!strncmp(argv[4], "mask=0x", 7) && strlen(argv[4]) > 7)
		{
			str = argv[4] + 7;
			sscanf(str, "%x", &output_state_valid_mask);
		}

		if(!strncmp(argv[5], "val=0x", 6) && strlen(argv[5]) > 6)
		{
			str = argv[5] + 6;
			sscanf(str, "%x", &output_state_values);
		}
	}
	else if (argc >= 3)
	{
		fprintf(stderr, "%s controlled_device %s [query]\n", argv[0], argv[2]);
		fprintf(stderr, "%s controlled_device %s [output] mask=0xXX val=0xXX\n", argv[0], argv[2]);
		return AUD_ERR_INVALIDPARAMETER;
	}

	conmon_audinate_init_gpio_control(body, 0, action, 1);
	conmon_audinate_gpio_control_state_set_at_index(body, 0, output_state_valid_mask, output_state_values);

	* body_size = (uint16_t) conmon_audinate_gpio_control_get_size(body);
	return AUD_SUCCESS;
}

aud_error_t conmon_audinate_clocking_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	//conmon_audinate_clocking_control_t * clock_control = (conmon_audinate_clocking_control_t *)body;
	uint16_t param;
	char * str;

	conmon_audinate_init_clocking_control(body,0);
	// parse args
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
	}
	else if(!strncmp(argv[3], "src", 3) && strlen(argv[3]) > 3)
	{
		str = argv[3] + 4;
		if(!strncmp(str,"bnc",3)) 
		{
			param = CONMON_AUDINATE_CLOCK_SOURCE_BNC;
		} 
		else if (!strncmp(str,"aes",3))
		{
			param = CONMON_AUDINATE_CLOCK_SOURCE_AES;
		}
		else if (!strncmp(str,"int",3))
		{
			param = CONMON_AUDINATE_CLOCK_SOURCE_INTERNAL;
		} 
		else
		{
			fprintf(stderr,"%s controlled_device clocking src=int|bnc|aes\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_clocking_control_set_source(body, param);
	}	
	else if(!strncmp(argv[3], "pref", 4) && strlen(argv[3]) > 4)
	{
		uint8_t pref;
		str = argv[3] + 5;
		if(!strncmp(str,"true",4)) 
		{
			pref = (uint8_t) AUD_TRUE;
		} 
		else if (!strncmp(str,"false",5))
		{
			pref = (uint8_t) AUD_FALSE;
		}
		else
		{
			fprintf(stderr,"%s controlled_device clocking pref=true|false\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_clocking_control_set_preferred(body, pref);
	}
	else if(!strncmp(argv[3], "udelay", 6) && strlen(argv[3]) > 6)
	{
		aud_bool_t udelay;
		str = argv[3] + 7;
		if(!strncmp(str,"true",4)) 
		{
			udelay = AUD_TRUE;
		} 
		else if (!strncmp(str,"false",5))
		{
			udelay = AUD_FALSE;
		}
		else
		{
			fprintf(stderr,"%s controlled_device clocking udelay=true|false\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_clocking_control_set_unicast_delay_requests(body, udelay);
	}
	else if(!strncmp(argv[3], "subdomain", 9) && strlen(argv[3]) > 9)
	{
		const char * subdomain_name = NULL;
		str = argv[3] + 10;
		if (str[0])
		{
			//subdomain = (conmon_audinate_clock_subdomain_t) atoi(str);
			subdomain_name = str;
		}
		conmon_audinate_clocking_control_set_subdomain_name(body, subdomain_name);

	}
	else if(!strncmp(argv[3], "enabled", 7) && strlen(argv[3]) > 7)
	{
		aud_bool_t enabled;
		str = argv[3] + 8;
		if(!strncmp(str,"true",4)) 
		{
			enabled = AUD_TRUE;
		} 
		else if (!strncmp(str,"false",5))
		{
			enabled = AUD_FALSE;
		}
		else
		{
			fprintf(stderr,"%s controlled_device clocking enabled=true|false\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_clocking_control_set_multicast_ports_enabled(body, enabled);
	}
	else if(!strncmp(argv[3], "slave_only", 10) && strlen(argv[3]) > 10)
        {
                aud_bool_t enabled;
                str = argv[3] + 11;
                if(!strncmp(str,"true",4))
                {
                        enabled = AUD_TRUE;
                }
                else if (!strncmp(str,"false",5))
                {
                        enabled = AUD_FALSE;
                }
                else
                {
                        fprintf(stderr,"%s controlled_device clocking slave_only=true|false\n", argv[0]);
                        return AUD_ERR_INVALIDPARAMETER;
                }
                conmon_audinate_clocking_control_set_slave_only_enabled(body, enabled);
        }
	// TODO set sync mode ?
	else
	{
		fprintf(stderr,"%s controlled_device clocking src=int|bnc|aes\n", argv[0]);
		fprintf(stderr,"%s controlled_device clocking pref=true|false\n", argv[0]);
		fprintf(stderr,"%s controlled_device clocking udelay=true|false\n", argv[0]);
		fprintf(stderr,"%s controlled_device clocking subdomain=[_DFLT|_ALT1|_ALT2|_ALT3|_ALT4]\n", argv[0]);
		fprintf(stderr,"%s controlled_device clocking enabled=true|false\n", argv[0]);
		fprintf(stderr,"%s controlled_device clocking slave_only=true|false\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}

	//*body_size = sizeof(conmon_audinate_clocking_control_t);
	*body_size = conmon_audinate_clocking_control_get_size(body);
	return AUD_SUCCESS;
}


aud_error_t conmon_audinate_unicast_clocking_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	conmon_audinate_init_unicast_clocking_control(body, 0);

	// parse args
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
	}
	else if(!strcmp(argv[3], "enabled=true"))
	{
		conmon_audinate_unicast_clocking_control_set_enabled(body, AUD_TRUE);
	}
	else if(!strcmp(argv[3], "enabled=false"))
	{
		conmon_audinate_unicast_clocking_control_set_enabled(body, AUD_FALSE);
	}
	else if(!strcmp(argv[3], "reload"))
	{
		conmon_audinate_unicast_clocking_control_set_reload_devices(body, AUD_TRUE);
	}
	else
	{
		fprintf(stderr,"%s controlled_device uclocking enabled=true|false\n", argv[0]);
		fprintf(stderr,"%s controlled_device clocking reload\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}

	*body_size = conmon_audinate_unicast_clocking_control_get_size(body);
	return AUD_SUCCESS;
}


aud_error_t conmon_audinate_name_id_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	uint16_t d, nd;
	if (argc == 3)
	{
		fprintf(stderr,"%s controlled_device [name=NAME | id=DEVICE_ID:PROCESS_ID | uuid=UUID]*\n",argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	nd = (uint16_t) (argc-3);
	conmon_audinate_init_name_id_control(body, 0, nd);
	for (d = 0; d < nd; d++)
	{
		const char * str = argv[d+3];
		int i, x[8], p;
		if (!strncmp(str, "name=", 5) && strlen(str) > 5)
		{
			conmon_audinate_name_id_set_device_name_id_at_index(body, d, str+5, NULL, NULL, NULL);
		}
		else if (!strncmp(str, "id=", 3) && sscanf(str+3, "%02x%02x%02x%02x%02x%02x%02x%02x:%04x",
			x+0, x+1, x+2, x+3, x+4, x+5, x+6, x+7, &p) == 9)
		{
			conmon_instance_id_t instance_id;
			for (i = 0; i < CONMON_DEVICE_ID_LENGTH; i++)
			{
				instance_id.device_id.data[i] = (uint8_t) x[i];
			}
			instance_id.process_id = (uint16_t) p;
			conmon_audinate_name_id_set_device_name_id_at_index(body, d, NULL, NULL, &instance_id, NULL);
		}
		else if (!strncmp(str, "uuid=", 5) && sscanf(str+5, "%02x:%02x:%02x:%02x:%02x:%02x", 
			x+0, x+1, x+2, x+3, x+4, x+5) == 6)
		{
			conmon_audinate_clock_uuid_t uuid;
			for (i = 0; i < CONMON_AUDINATE_CLOCK_UUID_LENGTH; i++)
			{
				uuid.data[i] = (uint8_t) x[i];
			}
			conmon_audinate_name_id_set_device_name_id_at_index(body, d, NULL, NULL, NULL, &uuid);
		}
		else
		{
			fprintf(stderr,"%s controlled_device [name=NAME | id=DEVICE_ID:PROCESS_ID | uuid=UUID]*\n",argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
	}
	*body_size = conmon_audinate_name_id_get_size(body);
	return AUD_SUCCESS;
}


aud_error_t conmon_audinate_igmp_vers_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	uint16_t version;
	//conmon_audinate_igmp_version_control_t * igmp_control = (conmon_audinate_igmp_version_control_t *)body;
	conmon_audinate_init_igmp_version_control(body, CONGESTION_DELAY);
	// parse args
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
	}
	else if(!strncmp(argv[3], "vers", 4) && strlen(argv[3]) > 4)
	{
		version = (uint16_t) atoi(argv[3] + 5);
		if(version < 1 || version > 3) 
		{
			fprintf(stderr,"%s controlled_device igmp vers=1|2|3\n",argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		else
		{
			conmon_audinate_igmp_version_control_set_version(body, version);
		}
	}	
	else
	{
		fprintf(stderr,"%s controlled_device igmp vers=1|2|3\n",argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	//*body_size = sizeof(conmon_audinate_igmp_version_control_t);
	*body_size = conmon_audinate_igmp_version_control_get_size(body);
	return AUD_SUCCESS;
}


aud_error_t
conmon_audinate_ifstats_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	conmon_audinate_init_ifstats_control (body, 0);
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
	}
	else
	{
		char * arg = argv [3];
		if (strcmp (arg, "clear") == 0)
		{
			conmon_audinate_ifstats_control_set_clear_errors(body);
		}
		else
		{
			goto l__usage;
		}
	}

	*body_size = (uint16_t) conmon_audinate_ifstats_control_get_size(body);
	return AUD_SUCCESS;

l__usage:
	fprintf(stderr,"%s controlled_device ifstats clear\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}


#define CHECK_SRATE(x) ( x == CONMON_AUDINATE_SRATE_44K || x == CONMON_AUDINATE_SRATE_48K  || x == CONMON_AUDINATE_SRATE_96K || x == CONMON_AUDINATE_SRATE_192K ||  x == CONMON_AUDINATE_SRATE_88K ||  x == CONMON_AUDINATE_SRATE_176K) 	 

aud_error_t
conmon_audinate_srate_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	conmon_audinate_init_srate_control (body, 0);
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message

		*body_size = conmon_audinate_srate_control_get_size(body);
		return AUD_SUCCESS;
	}
	else
	{
		char * arg = argv [3];

		char * value = strchr (arg, '=');
		if (! value)
		{
			goto l__usage;
		}

		* value = 0;
		value ++;
		if (! * value)
		{
			goto l__usage;
		}
		
		if (strcmp (arg, "rate") == 0)
		{
			uint32_t rate = atol (value);

			if (CHECK_SRATE(rate))
			{
				conmon_audinate_srate_control_set_rate(body, rate);
				*body_size = conmon_audinate_srate_control_get_size(body);
				return AUD_SUCCESS;
			}
			else
			{
				goto l__usage;
			}
		}
		else
		{
			goto l__usage;
		}
	}

l__usage:
	fprintf(stderr,"%s controlled_device srate rate=44100|48000|88200|96000|176400|192000\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}


aud_error_t
conmon_audinate_enc_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	conmon_audinate_init_enc_control (body, 0);
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message

		*body_size = conmon_audinate_enc_control_get_size(body);
		return AUD_SUCCESS;
	}
	else
	{
		char * arg = argv [3];

		char * value = strchr (arg, '=');
		if (! value)
		{
			goto l__usage;
		}

		* value = 0;
		value ++;
		if (! * value)
		{
			goto l__usage;
		}
		
		if (strcmp (arg, "enc") == 0)
		{
			uint16_t enc = (uint16_t) atoi (value);
			if (! enc)
			{
				if (strcmp (value, "pcm24") == 0)
				{
					enc = 24;
				}
				else if (strcmp (value, "raw32") == 0)
				{
					enc = 0x1300;
				}
				else
				{
					goto l__usage;
				}
			}

			conmon_audinate_enc_control_set_encoding (body, enc);
			*body_size = conmon_audinate_enc_control_get_size (body);
			return AUD_SUCCESS;
		}
	}

l__usage:
	fprintf(stderr,"%s controlled_device enc enc=<n>|pcm24|raw32\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}

#define PULLUP_STRING_NONE "0"
#define PULLUP_STRING_PLUS4 "+4.1667"
#define PULLUP_STRING_PLUS01 "+0.1"
#define PULLUP_STRING_MINUS01 "-0.1"
#define PULLUP_STRING_MINUS4 "-4.0"

aud_error_t
conmon_audinate_srate_pullup_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	uint32_t value;
	char * str;

	conmon_audinate_init_srate_pullup_control (body, 0);

	// parse args
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
		*body_size = conmon_audinate_srate_pullup_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 5)
	{
		if(!strncmp(argv[3], "val", 3) && strlen(argv[3]) > 3)
		{
			str = argv[3] + 4;
			if(!strcmp(str,PULLUP_STRING_NONE))
			{
				value = CONMON_AUDINATE_SRATE_PULLUP_NONE;
			} 
			else if (!strcmp(str,PULLUP_STRING_PLUS4))
			{
				value =  CONMON_AUDINATE_SRATE_PULLUP_PLUSFOURPOINTONESIXSIXSEVEN;
			}
			else if (!strcmp(str,PULLUP_STRING_PLUS01))
			{
				value =  CONMON_AUDINATE_SRATE_PULLUP_PLUSPOINTONE;
			} 
			else if (!strcmp(str,PULLUP_STRING_MINUS01))
			{
				value =  CONMON_AUDINATE_SRATE_PULLUP_MINUSPOINTONE;
			} 
			else if (!strcmp(str,PULLUP_STRING_MINUS4))
			{
				value =  CONMON_AUDINATE_SRATE_PULLUP_MINUSFOUR;
			} 
			else
			{
				goto l__usage;
			}
			conmon_audinate_srate_pullup_control_set_rate(body, value);	
		}
		if(!strncmp(argv[4], "subdomain", 9) && strlen(argv[4]) > 9)
		{
			const char * subdomain_name = NULL;
			str = argv[4] + 10;
			if (str[0])
			{
				subdomain_name = str;
			}
			conmon_audinate_srate_pullup_control_set_subdomain(body, subdomain_name);
		}	
		*body_size = conmon_audinate_srate_pullup_control_get_size(body);
		return AUD_SUCCESS;
	}
l__usage:
		fprintf(stderr,"%s controlled_device pullup val="
			PULLUP_STRING_NONE "|" PULLUP_STRING_PLUS4 "|"
			PULLUP_STRING_PLUS01 "|" PULLUP_STRING_MINUS01 "|" PULLUP_STRING_MINUS4 " subdomain=[_DFLT|_ALT1|_ALT2|_ALT3|_ALT4]\n"
			, argv[0]
		);
		return AUD_ERR_INVALIDPARAMETER;
}


aud_error_t conmon_audinate_edk_board_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	//conmon_audinate_edk_board_control_t * edk_control = (conmon_audinate_edk_board_control_t *)body;
	uint16_t param;
	char * str;

	conmon_audinate_init_edk_board_control(body, CONGESTION_DELAY);
	// parse args
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
	}
	else if(!strncmp(argv[3], "rev", 3) && strlen(argv[3]) > 3)
	{
		str = argv[3] + 4;
		if(!strncmp(str,"other",5)) 
		{
			param = CONMON_AUDINATE_EDK_BOARD_REV_OTHER;
		} 
		else if (!strncmp(str,"red",3))
		{
			param = CONMON_AUDINATE_EDK_BOARD_REV_RED;
		}
		else if (!strncmp(str,"green",5))
		{
			param = CONMON_AUDINATE_EDK_BOARD_REV_GREEN;
		} 
		else
		{
			fprintf(stderr,"%s controlled_device edk rev=other|red|green\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_edk_board_set_rev(body, param);
	}	
	else if(!strncmp(argv[3], "pad", 3) && strlen(argv[3]) > 3)
	{
		int db;
		str = argv[3] + 4;
		db=atoi(str);
		if(db == 0)
		{
			param = CONMON_AUDINATE_EDK_BOARD_PAD_0DB;
		}
		else if (db == -6)
		{
			param = CONMON_AUDINATE_EDK_BOARD_PAD_MINUS6DB;
		}
		else if (db == -12)
		{
			param = CONMON_AUDINATE_EDK_BOARD_PAD_MINUS12DB;
		}
		else if (db == -24)
		{
			param = CONMON_AUDINATE_EDK_BOARD_PAD_MINUS24DB;
		}
		else if (db == -48)
		{
			param = CONMON_AUDINATE_EDK_BOARD_PAD_MINUS48DB;
		}
		else
		{
			fprintf(stderr,"%s controlled_device edk pad=0|-6|-12|-24|-48\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_edk_board_set_pad(body, param);
	}	
	else if(!strncmp(argv[3], "dig", 3) && strlen(argv[3]) > 3)
	{
		str = argv[3] + 4;
		if(!strncmp(str,"spdif",5)) 
		{
			param = CONMON_AUDINATE_EDK_BOARD_DIG_SPDIF;
		}
		else if(!strncmp(str,"aes",3))
		{
			param = CONMON_AUDINATE_EDK_BOARD_DIG_AES;
		}
		else if(!strncmp(str,"tos",3))
		{
			param = CONMON_AUDINATE_EDK_BOARD_DIG_TOSLINK;
		}
		else
		{
			fprintf(stderr,"%s controlled_device edk dig=spdif|aes|tos\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_edk_board_set_dig(body, param);
	}
	else if(!strncmp(argv[3], "src", 3) && strlen(argv[3]) > 3)
	{
		str = argv[3] + 4;
		if(!strncmp(str,"sync",4)) 
		{
			param = CONMON_AUDINATE_EDK_BOARD_SRC_SYNC;
		}
		else if(!strncmp(str,"async",5)) 
		{
			param = CONMON_AUDINATE_EDK_BOARD_SRC_ASYNC;
		}
		else
		{
			fprintf(stderr,"%s controlled_device edk src=sync|async\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_edk_board_set_src(body, param);
	}
	else
	{
		fprintf(stderr,"%s controlled_device edk rev=other|red|green\n", argv[0]);
		fprintf(stderr,"%s controlled_device edk pad=0|-6|-12|-24|-48\n", argv[0]);
		fprintf(stderr,"%s controlled_device edk dig=spdif|aes|tos\n", argv[0]);
		fprintf(stderr,"%s controlled_device edk src=sync|async\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}

	//*body_size = sizeof(conmon_audinate_edk_board_control_t);
	*body_size = conmon_audinate_edk_board_control_get_size(body);
	return AUD_SUCCESS;
}

aud_error_t conmon_audinate_sys_reset_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	//conmon_audinate_sys_reset_control_t * rst_control = (conmon_audinate_sys_reset_control_t *)body;
	uint16_t param;
	char * str;
	// parse args
	conmon_audinate_init_sys_reset_control(body, CONGESTION_DELAY);

	if(argv[3] && !strncmp(argv[3], "mode", 4) && strlen(argv[3]) > 4)
	{
		str = argv[3] + 5;
		if(!strncmp(str,"soft",4)) 
		{
			param = CONMON_AUDINATE_SYS_RESET_SOFT;
		}
		else if(!strncmp(str,"factory",7)) 
		{
			param = CONMON_AUDINATE_SYS_RESET_FACTORY;
		}
		else
		{
			fprintf(stderr,"%s controlled_device sysreset mode=soft|factory\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_sys_reset_control_set_mode(body, param);
	}
	else
	{
		fprintf(stderr,"%s controlled_device sysreset mode=soft|factory\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	//*body_size = sizeof(conmon_audinate_sys_reset_control_t);
	*body_size = conmon_audinate_sys_reset_control_get_size(body);
	return AUD_SUCCESS;
}

aud_error_t conmon_audinate_access_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	uint16_t param;
	char * str;
	// parse args
	conmon_audinate_init_access_control(body, CONGESTION_DELAY);
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
	}
	else if(!strncmp(argv[3], "mode", 4) && strlen(argv[3]) > 4)
	{
		str = argv[3] + 5;
		if(!strncmp(str,"enable",6)) 
		{
			param = CONMON_AUDINATE_ACCESS_ENABLE;
		}
		else if(!strncmp(str,"disable",7)) 
		{
			param = CONMON_AUDINATE_ACCESS_DISABLE;
		}
		else if(!strncmp(str,"inetd_enable",7))
		{
			param = CONMON_AUDINATE_INETD_ENABLE;
		}
		else if(!strncmp(str,"inetd_disable",7)) 
		{
			param = CONMON_AUDINATE_INETD_DISABLE;
		}
		else
		{
			fprintf(stderr,"%s controlled_device access mode=enable|disable|inetd_enable|inetd_disable\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_access_control_set_mode(body, param);
	}
	else
	{
		fprintf(stderr,"%s controlled_device access mode=enable|disable|inetd_enable|inetd_disable\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	//*body_size = sizeof(conmon_audinate_access_control_t);
	*body_size = conmon_audinate_access_control_get_size(body);
	return AUD_SUCCESS;
}


aud_error_t conmon_audinate_rx_error_threshold_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
)
{
	uint16_t win;
	uint16_t thres;
	uint16_t reset;
	//char * str;
	// parse args
	conmon_audinate_init_rx_error_threshold_control(body, CONGESTION_DELAY);
	if(argc == 3)
	{
		printf("sending query message\n");
		// no params send null message
		goto send_msg;
	}
	else if(!strncmp(argv[3], "thres", 5) && strlen(argv[3]) > 5)
	{
		thres = (uint16_t) atoi(argv[3] + 6);
		conmon_audinate_rx_error_threshold_set_threshold(body, thres);
	}
	else
	{
		fprintf(stderr,"%s controlled_device errthres thres=val win=val reset=val\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	if(argv[4] && !strncmp(argv[4], "win", 3) && strlen(argv[4]) > 3)
	{
		win = (uint16_t) atoi(argv[4] + 4);
		conmon_audinate_rx_error_threshold_set_window(body, win);
	}
	else
	{
		fprintf(stderr,"%s controlled_device errthres thres=val win=val reset=val\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	if(argv[5] && !strncmp(argv[5], "reset", 5) && strlen(argv[5]) > 5)
	{
		reset = (uint16_t) atoi(argv[5] + 6);
		conmon_audinate_rx_error_threshold_set_reset_time(body, reset);
	}
	else
	{
		fprintf(stderr,"%s controlled_device errthres thres=val win=val reset=val\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	send_msg:
	*body_size = conmon_audinate_rx_error_threshold_control_get_size(body);
	return AUD_SUCCESS;
}


aud_error_t conmon_audinate_metering_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	conmon_audinate_init_metering_control(body, CONGESTION_DELAY);
	if(argc == 3)
	{
		printf("sending query message\n");
		*body_size = conmon_audinate_metering_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 4)
	{
		uint32_t rate;

		if (!strncmp(argv[3], "rate=", 5) && strlen(argv[3]) > 5)
		{
			rate = (uint16_t) atoi(argv[3]+5);
		}
		else
		{
			goto __error;
		}
		
		conmon_audinate_metering_control_set_update_rate(body, rate);
		*body_size = conmon_audinate_metering_control_get_size(body);
		return AUD_SUCCESS;
	}

__error:
	fprintf(stderr,"%s controlled_device metering rate=RATE\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}


aud_error_t conmon_audinate_serial_port_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	conmon_audinate_init_serial_port_control(body, CONGESTION_DELAY);
	if(argc == 3)
	{
		printf("sending query message\n");
		*body_size = conmon_audinate_serial_port_control_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 8)
	{
		uint16_t index;
		conmon_audinate_serial_port_baud_rate_t baud_rate;
		conmon_audinate_serial_port_bits_t bits;
		conmon_audinate_serial_port_parity_t parity;
		conmon_audinate_serial_port_stop_bits_t stop_bits;

		if (!strncmp(argv[3], "index=", 6) && strlen(argv[3]) > 6)
		{
			index = (uint16_t) atoi(argv[3]);
		}
		else
		{
			goto __error;
		}
		if (!strncmp(argv[4], "speed=", 6) && strlen(argv[4]) > 6)
		{
			baud_rate = (conmon_audinate_serial_port_baud_rate_t) atoi(argv[4]+6);
		}
		else
		{
			goto __error;
		}
		if (!strncmp(argv[5], "bits=", 5) && strlen(argv[5]) > 5)
		{
			bits = (conmon_audinate_serial_port_bits_t) atoi(argv[5]+5);
		}
		else
		{
			goto __error;
		}
		if (!strcmp(argv[6], "parity=none"))
		{
			parity = CONMON_AUDINATE_SERIAL_PORT_PARITY_NONE;
		}
		else if (!strcmp(argv[6], "parity=odd"))
		{
			parity = CONMON_AUDINATE_SERIAL_PORT_PARITY_ODD;
		}
		else if (!strcmp(argv[6], "parity=even"))
		{
			parity = CONMON_AUDINATE_SERIAL_PORT_PARITY_EVEN;
		}
		else
		{
			goto __error;
		}
		if (!strncmp(argv[7], "stop=", 5) && strlen(argv[7]) > 5)
		{
			stop_bits = (conmon_audinate_serial_port_stop_bits_t) atoi(argv[7]+5);
		}
		else
		{
			goto __error;
		}
		
		conmon_audinate_serial_port_control_set_port(body, index,
			baud_rate, bits, parity, stop_bits);
		*body_size = conmon_audinate_serial_port_control_size(body);
		return AUD_SUCCESS;
	}

__error:
	fprintf(stderr,"%s controlled_device serial index=val speed=val bits=7|8 parity=none|odd|even stop=0|1|2\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}

aud_error_t conmon_audinate_haremote_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	conmon_audinate_init_haremote_control(body, CONGESTION_DELAY);
	if(argc == 3)
	{
		printf("sending query message\n");
		*body_size = conmon_audinate_haremote_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if (argc == 4)
	{
		conmon_audinate_haremote_bridge_mode_t mode;
		if (!strcmp(argv[3], "mode=all"))
		{
			mode = CONMON_AUDINATE_HAREMOTE_BRIDGE_MODE_ALL;
		}
		else if (!strcmp(argv[3], "mode=none"))
		{
			mode = CONMON_AUDINATE_HAREMOTE_BRIDGE_MODE_NONE;
		}
		else if (!strcmp(argv[3], "mode=slot_db9"))
		{
			mode = CONMON_AUDINATE_HAREMOTE_BRIDGE_MODE_SLOT_DB9;
		}
		else if (!strcmp(argv[3], "mode=network_slot"))
		{
			mode = CONMON_AUDINATE_HAREMOTE_BRIDGE_MODE_NETWORK_SLOT;
		}
		else if (!strcmp(argv[3], "mode=network_db9"))
		{
			mode = CONMON_AUDINATE_HAREMOTE_BRIDGE_MODE_NETWORK_DB9;
		}
		else
		{
			goto __error;
		}
		conmon_audinate_haremote_control_set_bridge_mode(body, mode);
		*body_size = conmon_audinate_haremote_control_get_size(body);
		return AUD_SUCCESS;
	}

__error:
	fprintf(stderr,"%s controlled_device haremote mode=all|none|slot_db9|network_slot|network_db9\n", argv[0]);
	return AUD_ERR_INVALIDPARAMETER;
}

aud_error_t conmon_audinate_ptp_logging_control_handler
(
	int argc,
	char ** argv,
	conmon_message_body_t *body,
	uint16_t *body_size
) {
	char * str;

	conmon_audinate_init_ptp_logging_control(body, CONGESTION_DELAY);
	if(argc == 3)
	{
		printf("sending query message\n");
		*body_size = conmon_audinate_ptp_logging_control_get_size(body);
		return AUD_SUCCESS;
	}
	else if(!strncmp(argv[3], "enabled", 7) && strlen(argv[3]) > 7)
	{
		aud_bool_t enabled;
		str = argv[3] + 8;
		if(!strncmp(str,"true",4)) 
		{
			enabled = AUD_TRUE;
		} 
		else if (!strncmp(str,"false",5))
		{
			enabled = AUD_FALSE;
		}
		else
		{
			fprintf(stderr,"%s controlled_device ptp_logging enabled=true|false\n", argv[0]);
			return AUD_ERR_INVALIDPARAMETER;
		}
		conmon_audinate_ptp_logging_network_set_enabled(body,enabled);
		*body_size = conmon_audinate_ptp_logging_control_get_size(body);
	}
	else
	{
		fprintf(stderr,"%s controlled_device ptp_logging enabled=true|false\n", argv[0]);
		return AUD_ERR_INVALIDPARAMETER;
	}
	return AUD_SUCCESS;
}
