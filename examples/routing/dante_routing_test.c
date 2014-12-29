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
#include "dante_routing_test.h"
#include <signal.h>
#ifdef _WIN32
#include <conio.h>
#endif

#define DR_TEST_REQUEST_DESCRIPTION_LENGTH 64
#define DR_TEST_MAX_REQUESTS 128


static aud_bool_t g_test_running = AUD_TRUE;

/*
static void sig_handler(int sig)
{
	AUD_UNUSED(sig);
	signal(SIGINT, sig_handler);
	g_test_running = AUD_FALSE;
}
*/

typedef struct dr_test_options
{
	unsigned int num_handles;
	unsigned int request_limit;

	// for name-based connection
	uint16_t local_port;
	const char * device_name;

	// for address-based connection
	unsigned int num_addresses;
	unsigned int addresses[DR_TEST_MAX_INTERFACES];

	// for interface-aware / redundant name-based connection
	unsigned int num_local_interfaces;
	aud_interface_identifier_t local_interfaces[DR_TEST_MAX_INTERFACES];

	aud_bool_t automatic_update_on_state_change;
} dr_test_options_t;

typedef struct dr_test_request
{
	dante_request_id_t id;
	char description[DR_TEST_REQUEST_DESCRIPTION_LENGTH];
} dr_test_request_t;

typedef struct
{
	//dante_request_id_t request_id;
	//aud_error_t last_result;
	aud_bool_t sockets_changed;
} dr_test_async_info_t;

typedef struct
{
	dr_test_options_t * options;

	aud_env_t * env;
	dr_devices_t * devices;

	dr_device_t * device;
	uint16_t nintf;
	uint16_t ntx;
	uint16_t nrx;
	uint16_t max_txflow_slots;
	uint16_t max_rxflow_slots;

	dr_txchannel_t ** tx;
	dr_rxchannel_t ** rx;

	uint16_t txlabels_buflen;
	dr_txlabel_t * txlabels_buf;

	dr_test_request_t requests[DR_TEST_MAX_REQUESTS];

	dr_test_async_info_t async_info;
} dr_test_t;


// Static buffers: save  stack memory by sharing these buffers
aud_errbuf_t g_test_errbuf;
char g_input_buf[BUFSIZ];

// callback functions
static dr_devices_sockets_changed_fn dr_test_on_sockets_changed;
static dr_device_changed_fn dr_test_on_device_changed;
static dr_device_response_fn dr_test_on_response;

//----------------------------------------------------------
// Request management
//----------------------------------------------------------

static dr_test_request_t *
dr_test_allocate_request
(
	dr_test_t * test,
	const char * description
) {
	unsigned int i;
	for (i = 0; i < DR_TEST_MAX_REQUESTS; i++)
	{
		if (test->requests[i].id == DANTE_NULL_REQUEST_ID)
		{
			aud_strlcpy(test->requests[i].description, description ? description : "", DR_TEST_REQUEST_DESCRIPTION_LENGTH);
			return test->requests + i;
		}
	}
	DR_TEST_ERROR("error allocating request '%s': no more requests\n", description);
	return NULL;
}

static void
dr_test_request_release
(
	dr_test_request_t * request
) {
	request->id = DANTE_NULL_REQUEST_ID;
	request->description[0] = '\0';
}

void
dr_test_on_response
(
	dr_device_t * device,
	dante_request_id_t request_id,
	aud_error_t result
) {
	unsigned int i;
	dr_test_t * test = (dr_test_t *)  dr_device_get_context(device);

	for (i = 0; i < DR_TEST_MAX_REQUESTS; i++)
	{
		if (test->requests[i].id == request_id)
		{
			DR_TEST_PRINT("\nEVENT: completed request %p (%s) with result %s\n", 
				request_id, test->requests[i].description, dr_error_message(result, g_test_errbuf));
			dr_test_request_release(test->requests+i);
			return;
		}
	}
	DR_TEST_ERROR("\nEVENT: completed unknown request %p\n", request_id);
}

//----------------------------------------------------------
// State management and basic functionality
//----------------------------------------------------------

static aud_error_t
dr_test_query_capabilities
(
	dr_test_t * test
) {
	aud_error_t result;
	dr_test_request_t * request = dr_test_allocate_request(test, "QueryCapabilities");
	if (!request)
	{
		return AUD_ERR_NOBUFS;
	}

	result = dr_device_query_capabilities(test->device, &dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending query capabilities: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return result;
	}
	return result;
}

static void
dr_test_mark_component_stale
(
	dr_test_t * test,
	dr_device_component_t component
) {
	if (component == DR_DEVICE_COMPONENT_COUNT)
	{
		DR_TEST_DEBUG("Invalidating ALL components\n");
		for (component = 0; component < DR_DEVICE_COMPONENT_COUNT; component++)
		{
			dr_device_mark_component_stale(test->device, component);
		}
	}
	else
	{
		DR_TEST_DEBUG("Invalidating component %s\n", dr_device_component_to_string(component));
		dr_device_mark_component_stale(test->device, component);
	}
}

static aud_error_t
dr_test_update
(
	dr_test_t * test
) {
	aud_error_t result;
	dr_device_component_t c;
	for (c = 0; c < DR_DEVICE_COMPONENT_COUNT; c++)
	{
		dr_test_request_t * request;
		if (!dr_device_is_component_stale(test->device, c))
		{
			continue;
		}
		DR_TEST_DEBUG("Updating stale component %s\n", dr_device_component_to_string(c));

		request = dr_test_allocate_request(test, NULL);
		if (!request)
		{
			return AUD_ERR_NOBUFS;
		}
		SNPRINTF(request->description, DR_TEST_REQUEST_DESCRIPTION_LENGTH, "Update %s", dr_device_component_to_string(c));

		result = dr_device_update_component(test->device, &dr_test_on_response, &request->id, c);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error sending update %s: %s\n",
				dr_device_component_to_string(c), dr_error_message(result, g_test_errbuf));
			dr_test_request_release(request);
			return result;
		}
	}
	return AUD_SUCCESS;
}


static aud_error_t
dr_test_update_rxflow_errors
(
	dr_test_t * test, 
	dante_rxflow_error_type_t field_type,
	aud_bool_t clear
) {
	aud_error_t result;
	dr_test_request_t * request;
	if (field_type == DANTE_NUM_RXFLOW_ERROR_TYPES)
	{
		DR_TEST_DEBUG("Updating rxflow error flags\n");
		request = dr_test_allocate_request(test, NULL);
		if (!request)
		{
			return AUD_ERR_NOBUFS;
		}
		SNPRINTF(request->description, DR_TEST_REQUEST_DESCRIPTION_LENGTH, "Update rxflow error flags");
		result = dr_device_update_rxflow_error_flags(test->device, &dr_test_on_response, &request->id, clear);
	}
	else
	{
		DR_TEST_DEBUG("Updating rxflow error field %s\n", dante_rxflow_error_type_to_string(field_type));
			request = dr_test_allocate_request(test, NULL);
		if (!request)
		{
			return AUD_ERR_NOBUFS;
		}
		SNPRINTF(request->description, DR_TEST_REQUEST_DESCRIPTION_LENGTH, "Update rx flow %s", dante_rxflow_error_type_to_string(field_type));
		result = dr_device_update_rxflow_error_fields(test->device, &dr_test_on_response, &request->id, field_type, clear);
	}
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error update rxflow errors : %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return result;
	}
	return AUD_SUCCESS;
}

