/*
 * File     : $RCSfile$
 * Created  : January 2007
 * Updated  : $Date$
 * Author   : James Westendorp
 * Synopsis : A test harness for the audinate DLL demonstrating its use.
 *
 * This software is copyright (c) 2007 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1 
 */
#include "audinate/dante_api.h"
#include <stdio.h>
#include <signal.h>

#ifdef WIN32
#define SNPRINTF _snprintf
#include <conio.h>
#else
#include <stdlib.h>
#define SNPRINTF snprintf
#endif

#define MAX_RESOLVES 16
#define MAX_SOCKETS (MAX_RESOLVES+2) // need space for device and channel browsing

static aud_bool_t g_running = AUD_TRUE;

db_browse_node_changed_fn db_test_node_changed;
db_browse_network_changed_fn db_test_network_changed;
db_browse_sockets_changed_fn db_test_sockets_changed;

void sig_handler(int sig);

void sig_handler(int sig)
{
	AUD_UNUSED(sig);
	signal(SIGINT, sig_handler);
	g_running = AUD_FALSE;
}

typedef struct
{
	aud_env_t * env;
	db_browse_t * browse;

	aud_bool_t print_node_changes;
	aud_bool_t print_network_changes;
	aud_bool_t print_socket_changes;

	aud_bool_t network_changed;
	aud_bool_t sockets_changed;

	dante_sockets_t sockets;

	aud_errbuf_t errbuf;
} db_browse_test_t;

//----------------------------------------------------------
// Print functions
//----------------------------------------------------------

static const char *
db_test_browse_types_to_string
(
	db_browse_types_t browse_types,
	char * buf,
	size_t len
) {
	size_t offset = 0;
	aud_bool_t first = AUD_TRUE;

	buf[0] = '\0';

	if (browse_types & DB_BROWSE_TYPE_MEDIA_DEVICE)
	{
		offset += SNPRINTF(buf+offset, len-offset, "%sMEDIA", (first ? "" : "|"));
		first = AUD_FALSE;
	}
	if (browse_types & DB_BROWSE_TYPE_MEDIA_CHANNEL)
	{
		offset += SNPRINTF(buf+offset, len-offset, "%sCHANNEL", (first ? "" : "|"));
		first = AUD_FALSE;
	}
	if (browse_types & DB_BROWSE_TYPE_CONMON_DEVICE)
	{
		offset += SNPRINTF(buf+offset, len-offset, "%sCONMON", (first ? "" : "|"));
		first = AUD_FALSE;
	}
	if (browse_types & DB_BROWSE_TYPE_SAFE_MODE_DEVICE)
	{
		offset += SNPRINTF(buf+offset, len-offset, "%sSAFE_MODE", (first ? "" : "|"));
		first = AUD_FALSE;
	}

	return buf;
}

static const char *
db_test_node_change_to_string
(
	db_node_change_t node_change
) {
	switch (node_change)
	{
	case DB_NODE_CHANGE_ADDED: return "ADDED";
	case DB_NODE_CHANGE_MODIFIED: return "MODIFIED";
	case DB_NODE_CHANGE_REMOVED: return "REMOVED";
	};
	return "?";
}

static const char *
db_test_node_type_to_string
(
	db_node_type_t node_type
) {
	switch (node_type)
	{
	case DB_NODE_TYPE_DEVICE: return "DEVICE";
	case DB_NODE_TYPE_CHANNEL: return "CHANNEL";
	case DB_NODE_TYPE_LABEL: return "LABEL";
	};
	return "?";
}

static const char *
db_test_print_formats
(
	const dante_formats_t * formats,
	char * buf,
	size_t len
) {
	uint16_t e, nnpe = dante_formats_num_non_pcm_encodings(formats);
	size_t offset = 0;
	dante_samplerate_t rate = dante_formats_get_samplerate(formats);
	dante_encoding_t native = dante_formats_get_native_encoding(formats);
	dante_encoding_t native_pcm = dante_formats_get_native_pcm(formats);
	dante_encoding_pcm_map_t pcm_map = dante_formats_get_pcm_map(formats);
	const dante_encoding_t * np_encodings = dante_formats_get_non_pcm_encodings(formats);
	
	if (native)
	{
		offset += SNPRINTF(buf+offset, len-offset, "%u/[", rate);
		for (e = 0; e < nnpe; e++)
		{
			dante_encoding_t enc = np_encodings[e];
			offset += SNPRINTF(buf+offset, len-offset, "%s%s%u", 
				(e > 0 ? "," : ""), 
				(enc == native ? "*" : ""),
				enc);
		}
		if (native_pcm)
		{
			offset += SNPRINTF(buf+offset, len-offset, "%s%s%d->0x%04x", 
				(nnpe ? "," : ""), 
				(native_pcm  == native ? "*" : ""),
				native_pcm, pcm_map);
		}
		offset += SNPRINTF(buf+offset, len-offset, "]");
	}
	else
	{
		strcpy(buf, "-");
	}
	return buf;
}

