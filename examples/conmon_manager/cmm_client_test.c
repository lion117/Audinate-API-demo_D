#include "audinate/dante_api.h"

#include <stdio.h>
#include <signal.h>

typedef struct
{
	aud_env_t * env;
	cmm_client_t * client;
} cmm_client_test_t;

//-------------------
// Printing
//------------------

static const char *
cmm_connection_state_to_string
(
	cmm_connection_state_t connection_state
) {
	switch (connection_state)
	{
	case CMM_CONNECTION_STATE_INITIALISED:  return "INITIALISED";
	case CMM_CONNECTION_STATE_CONNECTING:   return "CONNECTING";
	case CMM_CONNECTION_STATE_CONNECTED:    return "CONNECTED";
	default: return "?";
	}
}

static void
cmm_client_test_print_info
(
	const cmm_info_t * info
) {
	if (info)
	{
		dante_version_t version;
		cmm_info_get_version(info, &version);
		printf("Version: %u.%u.%u\n", version.major, version.minor, version.bugfix);
		printf("Max Dante Networks: %u\n", cmm_info_max_dante_networks(info));
	}
}

static void
cmm_client_test_print_options
(
	const cmm_options_t * options
) {
	uint16_t i;
	printf("Interfaces:\n");
	if (options)
	{
		for (i = 0; i < cmm_options_num_interfaces(options); i++)
		{
			const cmm_interface_t * iface = 
				cmm_options_interface_at_index(options, i);
			const uint8_t * mac = cmm_interface_get_mac_address(iface);
			printf("  %ls(%02x:%02x:%02x:%02x:%02x:%02x)\n",
				cmm_interface_get_name(iface),
				mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
	}
}

const char * CMM_CLIENT_TEST_INTERFACE_NAMES[2] =
{
	"Primary",
	"Secondary"
};

static void
cmm_client_test_print_config
(
	const cmm_config_t * config
) {
	uint16_t i; 
	if (config != NULL)
	{
		for (i = 0; i < cmm_config_num_interfaces(config); i++)
		{
			const cmm_interface_t * iface = cmm_config_interface_at_index(config, i);
			const uint8_t * mac = cmm_interface_get_mac_address(iface);
			printf("  %ls(%02x:%02x:%02x:%02x:%02x:%02x)\n",
				cmm_interface_get_name(iface),
				mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
	}
}

static void
cmm_client_test_print_process
(
	const cmm_process_t * process
) {
	printf("  %s: %d (%s)\n",
		cmm_process_get_name(process), cmm_process_get_id(process),
		(cmm_process_is_locking_configuration(process) ? "true" : "false"));
}


static void
cmm_client_test_print_state
(
	const cmm_state_t * state
) {
	if (state)
	{
		cmm_system_status_t system_status = cmm_state_get_system_status(state);
		uint16_t i;

		printf("System status: %s (%04x)\n",
			cmm_system_status_to_string(system_status), system_status);
		printf("Dante device name: %s\n", cmm_state_get_dante_device_name(state));
		printf("Current config:\n");
		cmm_client_test_print_config(cmm_state_get_current_config(state));
		printf("Pending config:\n");
		cmm_client_test_print_config(cmm_state_get_pending_config(state));
		printf("Processes:\n");
		for (i = 0; i < cmm_state_num_processes(state); i++)
		{
			const cmm_process_t * process = cmm_state_process_at_index(state, i);
			cmm_client_test_print_process(process);
		}
	}
}

//----------------------------------------------------------
// Callback event handlers
//----------------------------------------------------------


aud_bool_t running = AUD_TRUE;

void signal_handler(int sig);

void signal_handler(int sig)
{
	AUD_UNUSED(sig);
	signal(SIGINT, signal_handler);
	running = AUD_FALSE;
}

cmm_client_event_fn cmm_client_test_event;

//----------------------------------------------------------
// Callback event handlers
//----------------------------------------------------------

static void
cmm_client_test_on_connection_changed
(
	cmm_client_t * client
) {
	cmm_connection_state_t connection = cmm_client_get_connection_state(client);
	printf("Client connection state is now %s\n",
		cmm_connection_state_to_string(connection));
	if (connection == CMM_CONNECTION_STATE_CONNECTED)
	{
		printf("Info is:\n");
		cmm_client_test_print_info(cmm_client_get_system_info(client));
	}
}

static void
cmm_client_test_on_options_changed
(
	cmm_client_t * client
) {
	printf("Options have changed, now:\n");
	cmm_client_test_print_options(cmm_client_get_system_options(client));
}

static void
cmm_client_test_on_state_changed
(
	cmm_client_t * client
) {
	printf("State has changed, now:\n");
	cmm_client_test_print_state(cmm_client_get_system_state(client));
}

static void
cmm_client_test_on_process_changed
(
	cmm_client_t * client
) {
	printf("Process has changed, now:\n");
	cmm_client_test_print_process(cmm_client_get_process(client));
}

static void
cmm_client_test_on_request_completed
(
	cmm_client_t * client
) {
	(void) client;

	printf("Request completed\n");
}

void
cmm_client_test_event
(
	cmm_client_t * client,
	const cmm_client_event_info_t * event_info
) {
	//cmm_client_test_t * test = (cmm_client_test_t *) cmm_client_get_context(client);

	printf("Got an event, flags=0x%08x\n", event_info->flags);
	
	if (event_info->flags & CMM_CLIENT_EVENT_FLAG_CONNECTION_CHANGED)
	{
		cmm_client_test_on_connection_changed(client);
	}

	if (event_info->flags & CMM_CLIENT_EVENT_FLAG_OPTIONS_CHANGED)
	{
		cmm_client_test_on_options_changed(client);
	}

	if (event_info->flags & CMM_CLIENT_EVENT_FLAG_STATE_CHANGED)
	{
		cmm_client_test_on_state_changed(client);
	}

	if (event_info->flags & CMM_CLIENT_EVENT_FLAG_PROCESS_CHANGED)
	{
		cmm_client_test_on_process_changed(client);
	}

	if (event_info->flags & CMM_CLIENT_EVENT_FLAG_REQUEST_COMPLETED)
	{
		cmm_client_test_on_request_completed(client);
	}

}

//----------------------------------------------------------
// Main functions
//----------------------------------------------------------

static aud_error_t
cmm_client_test_run
(
	cmm_client_test_t * test
) {
	aud_error_t result;
	aud_socket_t s = cmm_client_get_socket(test->client);
	aud_utime_t timeout = {1, 0};
	fd_set read_fds;
	int select_result;

	FD_ZERO(&read_fds);

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4127)
	FD_SET(s, &read_fds);
#pragma warning(pop)
	select_result = select(1, &read_fds, NULL, NULL, &timeout);
#else
	FD_SET(s, &read_fds);
	select_result = select(s+1, &read_fds, NULL, NULL, &timeout);
#endif
	if (select_result < 0)
	{
		result = aud_system_error_get_last();
		
		//aud_log(test->env->log, AUD_LOG_ERROR, "Select error: %s\n", aud_error_message(result, errbuf)); 
		return result;
	}
	else if (select_result >= 0)
	{
		result = cmm_client_process(test->client, NULL);
		if (result != AUD_SUCCESS)
		{
			//aud_log(test->env->log, AUD_LOG_ERROR, "Error processing dvs client: %s\n", aud_error_message(result, errbuf)); 
			return result;
		}
	}
	return AUD_SUCCESS;
}

static aud_error_t
cmm_test_set_interface(cmm_client_test_t * test, const wchar_t * name)
{
	aud_error_t result;
	aud_errbuf_t errbuf;

	cmm_config_t * config;
	const cmm_options_t * options = cmm_client_get_system_options(test->client);
	const cmm_interface_t * iface;

	config = cmm_client_get_temp_config(test->client);
	cmm_config_reset(config);
	iface = cmm_options_interface_with_name(options, name);
	if (iface == NULL)
	{
		printf("Unknown interface!\n");
		return AUD_ERR_INVALIDDATA;
	}
	cmm_config_add_interface(config,
		cmm_client_get_system_options(test->client),
		cmm_interface_get_mac_address(iface));
	{
		cmm_system_config_t system_config = {0};
		system_config.flags = CMM_SYSTEM_CONFIG_FLAG_CONFIG;
		system_config.config = config;
		system_config.lock = AUD_FALSE;
		result = cmm_client_set_system_config(test->client, &system_config);
	}
	if (result != AUD_SUCCESS)
	{
		printf("Error sending config: %s\n", aud_error_message(result, errbuf));
		return result;
	}
	while (cmm_client_has_active_request(test->client))
	{
		if (!running)
		{
			return AUD_ERR_INTERRUPTED;
		}
		cmm_client_test_run(test);
	}
	if (cmm_state_get_pending_config(cmm_client_get_system_state(test->client)))
	{
		printf("Sent config, waiting for new configuration to be applied\n");
		while (cmm_state_get_pending_config(cmm_client_get_system_state(test->client)))
		{
			if (!running)
			{
				return AUD_ERR_INTERRUPTED;
			}
			cmm_client_test_run(test);
		}
		printf("New configuration has been applied\n");
	}
	else
	{
		printf("Sent config, no changes\n");
	}
	return AUD_SUCCESS;
}

static void usage(char * bin)
{
	printf("Usage: %s OPTIONS\n", bin);
	printf("  -l         connect to the server and listen for events\n");
	printf("  -i=NAME    switch to the interface with name NAME\n");
}

typedef enum
{
	CMM_CLIENT_TEST_MODE_NONE,
	CMM_CLIENT_TEST_MODE_LISTEN,
	CMM_CLIENT_TEST_MODE_NAME
} cmm_client_test_mode_t;


int main(int argc, char * argv[])
{
	cmm_client_test_mode_t mode = CMM_CLIENT_TEST_MODE_NONE;
	wchar_t name[CMM_INTERFACE_NAME_LENGTH];
	int a;
	uint16_t port = 0;

	aud_error_t result;
	aud_errbuf_t errbuf;

	cmm_client_test_t test;

	for (a = 1; a < argc; a++)
	{
		const char * arg = argv[a];
		if (!strncmp(arg, "-l", 2))
		{
			mode = CMM_CLIENT_TEST_MODE_LISTEN;
		}
		else if (!strncmp(arg, "-p=", 3) && strlen(arg) > 3)
		{
			port = (uint16_t) atoi(arg+3);
		}
		else if (!strncmp(arg, "-i=", 3) && strlen(arg) > 3)
		{
			int i;
			const char * in = arg + 3;
			for (i = 0; i < CMM_INTERFACE_NAME_LENGTH-1 && *in; i++)
			{
				name[i] = in[i];
			}
			name[i] = '\0';
			mode = CMM_CLIENT_TEST_MODE_NAME;
		}
		else
		{
			usage(argv[0]);
			exit(0);
		}
	}

	result = aud_env_setup(&test.env);
	if (result != AUD_SUCCESS)
	{
		fprintf(stderr, "Error creating env: %s\n", aud_error_message(result, errbuf));
		return result;
	}
	test.client = cmm_client_new(test.env);
	if (test.client == NULL)
	{
		fprintf(stderr, "Error creating client: NO_MEMORY\n");
		goto cleanup;
	}
	result = cmm_client_set_server_port(test.client, port);
	if (result != AUD_SUCCESS)
	{
		fprintf(stderr, "Error setting client's server port: %s\n", aud_error_message(result, errbuf));
		goto cleanup;
	}

	result = cmm_client_init(test.client, "test");
	if (result != AUD_SUCCESS)
	{
		fprintf(stderr, "Error initialising client: %s\n", aud_error_message(result, errbuf));
		goto cleanup;
	}
	cmm_client_set_context(test.client, &test);
	cmm_client_set_event_cb(test.client, &cmm_client_test_event);
	result = cmm_client_connect(test.client);
	if (result != AUD_SUCCESS)
	{
		fprintf(stderr, "Error connecting client: %s\n", aud_error_message(result, errbuf)); 
		goto cleanup;
	}

	signal(SIGINT, signal_handler);

	printf("Waiting for connection\n");
	while (cmm_client_get_connection_state(test.client) != CMM_CONNECTION_STATE_CONNECTED)
	{
		if (!running)
		{
			goto cleanup;
		}
		cmm_client_test_run(&test);
	}
	printf("Waiting for connected\n");

	while (cmm_client_get_system_options(test.client) == NULL || cmm_client_get_system_state(test.client) == NULL)
	{
		if (!running)
		{
			goto cleanup;
		}
		cmm_client_test_run(&test);
	}
	printf("Acquired options and state\n");

	if (mode == CMM_CLIENT_TEST_MODE_NAME)
	{
		cmm_test_set_interface(&test, name);
	}
	else if (mode == CMM_CLIENT_TEST_MODE_LISTEN)
	{
		while (running)
		{
			cmm_client_test_run(&test);
		}
	}

cleanup:

	if (test.client)
	{
		cmm_client_disconnect(test.client);
		cmm_client_terminate(test.client);
		cmm_client_delete(test.client);
	}
	aud_env_release(test.env);
	return 0;
}