static void
dr_test_on_device_state_changed
(
	dr_test_t * test
) {
	switch (dr_device_get_state(test->device))
	{
	case DR_DEVICE_STATE_RESOLVED:
		// query capabilities
		if (test->options->automatic_update_on_state_change)
		{
			DR_TEST_PRINT("device resolved, querying capabilities\n");
			dr_test_query_capabilities(test);
		}
		return;

	case DR_DEVICE_STATE_ACTIVE:
		{
			// update information
			dr_device_get_txchannels(test->device, &test->ntx, &test->tx);
			dr_device_get_rxchannels(test->device, &test->nrx, &test->rx);

			// update all components when we enter the active state, unless we have a strange status
			if (test->options->automatic_update_on_state_change)
			{
				dr_device_status_flags_t status_flags;
				aud_error_t result = dr_device_get_status_flags(test->device, &status_flags);
				if (result != AUD_SUCCESS)
				{
					DR_TEST_ERROR("Error getting status flags: %s\n", dr_error_message(result, g_test_errbuf));
					return;
				}
				if (status_flags)
				{
					DR_TEST_PRINT("device active, status=0x%08x, not updating\n", status_flags);
					return;
				}
				DR_TEST_PRINT("device active, updating\n");
				dr_test_update(test);
			}
			return;
		}
	case DR_DEVICE_STATE_ERROR:
		{
			aud_error_t error_state_error = dr_device_get_error_state_error(test->device);
			const char * error_state_action = dr_device_get_error_state_action(test->device);
	
			DR_TEST_PRINT("device has entered the ERROR state: error=%s, action='%s'\n",
				dr_error_message(error_state_error, g_test_errbuf), (error_state_action ? error_state_action : ""));
			return;
		}
	default:
		// nothing to do
		DR_TEST_PRINT("device state is now '%s'\n", dr_device_state_to_string(dr_device_get_state(test->device)));
	}
}


static void
dr_test_on_device_addresses_changed
(
	dr_test_t * test
) {
	unsigned int i, na = 2;
	dante_ipv4_address_t addresses[2];
	dr_device_get_addresses(test->device, &na, addresses);

	DR_TEST_PRINT("Addresses are now: ");
	for (i = 0; i < na; i++)
	{
		uint8_t * host = (uint8_t *) &(addresses[i].host);
		DR_TEST_PRINT("%c%u.%u.%u.%u:%u", 
			(i ? ',' : '['),
			host[0], host[1], host[2], host[3], addresses[i].port);
	}
	DR_TEST_PRINT("]\n");
}

//----------------------------------------------------------
// Connect
//----------------------------------------------------------

static aud_error_t
dr_test_open
(
	dr_test_t * test,
	const dr_test_options_t * options
) {
	aud_error_t result;
	dr_device_open_t * config = NULL;
	unsigned i;

	if (options->device_name)
	{
		config = dr_device_open_config_new(options->device_name);
		if (! config)
		{
			result = AUD_ERR_NOMEMORY;
			goto l__error;
		}

		if (options->num_local_interfaces)
		{
			DR_TEST_PRINT("Opening connection to remote device %s using %d interfaces\n", 
				options->device_name, options->num_local_interfaces);

			//result = dr_device_open_remote_on_interfaces(test->devices, 
			//	options->device_name, options->num_local_interface_indexes, options->local_interface_indexes, &test->device);

			for (i = 0; i < options->num_local_interfaces; i++)
			{
				if (options->local_interfaces[i].flags == AUD_INTERFACE_IDENTIFIER_FLAG_NAME)
				{
					dr_device_open_config_enable_interface_by_name(
						config, i, options->local_interfaces[i].name
					);
				}
				else if (options->local_interfaces[i].flags == AUD_INTERFACE_IDENTIFIER_FLAG_INDEX)
				{
					dr_device_open_config_enable_interface_by_index(
						config, i, options->local_interfaces[i].index
					);
				}
			}
		}
		else
		{
			DR_TEST_PRINT("Opening connection to remote device %s\n", options->device_name);
			//result = dr_device_open_remote(test->devices, options->device_name, &test->device);
		}
		result = dr_device_open_with_config(test->devices, config, &test->device);

		dr_device_open_config_free(config);
		config = NULL;
	}
	else if (options->num_addresses)
	{
		config = dr_device_open_config_new(NULL);
		if (! config)
		{
			result = AUD_ERR_NOMEMORY;
			goto l__error;
		}

		DR_TEST_PRINT("Opening connection using %d explicit addresses\n", 
			options->num_addresses);
		//result = dr_device_open_addresses(test->devices, 
		//	options->num_addresses, options->addresses, &test->device);

		for (i = 0; i < options->num_addresses; i++)
		{
			if (options->addresses[i])
			{
				dr_device_open_config_enable_address(
					config, i, options->addresses[i], 0
				);
			}
		}
		result = dr_device_open_with_config(test->devices, config, &test->device);

		dr_device_open_config_free(config);
		config = NULL;
	}
	else
	{
		if (options->local_port)
		{
			DR_TEST_PRINT("Opening connection to local device on port %d\n", options->local_port);
			result = dr_device_open_local_on_port(test->devices, options->local_port, &test->device);
		}
		else
		{
			DR_TEST_PRINT("Opening connection to local device\n");
			result = dr_device_open_local(test->devices, &test->device);
		}
	}
	if (result != AUD_SUCCESS)
	{
	l__error:
		DR_TEST_ERROR("Error creating device: %s\n",
			dr_error_message(result, g_test_errbuf));
		return result;
	}

	dr_device_set_context(test->device, test);
	dr_device_set_changed_callback(test->device, dr_test_on_device_changed);

	// if we are connecting locally we may need to trigger the next state transition,
	// if not this is a no-op
	dr_test_on_device_state_changed(test);

	return AUD_SUCCESS;
}

//----------------------------------------------------------
// Device-level configuration and settings
//----------------------------------------------------------

static void
dr_test_ping
(
	dr_test_t * test
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Ping device\n");

	request = dr_test_allocate_request(test, "Ping");
	if (!request)
	{
		return;
	}
	result = dr_device_ping(test->device, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending ping: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_store_config
(
	dr_test_t * test
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: store config\n");

	request = dr_test_allocate_request(test, "StoreConfig");
	if (!request)
	{
		return;
	}

	result = dr_device_store_config(test->device, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending store config: %s\n",
			dr_error_message(result, g_test_errbuf));
		return;
	}
}

static void
dr_test_clear_config(dr_test_t * test)
{
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Clear config\n");

	request = dr_test_allocate_request(test, "ClearConfig");
	if (!request)
	{
		return;
	}

	result = dr_device_clear_config(test->device, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending clear config: %s\n",
			dr_error_message(result, g_test_errbuf));
		return;
	}
}

static aud_error_t
dr_test_rename
(
	dr_test_t * test,
	const char * new_name
) {
	aud_error_t result;

	DR_TEST_DEBUG("ACTION: rename\n");

	result = dr_device_close_rename(test->device, new_name);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error renaming device to '%s': %s\n",
			(new_name ? new_name : ""), dr_error_message(result, g_test_errbuf));
		return result;
	}
	DR_TEST_PRINT("Device renamed, handle closed, exiting\n");
	test->device = NULL;
	return AUD_SUCCESS;
}

typedef enum
{
	DR_TEST_PERFORMANCE_TX,
	DR_TEST_PERFORMANCE_RX,
	DR_TEST_PERFORMANCE_UNICAST
} dr_test_performance_t;

const char * DR_TEST_PERFORMANCE_NAMES[] =
{
	"tx",
	"rx",
	"unicast"
};