static void
db_test_print_device
(
	db_browse_test_t * test,
	const db_browse_device_t * device
)  {
	char temp[64];
	const char * name = db_browse_device_get_name(device);
	
	const char * default_name = db_browse_device_get_default_name(device);
	db_browse_types_t all_types, network_types[DB_BROWSE_MAX_INTERFACE_INDEXES], localhost_types;
	unsigned int n, nn = db_browse_num_interface_indexes(test->browse);
		
	printf("name=\"%s\"", name);
	
	all_types = db_browse_device_get_browse_types(device);
	printf(" all_types=%s", db_test_browse_types_to_string(all_types, temp, sizeof(temp)));
	for (n = 0; n < nn; n++)
	{
		network_types[n] = db_browse_device_get_browse_types_on_network(device, n);
		printf(" network_types[%d]=%s", n, db_test_browse_types_to_string(network_types[n], temp, sizeof(temp)));
	}
	if (db_browse_using_localhost(test->browse))
	{
		localhost_types = db_browse_device_get_browse_types_on_localhost(device);
		printf(" localhost_types=%s", db_test_browse_types_to_string(localhost_types, temp, sizeof(temp)));
	}

	if (all_types)
	{
		printf(" default_name=\"%s\"", default_name);
	}
	if (all_types & DB_BROWSE_TYPE_MEDIA_DEVICE)
	{
		const dante_version_t * router_version = db_browse_device_get_router_version(device);
		const dante_version_t * arcp_version = db_browse_device_get_arcp_version(device);
		const dante_version_t * arcp_min_version = db_browse_device_get_arcp_min_version(device);
		const char * router_info = db_browse_device_get_router_info(device);

		printf(" router_version=%u.%u.%u", router_version->major, router_version->minor, router_version->bugfix);
		printf(" arcp_version=%u.%u.%u", arcp_version->major, arcp_version->minor, arcp_version->bugfix);
		printf(" arcp_min_version=%u.%u.%u", arcp_min_version->major, arcp_min_version->minor, arcp_min_version->bugfix);
		printf(" router_info=\"%s\"", router_info ? router_info : "");	
	}
	if (all_types & DB_BROWSE_TYPE_SAFE_MODE_DEVICE)
	{
		uint16_t safe_mode_version = db_browse_device_get_safe_mode_version(device);
		printf("Safe mode %u",safe_mode_version);
	}
	if (all_types & DB_BROWSE_TYPE_CONMON_DEVICE)
	{
		const conmon_instance_id_t * instance_id = db_browse_device_get_instance_id(device);
		const uint8_t * d = instance_id->device_id.data;

		const dante_id64_t * vendor_id = db_browse_device_get_vendor_id(device);
		const uint8_t * v = vendor_id ? vendor_id->data : NULL;

		uint32_t vendor_broadcast_address = db_browse_device_get_vendor_broadcast_address(device);

		printf(" instance_id=%02x%02x%02x%02x%02x%02x%02x%02x/%d",
			d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], instance_id->process_id);
		if (v)
		{
			printf(" vendor_id=%02x%02x%02x%02x%02x%02x%02x%02x",
				v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
		}
		if (vendor_broadcast_address)
		{
			uint8_t * a = (uint8_t *) &vendor_broadcast_address;
			printf(" vba=%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
		}
	}
	if (all_types & (DB_BROWSE_TYPE_CONMON_DEVICE | DB_BROWSE_TYPE_MEDIA_DEVICE))
	{
		const dante_id64_t * mf_id = db_browse_device_get_manufacturer_id(device);
		const dante_id64_t * model_id = db_browse_device_get_model_id(device);
		char id_buf[DANTE_ID64_DNSSD_BUF_LENGTH];

		if (mf_id)
		{
			dante_id64_to_dnssd_text(mf_id, id_buf);
			printf(" mf=%s", id_buf);
		}
		if (model_id)
		{
			dante_id64_to_dnssd_text(model_id, id_buf);
			printf(" model=%s", id_buf);
		}
	}
}

static void
db_test_print_channel
(
	db_browse_test_t * test,
	const db_browse_channel_t * channel
)  {
	char temp[64];
	dante_id_t id = db_browse_channel_get_id(channel);
	const char * canonical_name = db_browse_channel_get_canonical_name(channel);
	const char * device_name = db_browse_device_get_name(db_browse_channel_get_device(channel));
	const dante_formats_t * formats = db_browse_channel_get_formats(channel);

	db_browse_types_t all_types, network_types[DB_BROWSE_MAX_INTERFACE_INDEXES], localhost_types;
	unsigned int n, nn = db_browse_num_interface_indexes(test->browse);

	printf(" id=%u device=\"%s\"", id, device_name);
		
	all_types = db_browse_channel_get_browse_types(channel);
	printf(" all_types=%s", db_test_browse_types_to_string(all_types, temp, sizeof(temp)));
	for (n = 0; n < nn; n++)
	{
		network_types[n] = db_browse_channel_get_browse_types_on_network(channel, n);
		printf(" network_types[%d]=%s", n, db_test_browse_types_to_string(network_types[n], temp, sizeof(temp)));
	}
	if (db_browse_using_localhost(test->browse))
	{
		localhost_types = db_browse_channel_get_browse_types_on_localhost(channel);
		printf(" localhost_types=%s", db_test_browse_types_to_string(localhost_types, temp, sizeof(temp)));
	}
	if (all_types)
	{
		printf(" canonical_name=\"%s\"", canonical_name);
	}
	printf(" format=%s\n", db_test_print_formats(formats, temp, sizeof(temp)));

}


static void
db_test_print_label
(
	db_browse_test_t * test,
	const db_browse_label_t * label
)  {
	char temp[64];
	const char * name = db_browse_label_get_name(label);
	const char * device_name = db_browse_device_get_name(db_browse_channel_get_device(db_browse_label_get_channel(label)));

	db_browse_types_t all_types, network_types[DB_BROWSE_MAX_INTERFACE_INDEXES], localhost_types;
	unsigned int n, nn = db_browse_num_interface_indexes(test->browse);
		
	printf("name=\"%s\" device=\"%s\"", name, device_name);
	all_types = db_browse_label_get_browse_types(label);
	printf(" all_types=%s", db_test_browse_types_to_string(all_types, temp, sizeof(temp)));
	for (n = 0; n < nn; n++)
	{
		network_types[n] = db_browse_label_get_browse_types_on_network(label, n);
		printf(" network_types[%d]=%s", n, db_test_browse_types_to_string(network_types[n], temp, sizeof(temp)));
	}
	if (db_browse_using_localhost(test->browse))
	{
		localhost_types = db_browse_label_get_browse_types_on_localhost(label);
		printf(" localhost_types=%s", db_test_browse_types_to_string(localhost_types, temp, sizeof(temp)));
	}
}

static void
db_test_print_node_change
(
	db_browse_test_t * test,
	const db_node_t * node,
	db_node_change_t node_change
) {
	printf("%s NODE %s: ", db_test_node_type_to_string(node->type), db_test_node_change_to_string(node_change));
	switch (node->type)
	{
	case DB_NODE_TYPE_DEVICE:	
		db_test_print_device(test, node->_.device);
		break;

	case DB_NODE_TYPE_CHANNEL:
		db_test_print_channel(test, node->_.channel);
		break;

	case DB_NODE_TYPE_LABEL:
		db_test_print_label(test, node->_.label);
		break;
	}
	printf("\n");

	fflush(stdout);
}

static void
db_test_print_network
(
	db_browse_test_t * test
) {
	unsigned int nd, d, nc, c, nl, l;
	const db_browse_network_t * network = db_browse_get_network(test->browse);

	printf("NETWORK:\n");
	nd = db_browse_network_get_num_devices(network);
	for (d = 0; d < nd; d++)
	{
		const db_browse_device_t * device = db_browse_network_device_at_index(network, d);
		printf("  ");
		db_test_print_device(test, device);
		printf("\n");
	
		nc = db_browse_device_get_num_channels(device);
		for (c = 0; c < nc; c++)
		{
			const db_browse_channel_t * channel = db_browse_device_channel_at_index(device, c);
			printf("    ");
			db_test_print_channel(test, channel);
			printf("\n");
		
			nl = db_browse_channel_get_num_labels(channel);
			for (l = 0; l < nl; l++)
			{
				const db_browse_label_t * label = db_browse_channel_label_at_index(channel, l);

				printf("      ");
				db_test_print_label(test, label);
				printf("\n");
			}
		}
	}
}

static void
db_test_print_sockets
(
	dante_sockets_t * sockets
) {
	int i;
	printf("SOCKETS: ");
	printf("  n=%d: ", sockets->n);
#ifdef WIN32
	for (i = 0; i < sockets->n; i++)
	{
		printf(" %d", sockets->read_fds.fd_array[i]);
	}
#else
	for (i = 0; i < sockets->n; i++)
	{
		if (FD_ISSET(i, &sockets->read_fds))
		{
			printf(" %d", i);
		}
	}
#endif
	printf("\n");
}

//----------------------------------------------------------
// Callbacks
//----------------------------------------------------------

void
db_test_node_changed
(
	db_browse_t * browse,
	const db_node_t * node,
	db_node_change_t node_change
) {
	db_browse_test_t * test = (db_browse_test_t *) db_browse_get_context(browse);
	if (test->print_node_changes)
	{
		db_test_print_node_change(test, node, node_change);
	}
}

void
db_test_network_changed
(
	const db_browse_t * browse
) {
	db_browse_test_t * test = (db_browse_test_t *) db_browse_get_context(browse);
	test->network_changed = AUD_TRUE;
}

void
db_test_sockets_changed
(
	const db_browse_t * browse
) {
	db_browse_test_t * test = (db_browse_test_t *) db_browse_get_context(browse);
	test->sockets_changed = AUD_TRUE;
}


//----------------------------------------------------------
// Main functionality
//----------------------------------------------------------

static aud_error_t 
db_browse_test_process_line(db_browse_test_t * test, char * buf)
{
	aud_error_t result;
	char in_action, in_type;
	char in_name[64];
	if (sscanf(buf, "%c %c %s", &in_action, &in_type, in_name) == 3 && in_action == 'r' && in_type == 'd')
	{
		const db_browse_network_t * network = db_browse_get_network(test->browse);
		db_browse_device_t * device = db_browse_network_device_with_name(network, in_name);
		if (!device)
		{
			printf("Unknown device '%s'\n", in_name);
			return AUD_SUCCESS;
		}
		result = db_browse_device_reconfirm(device, 0, AUD_FALSE);
		if (result != AUD_SUCCESS)
		{
			printf("Error reconfirming device '%s': %s\n", in_name, aud_error_message(result, test->errbuf));
			//return result;
		}
		printf("Reconfirming device '%s'\n", in_name);
	}
	else if (sscanf(buf, "%c %c", &in_action, &in_type) == 2 && in_action == 'r' && in_type == 'd')
	{
		unsigned int i;
		const db_browse_network_t * network = db_browse_get_network(test->browse);
		for (i = 0; i < db_browse_network_get_num_devices(network); i++)
		{
			db_browse_device_t * device = db_browse_network_device_at_index(network, i);
			const char * name = db_browse_device_get_name(device);
			result = db_browse_device_reconfirm(device, 0, AUD_FALSE);
			if (result != AUD_SUCCESS)
			{
				printf("Error reconfirming device '%s': %s\n", name, aud_error_message(result, test->errbuf));
				//return result;
			}
			printf("Reconfirming device '%s'\n", name);
		}
	}
	else
	{
		printf("Unknown command '%s'\n", buf);
	}
	return AUD_SUCCESS;
}

static aud_error_t
db_browse_test_main_loop
(
	db_browse_test_t * test
) {
	aud_utime_t select_timeout;
	aud_utime_t next_resolve_timeout;
	aud_error_t result;
	//dante_sockets_t all_sockets;
	aud_bool_t print_prompt = AUD_TRUE;
	
#ifdef  _WIN32
	// set to line buffered mode.
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT);
#endif

	dante_sockets_clear(&test->sockets);
	result = db_browse_get_sockets(test->browse, &test->sockets);
	if (result != AUD_SUCCESS)
	{
		printf("Error getting sockets: %s\n", aud_error_message(result, test->errbuf));
		return result;
	}
#ifndef _WIN32
	dante_sockets_add_read(&test->sockets, 0); // 0 is always stdin
#endif

	printf("Running main loop\n");

	select_timeout.tv_sec = 1;
	select_timeout.tv_usec = 0;
	next_resolve_timeout.tv_sec = 0;
	next_resolve_timeout.tv_usec = 0;

	while(g_running)
	{
		int select_result;
		dante_sockets_t curr_sockets;
		aud_bool_t processing_needed;
		char buf[BUFSIZ];

		// print prompt if needed
		if (print_prompt)
		{
			printf("\n> ");
			fflush(stdout);
			print_prompt = AUD_FALSE;
		}

		memcpy(&curr_sockets, &test->sockets, sizeof(dante_sockets_t));

		select_result = select(curr_sockets.n, &curr_sockets.read_fds, NULL, NULL, &select_timeout);
		if (select_result < 0)
		{
			result = aud_error_get_last();
			printf("Error select()ing: %s\n", aud_error_message(result, test->errbuf));
			return result;
		}

		processing_needed = AUD_FALSE;
		if (select_result > 0)
		{
			processing_needed = AUD_TRUE;
		}
		else if (next_resolve_timeout.tv_sec || next_resolve_timeout.tv_usec)
		{
			aud_utime_t now;
			aud_utime_get(&now);
			if (aud_utime_compare(&next_resolve_timeout, &now) < 0)
			{
				processing_needed = AUD_TRUE;
			}
		}


		if (processing_needed)
		{
			result = db_browse_process(test->browse, &curr_sockets, &next_resolve_timeout);
			if (result != AUD_SUCCESS)
			{
				printf("Error processing browse: %s\n", aud_error_message(result, test->errbuf));
				return result;
			}
			if (test->network_changed)
			{
				test->network_changed = AUD_FALSE;
				if (test->print_network_changes)
				{
					db_test_print_network(test);
				}
			}
			if (test->sockets_changed)
			{
				test->sockets_changed = AUD_FALSE;

				dante_sockets_clear(&test->sockets);
				result = db_browse_get_sockets(test->browse, &test->sockets);
				if (result != AUD_SUCCESS)
				{
					printf("Error getting sockets: %s\n", aud_error_message(result, test->errbuf));
					return result;
				}
				if (test->print_socket_changes)
				{
					db_test_print_sockets(&test->sockets);
				}
#ifndef _WIN32
				dante_sockets_add_read(&test->sockets, 0); // 0 is always stdin
#endif
			}
		}
		// and check stdin 
		buf[0] = '\0';
#ifdef _WIN32
		if (_kbhit())
		{
			DWORD len = 0;
			if (!ReadConsoleA(GetStdHandle(STD_INPUT_HANDLE),buf,BUFSIZ-1,&len, 0))
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
		if (FD_ISSET(0, &curr_sockets.read_fds)) // 0 is always stdin
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
					printf("Exiting with %s\n", dr_error_message(result, test->errbuf));
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
			result = db_browse_test_process_line(test, buf);
			if (result != AUD_SUCCESS)
			{
				break;
			}
		}
	}
	return AUD_SUCCESS;
}