static void
dr_test_set_performance
(
	dr_test_t * test,
	dr_test_performance_t performance,
	dante_latency_us_t latency_us,
	dante_fpp_t fpp
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("Setting %s performance\n", DR_TEST_PERFORMANCE_NAMES[performance]);

	request = dr_test_allocate_request(test, DR_TEST_PERFORMANCE_NAMES[performance]);
	if (!request)
	{
		return;
	}
	
	if (performance == DR_TEST_PERFORMANCE_TX)
	{
		result = dr_device_set_tx_performance_us(test->device,
			latency_us, fpp, dr_test_on_response, & request->id);
	}
	else if (performance == DR_TEST_PERFORMANCE_RX)
	{
		result = dr_device_set_rx_performance_us(test->device, 
			latency_us, fpp, dr_test_on_response, & request->id);
	}
	else
	{
		result = dr_device_set_unicast_performance_us(test->device,
			latency_us, fpp, dr_test_on_response, & request->id);
	}

	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error setting %s performance properties: %s\n",
			DR_TEST_PERFORMANCE_NAMES[performance], dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_set_lockdown
(
	dr_test_t * test,
	aud_bool_t lockdown
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Set lockdown\n");

	request = dr_test_allocate_request(test, "SetLockdown");
	if (!request)
	{
		return;
	}
	
	result = dr_device_set_lockdown(test->device,
		lockdown, dr_test_on_response, & request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error setting device lockdown: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}


static void
dr_test_set_network_loopback
(
	dr_test_t * test,
	aud_bool_t enabled
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Set Network Loopback\n");

	request = dr_test_allocate_request(test, "SetNetworkLoopback");
	if (!request)
	{
		return;
	}
	
	result = dr_device_set_network_loopback(test->device, enabled, dr_test_on_response, & request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error %s network loopback (request): %s\n",
			(enabled ? "enabling" : "disabling"), dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

//----------------------------------------------------------
// Channel actions
//----------------------------------------------------------

static void
dr_test_rxchannel_subscribe
(
	dr_test_t * test,
	unsigned int channel,
	char * name
) {
	aud_error_t result;
	
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Subscribe\n");

	if (channel < 1 || channel > test->nrx)
	{
		DR_TEST_ERROR("Invalid RX channel (must be in range 1..%u)\n", test->nrx);
		return;
	}
	
	request = dr_test_allocate_request(test, "Subscribe");
	if (!request)
	{
		return;
	}

	if (name != NULL)
	{
		char * c = name;
		char * r = strchr(name, '@');
		if (r == NULL)
		{
			DR_TEST_ERROR("Invalid subscription name (NAME must be in the form \"channel@device\")\n");
			dr_test_request_release(request);
			return;
		}
		*r = '\0';
		r++;
		result = dr_rxchannel_subscribe(test->rx[channel-1], dr_test_on_response, &request->id, r, c);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error sending subscribe message: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_test_request_release(request);
			return;
		}
	}
	else
	{
		result = dr_rxchannel_subscribe(test->rx[channel-1], dr_test_on_response, &request->id, NULL, NULL);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error sending unsubscribe message: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_test_request_release(request);
			return;
		}
	}
}

static void
dr_test_txchannel_set_enabled
(
	dr_test_t * test,
	unsigned int channel,
	aud_bool_t enabled
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Set TxChannel Enabled\n");

	if (channel < 1 || channel > test->ntx)
	{
		DR_TEST_PRINT("Invalid TX channel (must be in range 1..%u)\n", test->ntx);
		return;
	}

	request = dr_test_allocate_request(test, "SetTxChannelEnabled");
	if (!request)
	{
		return;
	}

	DR_TEST_PRINT("Setting TX channel %u to be %s\n", channel, (enabled ? "enabled" : "disabled"));
	result = dr_txchannel_set_enabled(test->tx[channel-1], dr_test_on_response, &request->id, enabled);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending set enabled message: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_add_txlabel
(
	dr_test_t * test,
	unsigned int channel,
	char * name
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Add Tx Label\n");

	if (channel < 1 || channel > test->ntx)
	{
		DR_TEST_ERROR("Invalid TX channel (must be in range 1..%u)\n", test->ntx);
		return;
	}

	request = dr_test_allocate_request(test, "AddTxLabel");
	if (!request)
	{
		return;
	}

	DR_TEST_DEBUG("Adding label \"%s\" to tx channel %u\n", name, channel);
	result = dr_txchannel_add_txlabel(test->tx[channel-1],
		dr_test_on_response, &request->id, name, DR_MOVEFLAG_MOVE_EXISTING);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending add label \"%s\" message to tx channel %u: %s\n",
			name, channel, dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_remove_txlabel_from_channel
(
	dr_test_t * test,
	dante_id_t channel,
	char * name
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Remove Tx Label From Channel\n");

	if (channel < 1 || channel > test->ntx)
	{
		DR_TEST_ERROR("Invalid TX channel (must be in range 1..%u)\n", test->ntx);
		return;
	}

	request = dr_test_allocate_request(test, "RemoveTxLabelFromChannel");
	if (!request)
	{
		return;
	}

	DR_TEST_DEBUG("Removing label \"%s\" from tx channel %u\n", name, channel);
	result = dr_txchannel_remove_txlabel(test->tx[channel-1],
		dr_test_on_response, &request->id, name);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending remove label \"%s\" message to tx channel %u: %s\n",
			name, channel, dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}


static void
dr_test_remove_txlabel
(
	dr_test_t * test,
	dante_id_t label_id
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Remove Tx Label\n");

	if (dr_device_txlabel_with_id(test->device, label_id, NULL) == AUD_ERR_RANGE)
	{
		DR_TEST_ERROR("Invalid TX label\n");
		return;
	}

	request = dr_test_allocate_request(test, "RemoveTxLabel");
	if (!request)
	{
		return;
	}

	DR_TEST_DEBUG("Removing TX label %u\n", label_id);
	result = dr_device_remove_txlabel_with_id(test->device,
		dr_test_on_response, &request->id, label_id
	);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending remove label %u message: %s\n",
			label_id, dr_error_message(result, g_test_errbuf)
		);
		dr_test_request_release(request);
		return;
	}
}


static void
dr_test_txchannel_set_muted
(
	dr_test_t * test,
	unsigned int channel,
	aud_bool_t muted
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Set Tx Channel Muted\n");

	if (channel < 1 || channel > test->ntx)
	{
		DR_TEST_PRINT("Invalid TX channel (must be in range 1..%u)\n", test->ntx);
		return;
	}

	request = dr_test_allocate_request(test, "SetTxChannelMuted");
	if (!request)
	{
		return;
	}

	DR_TEST_PRINT("Setting TX channel %u to be %s\n", channel, (muted ? "muted" : "unmuted"));
	result = dr_txchannel_set_muted(test->tx[channel-1], dr_test_on_response, &request->id, muted);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending %s message: %s\n",
			(muted ? "mute" : "unmute"), dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_rxchannel_set_muted
(
	dr_test_t * test,
	unsigned int channel,
	aud_bool_t muted
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Set Rx Channel Muted\n");

	if (channel < 1 || channel > test->nrx)
	{
		DR_TEST_PRINT("Invalid RX channel (must be in range 1..%u)\n", test->nrx);
		return;
	}

	request = dr_test_allocate_request(test, "SetRxChannelMuted");
	if (!request)
	{
		return;
	}

	DR_TEST_PRINT("Setting RX channel %u to be %s\n", channel, (muted ? "muted" : "unmuted"));
	result = dr_rxchannel_set_muted(test->rx[channel-1], dr_test_on_response, &request->id, muted);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending %s message: %s\n",
			(muted ? "mute" : "unmute"), dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}


static void
dr_test_rxchannel_set_name
(
	dr_test_t * test,
	unsigned int channel,
	const char * name
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Set Rx Channel Name\n");

	if (channel < 1 || channel > test->nrx)
	{
		DR_TEST_PRINT("Invalid RX channel (must be in range 1..%u)\n", test->nrx);
		return;
	}

	request = dr_test_allocate_request(test, "SetRxChannelName");
	if (!request)
	{
		return;
	}

	DR_TEST_PRINT("Setting RX channel %u name to \"%s\"\n", channel, name);
	result = dr_rxchannel_set_name(test->rx[channel-1], dr_test_on_response, &request->id, name);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending rx channel rename message: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

//----------------------------------------------------------
// Transmit Flows
//----------------------------------------------------------

static void
dr_test_add_txflow
(
	dr_test_t * test,
	dante_id_t flow_id,
	uint16_t num_slots,
	dante_latency_us_t latency_us,
	dante_fpp_t fpp
) {
	dante_name_t name;
	uint16_t i;
	dr_txflow_config_t * config = NULL;
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Add Tx Flow\n");

	DR_TEST_PRINT("Enter flow name (or simply 'enter' to use default name): ");
	fgets(g_input_buf, BUFSIZ, stdin);
	if (sscanf(g_input_buf, "%s", name) < 1)
	{
		name[0] = '\0';
	}

	result = dr_txflow_config_new(test->device, flow_id, num_slots, &config);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error creating txflow config: %s\n", dr_error_message(result, g_test_errbuf));
		return;
	}

	result = dr_txflow_config_set_name(config, name);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error setting txflow config name: %s\n", dr_error_message(result, g_test_errbuf));
		dr_txflow_config_discard(config);
		return;
	}

	result = dr_txflow_config_set_latency_us(config, latency_us);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error setting txflow config latency: %s\n", dr_error_message(result, g_test_errbuf));
		dr_txflow_config_discard(config);
		return;
	}

	result = dr_txflow_config_set_fpp(config, fpp);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error setting txflow config fpp: %s\n", dr_error_message(result, g_test_errbuf));
		dr_txflow_config_discard(config);
		return;
	}

	
	// Slots
	DR_TEST_PRINT("Enter 1-based tx channel index to add to slots (or simply 'enter' to have an empty slot):\n");
	for (i = 0; i < num_slots; i++)
	{
		unsigned int id;
		DR_TEST_PRINT("slot %d (of %d): ", i, num_slots);
		fgets(g_input_buf, BUFSIZ, stdin);
		if (sscanf(g_input_buf, "%u", &id) == 1)
		{
			dr_txchannel_t * tx = dr_device_txchannel_with_id(test->device, (dante_id_t) id);
			if (tx)
			{
				result = dr_txflow_config_add_channel(config, tx, i);
			}
		}
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error setting txflow config slot: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_config_discard(config);
			return;
		}
	}

	request = dr_test_allocate_request(test, "AddTxFlow");
	if (!request)
	{
		return;
	}

	result = dr_txflow_config_commit(config, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending txflow create request: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_replace_txflow_channels
(
	dr_test_t * test,
	dante_id_t id
) {
	uint16_t s, num_slots;
	dr_txflow_t * flow = NULL;
	dr_txflow_config_t * config = NULL;
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Replace Tx Flow Channels\n");

	result = dr_device_txflow_with_id(test->device, id, &flow);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error getting txflow at index %d: %s\n",
			id, dr_error_message(result, g_test_errbuf));
		return;
	}
	result = dr_txflow_replace_channels(flow, &config);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error creating txflow modify config: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_txflow_release(&flow);
		return;
	}
	dr_txflow_release(&flow);
	num_slots = dr_txflow_config_num_slots(config);

	DR_TEST_PRINT("Enter 1-based tx channel id to add to slots (or simply 'enter' to have an empty slot):\n");
	for (s = 0; s < num_slots; s++)
	{
		unsigned int  channel_id;
		DR_TEST_PRINT("Slot %d/%d: ", s, num_slots);
		fgets(g_input_buf, BUFSIZ, stdin);
		if (sscanf(g_input_buf, "%u", &channel_id) == 1)
		{
			dr_txchannel_t * tx = dr_device_txchannel_with_id(test->device, (dante_id_t) channel_id);
			if (tx)
			{
				result = dr_txflow_config_add_channel(config, tx, s);
			}
		}
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error setting txflow conig slot: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_config_discard(config);
			return;
		}
	}

	request = dr_test_allocate_request(test, "ReplaceTxFlowChannels");
	if (!request)
	{
		return;
	}

	result = dr_txflow_config_commit(config, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending modify txflow request: %s\n",
			dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_delete_txflow
(
	dr_test_t * test,
	dante_id_t id
) {
	dr_txflow_t * flow = NULL;
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Delete Tx Flow\n");


	result = dr_device_txflow_with_id(test->device, id, &flow);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error getting txflow %d: %s\n",
			id, dr_error_message(result, g_test_errbuf));
		return;
	}

	request = dr_test_allocate_request(test, "DeleteTxFlow");
	if (!request)
	{
		return;
	}
	result = dr_txflow_delete(&flow, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending delete txflow request for flow %d: %s\n",
			id, dr_error_message(result, g_test_errbuf));
		dr_txflow_release(&flow);
		dr_test_request_release(request);
		return;
	}
}

//----------------------------------------------------------
// Rx Flows 
//----------------------------------------------------------

static void 
dr_test_read_multicast_associations
(
	dr_test_t * test,
	dr_rxflow_config_t * config
) {
	aud_error_t result;
	unsigned int i, n = dr_device_max_rxflow_slots(test->device);

	DR_TEST_PRINT("Multicast template flow associations are configured by choosing transmit channels and\n");
	DR_TEST_PRINT("Selecting one or more receive channels to be subscribed to each of those channels.\n");
	DR_TEST_PRINT("A maximum of of %d subscriptions are allowed for this device.\n", n);

	for (i = 0; i < n; i++)
	{
		dante_name_t subscribed_channel;
		unsigned int rxchannel_id;
		DR_TEST_PRINT("  Enter transmit channel name, slot %u of %u (ENTER to finish):",
			(i+1), n);
		fgets(g_input_buf, BUFSIZ, stdin);

		subscribed_channel[0] = '\0';
		sscanf(g_input_buf, "%[^\r\n]", subscribed_channel);
		if (subscribed_channel[0] == '\0')
		{
			break;
		}

		// parse ids
		do 
		{
			dr_rxchannel_t * rxc = NULL;
			DR_TEST_PRINT("    Receive channel id (ENTER to finish): ");
			fgets(g_input_buf, BUFSIZ, stdin);
			if (sscanf(g_input_buf, "%u", &rxchannel_id) != 1)
			{
				break;
			}
			if (rxchannel_id == 0 || rxchannel_id > test->nrx)
			{
				DR_TEST_PRINT("Invalid rx channel id %u\n", rxchannel_id);
				continue;
			}
			rxc = test->rx[rxchannel_id-1];
			result = dr_rxflow_config_add_associated_channel(config, rxc, subscribed_channel);
			if (result != AUD_SUCCESS)
			{
				DR_TEST_PRINT("Error adding template association channel %d subscribed to %s@%s: %s\n",
					rxchannel_id, subscribed_channel, dr_rxflow_config_get_tx_device_name(config),
					dr_error_message(result, g_test_errbuf));
				continue;
			}
		} while (rxchannel_id);
	}
}

static void 
dr_test_add_rxflow_multicast
(
	dr_test_t * test,
	unsigned int flow_id
) {
	dante_name_t tx_device_name;
	dante_name_t tx_flow_name;
	aud_error_t result;
	dr_test_request_t * request;
	dr_rxflow_config_t * config = NULL;

	DR_TEST_DEBUG("ACTION: Add Multicast Rx Flow Template\n");

	DR_TEST_PRINT("Enter the multicast transmit flow and device name in the form \"flow@device\": ");
	fgets(g_input_buf, BUFSIZ, stdin);
	if (sscanf(g_input_buf, "\"%[^@]@%[^\r\n\"]\"", tx_flow_name, tx_device_name) != 2)
	{
		printf("Invalid flow name.");
		return;
	}

	result = dr_rxflow_config_new_multicast(test->device,
		(dante_id_t) flow_id, tx_device_name, tx_flow_name,
		&config);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error creating multicast config object: %s\n", dr_error_message(result, g_test_errbuf));
		return;
	}

	dr_test_read_multicast_associations(test, config);

	request = dr_test_allocate_request(test, "AddMulticastRxFlowTemplate");
	if (!request)
	{
		return;
	}
	result = dr_rxflow_config_commit(config, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending multicast template create request: %s\n", dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void 
dr_test_read_unicast_associations
(
	dr_test_t * test,
	dr_rxflow_config_t * config
) {
	aud_error_t result;
	uint16_t i, num_slots = dr_rxflow_config_num_slots(config);

	DR_TEST_PRINT("Unicast template flow associations are configured by choosing a receive channel and\n");
	DR_TEST_PRINT("setting the transmit channel to which it will subscribe.\n");
	DR_TEST_PRINT("A maximum of of %d subscriptions are allowed for this device.\n", num_slots);
	
	for (i = 0; i < num_slots;)
	{
		dante_name_t subscribed_channel;
		unsigned int rxchannel_id;
		dr_rxchannel_t * rxc = NULL;
		
		DR_TEST_PRINT("Next receive channel id (hit ENTER to finish):");
		fgets(g_input_buf, BUFSIZ, stdin);
		if (sscanf(g_input_buf, "%u", &rxchannel_id) != 1)
		{
			break;
		}
		if (rxchannel_id == 0 || rxchannel_id > test->nrx)
		{
			DR_TEST_PRINT("Invalid rx channel id %u\n", rxchannel_id);
			continue;
		}
		rxc = test->rx[rxchannel_id-1];

		DR_TEST_PRINT("Transmit channel for rxchannel %u:", rxchannel_id);
		fgets(g_input_buf, BUFSIZ, stdin);
		sscanf(g_input_buf, "%[^\"\r\n]", subscribed_channel);
		if (subscribed_channel[0] == '\0')
		{
			DR_TEST_PRINT("Invalid transmit channel name\n");
			continue;
		}

		result = dr_rxflow_config_add_associated_channel(config, rxc, subscribed_channel);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_PRINT("Error adding template association channel %d subscribed to %s@%s: %s\n",
				rxchannel_id, subscribed_channel, dr_rxflow_config_get_tx_device_name(config),
				dr_error_message(result, g_test_errbuf));
			continue;
		}

		// all good, add to counter...
		i++;
	}
}