static void usage(void)
{
	printf("OPTIONS:\n");
	printf("  -i print incremental (node) changes\n");
	printf("  -n print network changes\n");
	printf("  -s print socket changes\n");
	printf("  -media browse for media devices\n");
	printf("  -conmon browse for conmon devices\n");
	printf("  -safe browse for safe-mode devices\n");
	printf("  -channels browse for media channels\n");
	printf("  -ii=INDEX add browsing network with interface INDEX\n");
	printf("  -localhost=BOOL enable / disable browsing on localhost interface\n");
	printf("  -f=_MFID filter browse by manufacturer ID (sytnax _0123abcd...)\n");
}


int main(int argc, char * argv[])
{
	db_browse_test_t test;
	aud_error_t result = AUD_SUCCESS;
	int i;
	aud_bool_t first_interface = AUD_TRUE;

	db_browse_config_t browse_config;
	db_browse_types_t types = 0;
	const char * browse_filter = NULL;

	memset(&test, 0, sizeof(db_browse_test_t));
	db_browse_config_init_defaults(&browse_config);

	for(i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-i"))
		{
			test.print_node_changes = AUD_TRUE;
		}
		else if (!strcmp(argv[i], "-n"))
		{
			test.print_network_changes = AUD_TRUE;
		}
		else if (!strcmp(argv[i], "-s"))
		{
			test.print_socket_changes = AUD_TRUE;
		}
		else if (!strcmp(argv[i], "-media"))
		{
			types |= DB_BROWSE_TYPE_MEDIA_DEVICE;
		}
		else if (!strcmp(argv[i], "-conmon"))
		{
			types |= DB_BROWSE_TYPE_CONMON_DEVICE;
		}
		else if (!strcmp(argv[i], "-safe"))
		{
			types |= DB_BROWSE_TYPE_SAFE_MODE_DEVICE;
		}
		else if (!strcmp(argv[i], "-channels"))
		{
			types |= DB_BROWSE_TYPE_MEDIA_CHANNEL;
		}
		else if (!strncmp(argv[i], "-ii=", 4))
		{
			if (first_interface)
			{
				browse_config.num_interface_indexes = 0;
				first_interface = AUD_FALSE;
			}
			if (browse_config.num_interface_indexes < DB_BROWSE_MAX_INTERFACE_INDEXES)
			{
				browse_config.interface_indexes[browse_config.num_interface_indexes] = atoi(argv[i]+4);
				browse_config.num_interface_indexes++;
			}
			else
			{
				printf("Too many interface indexes (max %d)\n", DB_BROWSE_MAX_INTERFACE_INDEXES);
				exit(0);
			}
		}
		else if (!strcmp(argv[i], "-localhost=true"))
		{
			browse_config.localhost = AUD_TRUE;
		}
		else if (!strcmp(argv[i], "-localhost=false"))
		{
			browse_config.localhost = AUD_FALSE;
		}
		else if (!strncmp(argv[i], "-f=", 3))
		{
			browse_filter = argv[i] + 3;
		}
		else
		{
			usage();
			exit(0);
		}
	}

	if (types == 0)
	{
		types = DB_BROWSE_TYPES_ALL;
	}
	
	result = aud_env_setup(&test.env);
	if (result != AUD_SUCCESS)
	{
		printf("Error initialising environment: %s\n", aud_error_message(result, test.errbuf));
		goto cleanup;
	}
	printf("Created environment\n");

	result = db_browse_new(test.env, types, &test.browse);
	if (result != AUD_SUCCESS)
	{
		printf("Error creating browse: %s\n", aud_error_message(result, test.errbuf));
		goto cleanup;
	}
	result = db_browse_set_max_sockets(test.browse, MAX_SOCKETS);
	if (result != AUD_SUCCESS)
	{
		printf("Error setting max browse sockets: %s\n", aud_error_message(result, test.errbuf));
		goto cleanup;
	}

	db_browse_set_node_changed_callback(test.browse, db_test_node_changed);
	db_browse_set_network_changed_callback(test.browse, db_test_network_changed);
	db_browse_set_sockets_changed_callback(test.browse, db_test_sockets_changed);
	db_browse_set_context(test.browse, &test);

	if (browse_filter && browse_filter[0])
	{
		dante_id64_t filter_id;
		if (dante_id64_from_dnssd_text(&filter_id, browse_filter))
		{
			db_browse_set_id64_filter(test.browse, &filter_id);
		}
	}

	result = db_browse_start_config(test.browse, &browse_config);
	if (result != AUD_SUCCESS)
	{
		printf("Error starting browse: %s\n", aud_error_message(result, test.errbuf));
		goto cleanup;
	}

	result = db_browse_test_main_loop(&test);
	printf("Finished main loop\n");

cleanup:
	if (test.browse)
	{
		db_browse_delete(test.browse);
	}
	if (test.env)
	{
		aud_env_release(test.env);
	}
	return result;
}