static void 
dr_test_add_rxflow_unicast
(
	dr_test_t * test,
	dante_id_t flow_id,
	uint16_t num_slots
) {
	char tx_device_name[BUFSIZ];
	aud_error_t result;
	dr_test_request_t * request;
	dr_rxflow_config_t * config = NULL;

	DR_TEST_DEBUG("ACTION: Add Unicast Rx Flow Template\n");

	DR_TEST_PRINT("Enter the transmit device name for the unicast template:");
	fgets(g_input_buf, BUFSIZ, stdin);
	if (sscanf(g_input_buf, "%[^\"\r\n]", tx_device_name) != 1)
	{
		printf("Invalid device name.");
		return;
	}

	result = dr_rxflow_config_new_unicast(test->device,
		(dante_id_t) flow_id, tx_device_name, num_slots,
		&config);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error creating unicast config object: %s\n", dr_error_message(result, g_test_errbuf));
		return;
	}

	dr_test_read_unicast_associations(test, config);

	request = dr_test_allocate_request(test, "AddUnicastRxFlowTemplate");
	if (!request)
	{
		return;
	}

	result = dr_rxflow_config_commit(config, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending multicast template create request: %s\n", dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void 
dr_test_replace_rxflow_associations
(
	dr_test_t * test,
	dante_id_t flow_id
) {
	dr_rxflow_t * flow = NULL;
	dr_rxflow_config_t * config = NULL;
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Replace Rx Flow Associations\n");
	
	result = dr_device_rxflow_with_id(test->device, flow_id, &flow);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error getting flow with id %d: %s\n",
			flow_id, dr_error_message(result, g_test_errbuf));
		return;
	}

	result = dr_rxflow_replace_associations(flow, &config);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error getting config for flow with id %d: %s\n",
			flow_id, dr_error_message(result, g_test_errbuf));
		return;
	}

	if (dr_rxflow_config_is_multicast_template(config))
	{
		dr_test_read_multicast_associations(test, config);
	}
	else if (dr_rxflow_config_is_unicast_template(config))
	{
		dr_test_read_unicast_associations(test, config);
	}

	request = dr_test_allocate_request(test, "ReplaceRxFlowAssociations");
	if (!request)
	{
		return;
	}

	result = dr_rxflow_config_commit(config, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending update template associations request: %s\n", dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

static void
dr_test_delete_rxflow
(
	dr_test_t * test,
	dante_id_t flow_id
) {
	aud_error_t result;
	dr_test_request_t * request;
	dr_rxflow_t * flow;

	DR_TEST_DEBUG("ACTION: Delete Rx Flow\n");

	result = dr_device_rxflow_with_id(test->device, flow_id, &flow);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error getting flow with id %d: %s\n",
			flow_id, dr_error_message(result, g_test_errbuf));
		return;
	}

	request = dr_test_allocate_request(test, "DeleteRxFlow");
	if (!request)
	{
		return;
	}

	result = dr_rxflow_delete(&flow, dr_test_on_response, &request->id);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending flow delete request: %s\n", dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}

/*
static void 
dr_test_set_one_rxflow_interface_mode
(
	dr_test_t * test,
	dr_rxflow_error_field_t field,
	dante_id_t flow_id,
	unsigned int intf,
	unsigned int window_interval_us
) {
	aud_error_t result;
	dr_test_request_t * request;

	DR_TEST_DEBUG("ACTION: Set One RxFlow Interface Mode\n");

	request = dr_test_allocate_request(test, "SetOneRxFlowInterfaceMode");
	if (!request)
	{
		return;
	}

	result = dr_device_set_one_flow_interface_mode(test->device, dr_test_on_response, &request->id, field, flow_id, intf, window_interval_us);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error sending set one rxflow interface mode request: %s\n", dr_error_message(result, g_test_errbuf));
		dr_test_request_release(request);
		return;
	}
}
*/

//----------------------------------------------------------
// Asynchronous event handlers
//----------------------------------------------------------

static void
dr_test_on_sockets_changed
(
	const dr_devices_t * devices
) {
	dr_test_t * test = (dr_test_t *) dr_devices_get_context(devices);
	DR_TEST_DEBUG("\nEVENT: sockets changed\n");
	test->async_info.sockets_changed = AUD_TRUE;
}

static void 
dr_test_on_device_changed
(
	dr_device_t * device,
	dr_device_change_flags_t change_flags
) {
	dr_test_t * test = (dr_test_t *) dr_device_get_context(device);
	dr_device_change_index_t i;

	(void) device;
	
	DR_TEST_DEBUG("\nEVENT: device changed:");
	for (i = 0; i < DR_DEVICE_CHANGE_INDEX_COUNT; i++)
	{
		if (change_flags & (1 << i))
		{
			DR_TEST_DEBUG(" %s", dr_device_change_index_to_string(i));
			if (i == DR_DEVICE_CHANGE_INDEX_STATE)
			{
				dr_device_state_t state = dr_device_get_state(device);
				DR_TEST_DEBUG(" (%s)", dr_device_state_to_string(state));
			}
		}
	}
	DR_TEST_DEBUG("\n");

	if (change_flags & DR_DEVICE_CHANGE_FLAG_STATE)
	{
		dr_test_on_device_state_changed(test);
	}

	if (change_flags & DR_DEVICE_CHANGE_FLAG_ADDRESSES)
	{
		dr_test_on_device_addresses_changed(test);
	}

	printf("Active Requests: %d/%d\n", 
		dr_devices_num_requests_pending(test->devices),
		dr_devices_get_request_limit(test->devices));

}

//----------------------------------------------------------
// Application-level functionality
//----------------------------------------------------------

static void
dr_test_help(char filter)
{
	DR_TEST_PRINT("Usage:\n\n");
	if (!filter)
	{
		DR_TEST_PRINT("?            prints a help message\n");
		DR_TEST_PRINT("q            Quit\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'c')
	{
		DR_TEST_PRINT("c +          Write the current device configuration\n");
		DR_TEST_PRINT("c -          Clear the current device configuration\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'd')
	{
		DR_TEST_PRINT("d t L F      Set device tx multicast performance properties to L microseconds and F frames per packet\n");
		DR_TEST_PRINT("d r L F      Set device rx multicast performance properties to L microseconds and F frames per packet\n");
		DR_TEST_PRINT("d u L F      Set device unicast performance properties to L microseconds and F frames per packet\n");
		DR_TEST_PRINT("d p          'Ping' the device (sends a no-op routing message and waits for a response)\n");
		DR_TEST_PRINT("d !          Mark properties component as stale\n");
		DR_TEST_PRINT("d            Display device information (properties,capabilities,status)\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'e')
	{
		DR_TEST_PRINT("e N +        Enable tx channel N\n");
		DR_TEST_PRINT("e N -        Disable tx channel N\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'f')
	{
		DR_TEST_PRINT("f + N S L F  Create new transmit flow with id N, S slots, latency L and F fpp\n");
		DR_TEST_PRINT("f + S        Create new transmit flow with S slots\n");
		DR_TEST_PRINT("f N          Modify transmit flow N\n");
		DR_TEST_PRINT("f N -        Delete transmit flow N\n");
		DR_TEST_PRINT("f !          Mark transmit flow component as being stale\n");
		DR_TEST_PRINT("f            Display all transmit flows\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'g')
	{
		DR_TEST_PRINT("g m N        Create a new multicast receive template on flow id N\n");
		DR_TEST_PRINT("g u N S      Create a new unicast receive template on flow id N with S slots\n");
		DR_TEST_PRINT("g * N        Replace channel associations for template with flow id N\n");
		DR_TEST_PRINT("g - N        Delete template with flow id N\n");
		DR_TEST_PRINT("g !          Mark receive flow component as being stale\n");
		DR_TEST_PRINT("g            Display all receive flows\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'l')
	{
		DR_TEST_PRINT("l N \"NAME\" + Add label NAME to tx channel N\n");
		DR_TEST_PRINT("l N \"NAME\" - Remove label NAME from tx channel N\n");
		DR_TEST_PRINT("l N          List labels for tx channel N\n");
		DR_TEST_PRINT("l !          Mark tx labels component as stale\n");
		DR_TEST_PRINT("l            List labels for all tx channels\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'L')
	{
		DR_TEST_PRINT("L N -        Remove label N\n");
		DR_TEST_PRINT("L N          Display label N\n");
		DR_TEST_PRINT("L !          Mark tx labels component as stale\n");
		DR_TEST_PRINT("L            List all labels\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'n')
	{
		DR_TEST_PRINT("n NAME       Rename the device to NAME (closes the device handle)\n");
		DR_TEST_PRINT("n -          Rename the device to its default name (closes the device handle)\n");
		DR_TEST_PRINT("n            Print the device's name\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'o')
	{
		DR_TEST_PRINT("o +          Enable network loopback\n");
		DR_TEST_PRINT("o -          Disable network loopback\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'r')
	{
		DR_TEST_PRINT("r !          Mark rx channel component as stale\n");
		DR_TEST_PRINT("r N m +      Mute rx channel N\n");
		DR_TEST_PRINT("r N m -      Unute rx channel N\n");
		DR_TEST_PRINT("r N \"NAME\"   Renamed rx channel N\n");
		DR_TEST_PRINT("r            Display rx channel information\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 's')
	{
		DR_TEST_PRINT("s N \"NAME\"   Subscribe rx channel N to network channel NAME\n");
		DR_TEST_PRINT("s N          Unsubscribe rx channel N\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 't')
	{
		DR_TEST_PRINT("t !          Mark tx channel component as stale\n");
		DR_TEST_PRINT("t N m +      Mute tx channel N\n");
		DR_TEST_PRINT("t N m -      Unute tx channel N\n");
		DR_TEST_PRINT("t            Display tx channel information\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'u')
	{
		DR_TEST_PRINT("u q          Query (or re-query) capabilities\n");
		DR_TEST_PRINT("u !          Invalidate and update all components\n");
		DR_TEST_PRINT("u            Update information for all stale components\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'U')
	{
		DR_TEST_PRINT("U TYPE +     Get and clear rxflow error field TYPE for all flows\n");
		DR_TEST_PRINT("U TYPE -     Get rxflow error field TYPE for all flows\n"); 
		DR_TEST_PRINT("U +          Get and clear rxflow error flags for all flows\n");
		DR_TEST_PRINT("U -          Get rxflow error flags for all flows\n");
		DR_TEST_PRINT("U N          Print rxflow error information for rx flow N\n");
		DR_TEST_PRINT("U            Print rxflow error information for all rx flows\n");
		DR_TEST_PRINT("\n");
	}
	if (!filter || filter == 'x')
	{
		DR_TEST_PRINT("x +          Put the device into lockdown\n");
		DR_TEST_PRINT("x -          Take the device out of lockdown\n");
		DR_TEST_PRINT("\n");
	}
}

static void
dr_test_usage
(
	const char * bin
) {
	printf("Usage: %s OPTIONS [DEVICE]\n", bin);
	printf("  OPTIONS are\n");
	printf("    -h=N set num of handles to N\n");
	printf("    -r=N set num of requests to N\n");
	printf("    -ii=INDEX use local interface INDEX (specify once per interface to be used)\n");
	printf("    -i=INDEX use local interface NAME (specify once per interface to be used)\n");
	printf("    -a=ADDRESS use address A instead of name (specify once per interface to be used)\n");
	printf("    -u=BOOL enable/disable automatic query / updates on state changes\n");
	printf("    -p=PORT set port for local device connection (for debugging purposes only)\n");
	printf("  If no name or addresses specified then connect to the local dante device via localhost\n");
}

static void
dr_test_parse_options
(
	dr_test_options_t * options,
	int argc, 
	char * argv[]
) {
	int a;

	// init defaults
	options->automatic_update_on_state_change = AUD_TRUE;

	// and parse options
	for (a = 1; a < argc; a++)
	{
		if (!strncmp(argv[a], "-h=", 3))
		{
			options->num_handles = atoi(argv[a]+3);
		}
		else if (!strncmp(argv[a], "-r=", 3))
		{
			options->request_limit = atoi(argv[a]+3);
		}
		else if (!strncmp(argv[a], "-ii=", 4))
		{
			if (options->num_local_interfaces < DR_TEST_MAX_INTERFACES)
			{
				int local_interface_index = atoi(argv[a]+4);
				if (local_interface_index)
				{
					options->local_interfaces[options->num_local_interfaces].index = local_interface_index;
					options->local_interfaces[options->num_local_interfaces].flags = AUD_INTERFACE_IDENTIFIER_FLAG_INDEX;
					options->num_local_interfaces++;
				}
				else
				{
					dr_test_usage(argv[0]);
					exit(1);
				}
			}
		}
		else if (!strncmp(argv[a], "-i=", 3))
		{
			if (options->num_local_interfaces < DR_TEST_MAX_INTERFACES)
			{
				const char * intf_name = argv[a]+3;
				if (intf_name[0])
				{
					aud_strlcpy(
						(char *) options->local_interfaces[options->num_local_interfaces].name,
						intf_name,
						AUD_INTERFACE_NAME_LENGTH
					);
					options->local_interfaces[options->num_local_interfaces].flags = AUD_INTERFACE_IDENTIFIER_FLAG_NAME;
					options->num_local_interfaces++;
				}
				else
				{
					dr_test_usage(argv[0]);
					exit(1);
				}
			}
		}
		else if (!strncmp(argv[a], "-a=", 3))
		{
			if (options->num_addresses < DR_TEST_MAX_INTERFACES)
			{
				unsigned int addr = inet_addr(argv[a]+3);
				options->addresses[options->num_addresses++] = addr;
			}
		}
		else if (!strcmp(argv[a], "-u=true"))
		{
			options->automatic_update_on_state_change = AUD_TRUE;
		}
		else if (!strcmp(argv[a], "-u=false"))
		{
			options->automatic_update_on_state_change = AUD_FALSE;
		}
		else if (!strncmp(argv[a], "-p=", 3) && strlen(argv[a]) > 3)
		{
			options->local_port = (uint16_t) atoi(argv[a]+3);
		}
		else if (argv[a][0] == '-')
		{
			dr_test_usage(argv[0]);
			exit(1);
		}
		else
		{
			options->device_name = argv[a];
			break;
		}
	}
}

static aud_error_t 
dr_test_process_line(dr_test_t * test, char * buf)
{
	char in_name[BUFSIZ];
	char in_action[BUFSIZ];
	char in_c;
	unsigned int in_channel, in_id, in_count, in_fpp, in_latency, in_type;
	int match_count;

	// trim beginning and end of string
	//printf("Before=\"%s\"\n", buf);
	while(*buf && isspace(*buf)) buf++;
	{
		char * end = buf;
		while (*end) end++;
		end--;
		while (end > buf && isspace(*end)) {*end = '\0'; end--;}
	}
	//printf("After=\"%s\"\n", buf);

	switch (buf[0])
	{
	case '?':
		{
			dr_test_help(0);
			break;
		}
	
	case 'c':
		{
			if (sscanf(buf, "c %s", in_action) == 1 && !strcmp(in_action, "+"))
			{
				dr_test_store_config(test);
			}
			else if (sscanf(buf, "c %s", in_action) == 1 && !strcmp(in_action, "-"))
			{
				dr_test_clear_config(test);
			}
			else
			{
				dr_test_help('c');
			}
			break;
		}

	case 'd':
		{
			if (sscanf(buf, "d t %u %u", &in_latency, &in_fpp) == 2)
			{
				dr_test_set_performance(test, DR_TEST_PERFORMANCE_TX, (dante_latency_us_t) in_latency, (dante_fpp_t) in_fpp);
			}
			else if (sscanf(buf, "d r %u %u", &in_latency, &in_fpp) == 2)
			{
				dr_test_set_performance(test, DR_TEST_PERFORMANCE_RX, (dante_latency_us_t) in_latency, (dante_fpp_t) in_fpp);
			}
			else if (sscanf(buf, "d u %u %u", &in_latency, &in_fpp) == 2)
			{
				dr_test_set_performance(test, DR_TEST_PERFORMANCE_UNICAST, (dante_latency_us_t) in_latency, (dante_fpp_t) in_fpp);
			}
			else if (sscanf(buf, "d %s", in_action) == 1)
			{
				if (!strcmp(in_action, "p"))
				{
					dr_test_ping(test);
				}
				else if (!strcmp(in_action, "!"))
				{
					dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_PROPERTIES);
				}
				else
				{
					dr_test_help('d');
				}
			}
			else
			{
				dr_test_print_device_info(test->device);
			}
			break;
		}

	case 'e':
		{
			if (sscanf(buf, "e %u %s", &in_channel, in_action) == 2 && !strcmp(in_action, "+"))
			{
				dr_test_txchannel_set_enabled(test, in_channel, AUD_TRUE);
			}
			else if (sscanf(buf, "e %u %s", &in_channel, in_action) == 2 && !strcmp(in_action, "-"))
			{
				dr_test_txchannel_set_enabled(test, in_channel, AUD_FALSE);
			}
			else
			{
				dr_test_help('e');
			}
			break;
		}

	// transmit flows
	case 'f':
		{
			if (sscanf(buf, "f + %u %u %u %u", &in_id, &in_count, &in_latency, &in_fpp) == 4)
			{
				dr_test_add_txflow(test, (dante_id_t) in_id, (uint16_t) in_count, (dante_latency_us_t) in_latency, (dante_fpp_t) in_fpp);
			}
			else if (sscanf(buf, "f + %u", &in_count) == 1)
			{
				dr_test_add_txflow(test, 0, (uint16_t) in_count, 0, 0);
			}
			else if (sscanf(buf, "f %u %c", &in_id, &in_c) == 2 && in_c == '-')
			{
				dr_test_delete_txflow(test, (dante_id_t) in_id);
			}
			else if (sscanf(buf, "f %u", &in_id) == 1)
			{
				dr_test_replace_txflow_channels(test, (dante_id_t) in_id);
			}
			else if (sscanf(buf, "f %s", in_action) == 1)
			{
				if (!strcmp(in_action, "!"))
				{
					dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_TXFLOWS);
				}
				else
				{
					dr_test_help('f');
				}
			}
			else
			{
				dr_test_print_device_txflows(test->device);
			}
			break;
		}

	// receive flows
	case 'g':
		{

			if (sscanf(buf, "g u %d %d", &in_id, &in_count) == 2)
			{
				dr_test_add_rxflow_unicast(test, (dante_id_t) in_id, (uint16_t) in_count);
			}
			else if (sscanf(buf, "g %c %d", &in_c, &in_id) == 2)
			{
				if (in_c == 'm')
				{
					dr_test_add_rxflow_multicast(test, in_id);
				}
				else if (in_c == '*')
				{
					dr_test_replace_rxflow_associations(test, (dante_id_t) in_id);
				}
				else if (in_c == '-')
				{
					dr_test_delete_rxflow(test, (dante_id_t) in_id);
				}
				else
				{
					dr_test_help('g');
				}
			}
			else if (sscanf(buf, "g %s", in_action) == 1)
			{
				if (!strcmp(in_action, "!"))
				{
					dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_RXFLOWS);
				}
				else
				{
					dr_test_help('g');
				}
			}
			else
			{
				dr_test_print_device_rxflows(test->device);
			}
			break;
		}

	case 'l':
		{
			if (sscanf(buf, "l %u \"%[^\"\r\n]%c %s", &in_channel, in_name, &in_c, in_action) == 4 && in_c == '\"')
			{
				if (!strcmp(in_action, "+"))
				{
					dr_test_add_txlabel(test, in_channel, in_name);
				}
				else if (!strcmp(in_action, "-"))
				{
					dr_test_remove_txlabel_from_channel(test, (dante_id_t) in_channel, in_name);
				}
				else
				{
					dr_test_help('l');
				}
			}
			else if (sscanf(buf, "l %u %s", &in_channel, in_name) == 2)
			{
				dr_test_help('l');
			}
			else if (sscanf(buf, "l %u", &in_channel) == 1)
			{
				dr_test_print_channel_txlabels(test->device, (dante_id_t) in_channel);
			}
			else if (sscanf(buf, "l %s", in_name) == 1)
			{
				if (!strcmp(in_name, "!"))
				{
					dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_TXLABELS);
				}
				else
				{
					dr_test_help('l');
				}
			}
			else
			{
				dr_test_print_channel_txlabels(test->device, 0);
			}
			break;
		}

	case 'L':
		{
			match_count = sscanf(buf, "L %u %s", &in_id, in_action);
			if (match_count == 2)
			{
				if (!strcmp(in_action, "-"))
				{
					dr_test_remove_txlabel(test, (dante_id_t) in_id);
				}
				else
				{
					dr_test_help('L');
				}
			}
			else if (match_count == 1)
			{
				dr_test_print_device_txlabels(test->device, (dante_id_t) in_id, AUD_FALSE);
			}
			else
			{
				match_count = sscanf(buf, "L %s", in_action);
				if (match_count == 1)
				{
					if (!strcmp(in_action, "!"))
					{
						dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_TXLABELS);
					}
					else
					{
						dr_test_help('L');
					}
				}
				else
				{
					dr_test_print_device_txlabels(test->device, 0, AUD_TRUE);
				}
			}
			break;
		}

	case 'n':
		{
			if (sscanf(buf, "n %s", in_name) == 1)
			{
				if (!strcmp(in_name, "-"))
				{
					dr_test_rename(test, NULL);
				}
				else
				{
					dr_test_rename(test, in_name);
				}
				return AUD_ERR_DONE;
			}
			else
			{
				dr_test_print_device_names(test->device);
			}
			break;
		}

	case 'o':
		{
			if (sscanf(buf, "o %s", in_action) == 1 && !strcmp(in_action, "+"))
			{
				dr_test_set_network_loopback(test, AUD_TRUE);
			}
			else if (sscanf(buf, "o %s", in_action) == 1 && !strcmp(in_action, "-"))
			{
				dr_test_set_network_loopback(test, AUD_FALSE);
			}
			else
			{
				dr_test_help('o');
			}
			break;
		}

	case 'q':
		{
			printf("\n");
			g_test_running = AUD_FALSE;
			break;
		}

	case 'r':
		{
			if (sscanf(buf, "r %u m %c", &in_channel, &in_c) == 2 && in_c == '+')
			{
				dr_test_rxchannel_set_muted(test, in_channel, AUD_TRUE);
			}
			else if (sscanf(buf, "r %u m %c", &in_channel, &in_c) == 2 && in_c == '-')
			{
				dr_test_rxchannel_set_muted(test, in_channel, AUD_FALSE);
			}
			else if (sscanf(buf, "r %u \"%[^\"\r\n]%c", &in_channel, in_name, &in_c) == 3 && in_c == '\"')
			{
				dr_test_rxchannel_set_name(test, in_channel, in_name);
			}

			else if (sscanf(buf, "r %s", in_action) == 1)
			{
				if (!strcmp(in_action, "!"))
				{
					dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_RXCHANNELS);
				}
				else
				{
					dr_test_help('r');
				}
			}
			else
			{
				dr_test_print_device_rxchannels(test->device);
			}
			break;
		}

	case 's':
		{
			if (sscanf(buf, "s %u \"%[^\"\r\n]%c", &in_channel, in_name, &in_c) == 3 && in_c == '"')
			{
				dr_test_rxchannel_subscribe(test, in_channel, in_name);
			}
			else if (sscanf(buf, "s %u %s", &in_channel, in_name) == 2)
			{
				DR_TEST_PRINT("NAME must be of the form \"channel@device\"\n");
			}
			else if (sscanf(buf, "s %u", &in_channel) == 1)
			{
				dr_test_rxchannel_subscribe(test, in_channel, NULL);
			}
			else
			{
				dr_test_help('s');
			}
			break;
		}

	case 't':
		{
			if (sscanf(buf, "t %u m %c", &in_channel, &in_c) == 2 && in_c == '+')
			{
				dr_test_txchannel_set_muted(test, in_channel, AUD_TRUE);
			}
			else if (sscanf(buf, "t %u m %c", &in_channel, &in_c) == 2 && in_c == '-')
			{
				dr_test_txchannel_set_muted(test, in_channel, AUD_FALSE);
			}
			else if (sscanf(buf, "t %s", in_action) == 1)
			{
				if (!strcmp(in_action, "!"))
				{
					dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_TXCHANNELS);
				}
				else
				{
					dr_test_help('t');
				}
			}
			else
			{
				dr_test_print_device_txchannels(test->device);
			}
			break;
		}

	case 'u':
		{
			if (sscanf(buf, "u %s", in_action) == 1)
			{
				if (!strcmp(in_action, "q"))
				{
					dr_test_query_capabilities(test);
				}
				if (!strcmp(in_action, "!"))
				{
					dr_test_mark_component_stale(test, DR_DEVICE_COMPONENT_COUNT);
					dr_test_update(test);
				}
				else
				{
					dr_test_help('u');
				}
			}
			else
			{
				dr_test_update(test);
			}
			break;
		}

	case 'U':
		{
			if (sscanf(buf, "U %u %c", &in_type, &in_c) == 2 && in_c == '+')
			{
				dr_test_update_rxflow_errors(test, (dante_rxflow_error_type_t) in_type, AUD_TRUE);
			}
			else if (sscanf(buf, "U %u %c", &in_type, &in_c) == 2 && in_c == '-')
			{
				dr_test_update_rxflow_errors(test, (dante_rxflow_error_type_t) in_type, AUD_FALSE);
			}
			else if (sscanf(buf, "U %c", &in_c) == 1 && in_c == '+')
			{
				dr_test_update_rxflow_errors(test, DANTE_NUM_RXFLOW_ERROR_TYPES, AUD_TRUE);
			}
			else if (sscanf(buf, "U %c", &in_c) == 1 && in_c == '-')
			{
				dr_test_update_rxflow_errors(test, DANTE_NUM_RXFLOW_ERROR_TYPES, AUD_FALSE);
			}
			else if (sscanf(buf, "U %u", &in_id) == 1)
			{
				dr_test_print_device_rxflow_errors(test->device, (dante_id_t) in_id);
			}
			else
			{
				dr_test_print_device_rxflow_errors(test->device, 0);
			}
		}

	case 'x':
		{
			if (sscanf(buf, "x %s", in_action) == 1)
			{
				if (!strcmp(in_action, "+"))
				{
					dr_test_set_lockdown(test, AUD_TRUE);
				}
				else if (!strcmp(in_action, "-"))
				{
					dr_test_set_lockdown(test, AUD_FALSE);
				}
				else
				{
					dr_test_help('x');
				}
			}
			else
			{
				dr_test_update(test);
			}
			break;
		}

	default:
		if (buf[0])
		{
			dr_test_help(0);
			break;
		}
	}
	return AUD_SUCCESS;
}

//----------------------------------------------------------
// Main run loop and socket handling
//----------------------------------------------------------

static aud_error_t
dr_test_update_sockets
(
	dr_test_t * test,
	dante_sockets_t * sockets
) {

	aud_error_t result;

	dante_sockets_clear(sockets);
	result = dr_devices_get_sockets(test->devices, sockets);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error getting device sockets: %s\n",
			dr_error_message(result, g_test_errbuf));
		return result;
	}
	test->async_info.sockets_changed = AUD_FALSE;

	{
		char buf[128];
		dr_test_print_sockets(sockets, buf, sizeof(buf));
		DR_TEST_PRINT("Updated sockets, sockets are now %s\n", buf);
	}


	return AUD_SUCCESS;
}

static aud_error_t
dr_test_main_loop
(
	dr_test_t * test
) {
	aud_error_t result;
	dante_sockets_t all_sockets;
	aud_bool_t print_prompt = AUD_TRUE;
	
	test->async_info.sockets_changed = AUD_TRUE;

#ifdef  _WIN32
	// set to line buffered mode.
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT);
#endif

	DR_TEST_PRINT("\nDante Routing API test program. Type '?' for help\n\n");
	
	while(g_test_running)
	{
		dante_sockets_t select_sockets;
		char buf[BUFSIZ];
		int select_result;
		struct timeval timeout;
		
		// print prompt if needed
		if (print_prompt)
		{
			DR_TEST_PRINT("\n'%s'> ", dr_device_get_name(test->device));
			fflush(stdout);
			print_prompt = AUD_FALSE;
		}

		// update sockets if needed
		if (test->async_info.sockets_changed)
		{
			dr_test_update_sockets(test, &all_sockets);
#ifndef _WIN32
			dante_sockets_add_read(&all_sockets, 0); // 0 is always stdin
#endif
		}
	
		// prepare sockets and timeouts for 
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;
		select_sockets = all_sockets;

		// drive select loop
		/*{
			char buf[128];
			dr_test_print_sockets(&select_sockets, buf, sizeof(buf));
			DR_TEST_PRINT("pre-select, sockets are %s\n", buf);
		}*/
		select_result = select(select_sockets.n, &select_sockets.read_fds, NULL, NULL, &timeout);
		/*{
			char buf[128];
			dr_test_print_sockets(&select_sockets, buf, sizeof(buf));
			DR_TEST_PRINT("post-select, sockets are %s\n", buf);
		}*/

		if (select_result < 0)
		{
			result = aud_error_get_last();
			if (result == AUD_ERR_INTERRUPTED)
			{
				continue;
			}
			DR_TEST_ERROR("Error select()ing: %s\n", dr_error_message(result, g_test_errbuf));
			return result;
		}
		else if (select_result > 0)
		{
			result = dr_devices_process(test->devices, &select_sockets);
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
		if (FD_ISSET(0, &select_sockets.read_fds)) // 0 is always stdin
		{
			if (fgets(buf, BUFSIZ, stdin) == NULL)
			{
				result = aud_error_get_last();
				if (feof(stdin))
				{
					DR_TEST_PRINT("Exiting...\n");
					return AUD_SUCCESS;
				}
				else if (result == AUD_ERR_INTERRUPTED)
				{
					clearerr(stdin);
				}
				else
				{
					DR_TEST_ERROR("Exiting with %s\n", dr_error_message(result, g_test_errbuf));
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
			DR_TEST_PRINT("\n");
		#endif
			result = dr_test_process_line(test, buf);
			if (result != AUD_SUCCESS)
			{
				break;
			}
		}
	}
	return AUD_SUCCESS;
}

//----------------------------------------------------------
// Application entry
//----------------------------------------------------------

int main(int argc, char * argv[])
{
	dr_test_options_t options;
	dr_test_t test;
	aud_error_t result = AUD_SUCCESS;

	DR_TEST_PRINT("%s: Routing API version %u.%u.%u\n",
		argv[0], DR_VERSION_MAJOR, DR_VERSION_MINOR, DR_VERSION_BUGFIX);

	// use default ARCP port for now... (make overridable fromn command line)
	memset(&options, 0, sizeof(dr_test_options_t));
	memset(&test, 0, sizeof(dr_test_t));

	test.options = &options;

	dr_test_parse_options(&options, argc, argv);

	// create an environment
	result = aud_env_setup(&test.env);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error initialising environment: %s\n", dr_error_message(result, g_test_errbuf));
		goto cleanup;
	}
	//aud_log_set_threshold(aud_env_get_log(test.env), AUD_LOGTYPE_STDOUT, AUD_LOG_DEBUG);

	// create a devices structure and set its options
	result = dr_devices_new(test.env, &test.devices);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error creating device factory: %s\n", dr_error_message(result, g_test_errbuf));
		goto cleanup;
	}
	dr_devices_set_context(test.devices, &test);
	dr_devices_set_sockets_changed_callback(test.devices, dr_test_on_sockets_changed);
	if (options.num_handles)
	{
		result = dr_devices_set_num_handles(test.devices, options.num_handles);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error setting num_handles to %d: %s\n", options.num_handles, dr_error_message(result, g_test_errbuf));
			goto cleanup;
		}
	}
	if (options.request_limit)
	{
		result = dr_devices_set_request_limit(test.devices, options.request_limit);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error setting request_limit to %d: %s\n", options.request_limit, dr_error_message(result, g_test_errbuf));
			goto cleanup;
		}
	}
	
	// open a device connection
	result = dr_test_open(&test, &options);
	if (result != AUD_SUCCESS)
	{
		goto cleanup;
	}

	// and run the main loop
	dr_test_main_loop(&test);

cleanup:
	if (test.device)
	{
		dr_device_close(test.device);
	}
	if (test.devices)
	{
		dr_devices_delete(test.devices);
	}
	if (test.env)
	{
		aud_env_release(test.env);
	}
	return result;
}
