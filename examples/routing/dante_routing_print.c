#include "dante_routing_test.h"

void
dr_test_print_sockets
(
	const dante_sockets_t * sockets,
	char * buf,
	size_t len
) {
	size_t offset = 0;
	uint16_t i;

#ifdef _WIN32
	for (i = 0; i < sockets->read_fds.fd_count; i++)
	{
		offset += SNPRINTF(buf+offset, len-offset, "%c%d", (i ? ',' : '['), sockets->read_fds.fd_array[i]);
	}
#else
	for (i = 0; i < sockets->n; i++)
	{
		if (FD_ISSET(i, (fd_set *) &sockets->read_fds))
			// cast removes 'const'
		{
			offset += SNPRINTF(buf+offset, len-offset, "%c%d", (i ? ',' : '['), i);
			DR_TEST_PRINT(" %d", i);
		}
	}
#endif	
	offset += SNPRINTF(buf+offset, len-offset, "]");
}

void
dr_test_print_connections_active
(
	uint16_t num_connections,
	uint16_t connections_active,
	char * buf,
	size_t len
) {
	size_t offset = 0;
	uint16_t i;
	if (num_connections)
	{
		for (i = 0; i < num_connections; i++)
		{
			char c = ((connections_active & (1 << i)) ? '+' : '-'); 
			if (i)
			{
				offset += SNPRINTF(buf+offset, len-offset, ",%c", c);
			}
			else
			{
				offset += SNPRINTF(buf+offset, len-offset, "%c", c);
			}
		}
	}
	else
	{
		SNPRINTF(buf, len, "-");
	}
}

void
dr_test_print_formats
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
}

void
dr_test_print_legacy_channel_formats
(
	dante_samplerate_t samplerate,
	uint16_t num_encodings,
	dante_encoding_t * encodings,
	char * buf,
	size_t len
) {
	uint16_t e;
	size_t offset = 0;
	
	if (num_encodings > 1)
	{
		offset += SNPRINTF(buf+offset, len-offset, "%u/[", samplerate);
		for (e = 0; e < num_encodings; e++)
		{
			if (e > 0)
			{
				offset += SNPRINTF(buf+offset, len-offset, ",%u", encodings[e]);
			}
			else
			{
				offset += SNPRINTF(buf+offset, len-offset, "%u", encodings[e]);
			}
		}
		offset += SNPRINTF(buf+offset, len-offset, "]");
	}
	else if (num_encodings == 1)
	{
		SNPRINTF(buf, len, "%u/%u", samplerate, encodings[0]);
	}
	else
	{
		strcpy(buf, "-");
	}
}

void
dr_test_print_addresses
(
	uint16_t num_addresses,
	dante_ipv4_address_t * addresses,
	char * buf,
	size_t len
) {
	uint16_t a;
	size_t offset = 0;

	if (num_addresses)
	{
		for (a = 0; a < num_addresses; a++)
		{
			struct in_addr in;
			in.s_addr = addresses[a].host;
			if (a)
			{
				offset += SNPRINTF(buf+offset,len-offset, ",");
			}
			offset += SNPRINTF(buf+offset,len-offset, "%s:%d",
				inet_ntoa(in), addresses[a].port);
		}
	}
	else
	{
		strcpy(buf, "-");
	}
}

void
dr_test_print_device_names
(
	const dr_device_t * device
) {
	const char * actual_name = dr_device_get_actual_name(device);
	const char * connect_name = dr_device_get_connect_name(device);
	const char * advertised_name = dr_device_get_advertised_name(device);
	
	if (! actual_name)
		actual_name = "<not available>";
	if (! connect_name)
		connect_name = "[localhost]";
	if (! advertised_name)
		advertised_name = "<not available>";

	DR_TEST_PRINT(
		"Device name is '%s'\n"
		"  Connect name:    %s\n"
		"  Advertised name: %s\n"
		, actual_name, connect_name, advertised_name
	);
}

const char *
STATUS_STRINGS[] = 
{
	"NAME_CONFLICT",
	"UNLICENSED",
	"LOCKDOWN"
};

void
dr_test_print_device_info
(
	const dr_device_t * device
) {
	aud_error_t result;
	dr_device_state_t state = dr_device_get_state(device);
	dr_device_component_t c;
	unsigned int i, num_addresses = 2;
	dante_ipv4_address_t addresses[2];
	uint16_t min_port, max_port;
	uint16_t max_txflows, max_rxflows, max_txlabels;
	dr_device_status_flags_t status_flags = 0;

	DR_TEST_PRINT("Device %s: state %s\n", dr_device_get_name(device), dr_device_state_to_string(state));
	if (state == DR_DEVICE_STATE_ERROR)
	{
		aud_error_t error_state_error = dr_device_get_error_state_error(device);
		const char * error_state_action = dr_device_get_error_state_action(device);
	
		DR_TEST_PRINT("  error=%s action='%s'\n",
			dr_error_message(error_state_error, g_test_errbuf), 
			(error_state_action ? error_state_action : ""));
	}
	dr_device_get_addresses(device, &num_addresses, addresses);
	DR_TEST_PRINT("  Address");
	for (i = 0; i < num_addresses; i++)
	{
		uint8_t * host = (uint8_t *) &(addresses[i].host);
		DR_TEST_PRINT("%c%u.%u.%u.%u:%u", 
			(i ? ',' : '='),
			host[0], host[1], host[2], host[3], addresses[i].port);
	}
	DR_TEST_PRINT("\n");
		
	if (state == DR_DEVICE_STATE_ACTIVE)
	{
		DR_TEST_PRINT("  Fixed Capabilities:\n");
		DR_TEST_PRINT("    num interfaces:   %d\n", dr_device_num_interfaces(device));
		result = dr_device_max_txlabels(device, &max_txlabels);
		DR_TEST_PRINT("    num tx channels:  %d\n", dr_device_num_txchannels(device));
		DR_TEST_PRINT("    num rx channels:  %d\n", dr_device_num_rxchannels(device));
		result = dr_device_max_txlabels(device, &max_txlabels);
		if (result == AUD_SUCCESS)
		{
			DR_TEST_PRINT("    max tx labels:  %d\n", max_txlabels);
		}
		else
		{
			DR_TEST_PRINT("    max tx labels:  N/A\n");
		}
		result = dr_device_max_txflows(device, &max_txflows);
		if (result == AUD_SUCCESS)
		{
			DR_TEST_PRINT("    max tx flows:  %d\n", max_txflows);
		}
		else
		{
			DR_TEST_PRINT("    max tx flows:  N/A\n");
		}
		result = dr_device_max_rxflows(device, &max_rxflows);
		if (result == AUD_SUCCESS)
		{
			DR_TEST_PRINT("    max rx flows:  %d\n", max_rxflows);
		}
		else
		{
			DR_TEST_PRINT("    max rx flows:  N/A\n");
		}
		
		DR_TEST_PRINT("    max tx flow slots: %d\n", dr_device_max_txflow_slots(device));
		DR_TEST_PRINT("    max rx flow slots: %d\n", dr_device_max_rxflow_slots(device));
		
		DR_TEST_PRINT("\n");
		DR_TEST_PRINT("  Component freshness:\n");
		for (c = 0; c < DR_DEVICE_COMPONENT_COUNT; c++)
		{
			aud_bool_t stale = dr_device_is_component_stale(device, c);
			DR_TEST_PRINT("    %s: %s\n",
				dr_device_component_to_string(c), 
				(stale ? "stale" : "ok"));
		}

		DR_TEST_PRINT("  Properties:\n");
		DR_TEST_PRINT("    tx latency:     %dus\n", dr_device_get_tx_latency_us(device));
		DR_TEST_PRINT("    tx latency min: %dus\n", dr_device_get_tx_latency_min_us(device));
		DR_TEST_PRINT("    tx fpp:         %d\n", dr_device_get_tx_fpp(device));
		DR_TEST_PRINT("    rx latency:     %dus\n", dr_device_get_rx_latency_us(device));
		DR_TEST_PRINT("    rx latency max: %dus\n", dr_device_get_rx_latency_max_us(device));
		DR_TEST_PRINT("    rx fpp:         %d\n", dr_device_get_rx_fpp(device));
		DR_TEST_PRINT("    rx fpp min:     %d\n", dr_device_get_rx_fpp_min(device));
		DR_TEST_PRINT("    rx flow default slots: %d\n", dr_device_get_rx_flow_default_slots(device));
		{
			const char * clock_subdomain_name = dr_device_get_clock_subdomain_name(device);
			DR_TEST_PRINT("    clock subdomain name: \"%s\"\n", 
				(clock_subdomain_name ? clock_subdomain_name : ""));
		}
		DR_TEST_PRINT("    network loopback: %s\n", 
			dr_device_has_network_loopback(device) ? 
				(dr_device_get_network_loopback(device) ? "on" : "off") 
				: "N/A");
		result = dr_device_get_manual_unicast_receive_port_range(device, &min_port, &max_port);
		if (result == AUD_SUCCESS)
		{
			DR_TEST_PRINT("    unicast port range: %d->%d\n", min_port, max_port);
		}
		else
		{
			DR_TEST_PRINT("    unicast port range: N/A\n");
		}


		result = dr_device_get_status_flags(device, &status_flags);
		if (result == AUD_SUCCESS)
		{
			uint32_t i;
			DR_TEST_PRINT("  Device Status Flags:\n");
			for (i = 0; i < DR_DEVICE_STATUS_COUNT; i++)
			{
				aud_bool_t b = (status_flags & (1 << i));
				DR_TEST_PRINT("    %13s: %s\n", STATUS_STRINGS[i], b ? "true" : "false");
			}
		}
		else
		{
			DR_TEST_PRINT("    unknown (error=%s)\n", dr_error_message(result, g_test_errbuf));
		}

		{
			dante_rxflow_error_type_t type;
			dante_rxflow_error_flags_t rxflow_error_flags = dr_device_available_rxflow_error_flags(device);
			dante_rxflow_error_flags_t rxflow_error_fields = dr_device_available_rxflow_error_fields(device);
			uint32_t subsecond_range = dr_device_rxflow_error_subsecond_range(device);

			DR_TEST_PRINT("  RxFlow Error Flags: 0x%04x\n", rxflow_error_flags);
			for (type = 0; type < DANTE_NUM_RXFLOW_ERROR_TYPES; type++)
			{
				DR_TEST_PRINT("    %s: %s\n",
					dante_rxflow_error_type_to_string(type),
					(rxflow_error_flags & (1 << type)) ? "true" : "false");
			}
			DR_TEST_PRINT("  RxFlow Error Fields: 0x%04x\n", rxflow_error_fields);
			for (type = 0; type < DANTE_NUM_RXFLOW_ERROR_TYPES; type++)
			{
				DR_TEST_PRINT("    %s: %s\n",
					dante_rxflow_error_type_to_string(type),
					(rxflow_error_fields & (1 << type)) ? "true" : "false");
			}
			DR_TEST_PRINT("  RxFlow Error Subsecond Range: %u\n", subsecond_range);
		}
	}
}

void
dr_test_print_device_txchannels
(
	dr_device_t * device
) {
	unsigned int i, n = dr_device_num_txchannels(device);

	enum
	{
		ID_FIELD_WIDTH = 2,
		NAME_FIELD_WIDTH = 20,
		FORMAT_FIELD_WIDTH = 10,
		ENABLED_FIELD_WIDTH = 8,
		MUTED_FIELD_WIDTH = 8
	};

	if (dr_device_is_component_stale(device, DR_DEVICE_COMPONENT_TXCHANNELS))
	{
		DR_TEST_PRINT("WARNING: the TX_CHANNELS component has been marked as stale and needs updating\n");
	}

	DR_TEST_PRINT(" TX Channels for device %s:\n\n", dr_device_get_name(device));
	DR_TEST_PRINT("  %*s %*s %*s %*s %*s\n", 
		-ID_FIELD_WIDTH,      "ID",
		-NAME_FIELD_WIDTH,    "Name",
		-FORMAT_FIELD_WIDTH,  "Format",
		-ENABLED_FIELD_WIDTH, "Enabled",
		-MUTED_FIELD_WIDTH,   "Muted"
	);
	for (i = 0; i < n; i++)
	{
		dr_txchannel_t * txc = dr_device_txchannel_at_index(device, i);
		dante_id_t id = dr_txchannel_get_id(txc);
		if (dr_txchannel_is_stale(txc))
		{
			DR_TEST_PRINT("  %*d %*s %*s %*s %*s\n",
				-ID_FIELD_WIDTH,      id,
				-NAME_FIELD_WIDTH,    "?",
				-FORMAT_FIELD_WIDTH,  "?",
				-ENABLED_FIELD_WIDTH, "?",
				-MUTED_FIELD_WIDTH,   "?"
			);
		}
		else
		{
			
			const char * name = dr_txchannel_get_canonical_name(txc);
			char format[512];
			aud_bool_t enabled = dr_txchannel_is_enabled(txc);
			aud_bool_t muted = dr_txchannel_is_muted(txc);

			if (DR_TEST_PRINT_LEGACY_FORMATS)
			{
				dante_samplerate_t samplerate = dr_txchannel_get_sample_rate(txc);
				uint16_t e, num_encodings = dr_txchannel_num_encodings(txc);
				dante_encoding_t encodings[DR_TEST_MAX_ENCODINGS];
				for (e = 0; e < num_encodings && e < DR_TEST_MAX_ENCODINGS; e++)
				{
					encodings[e] = dr_txchannel_encoding_at_index(txc, e);
				}
				dr_test_print_legacy_channel_formats(samplerate, num_encodings, encodings, format, sizeof(format));
			}
			else
			{
				dr_test_print_formats(dr_txchannel_get_formats(txc), format, sizeof(format));
			}
			
			DR_TEST_PRINT("  %*d %*s %*s %*s %*s\n",
				-ID_FIELD_WIDTH,      id,
				-NAME_FIELD_WIDTH,    name,
				-FORMAT_FIELD_WIDTH,  format,
				-ENABLED_FIELD_WIDTH, (enabled ? "true" : "false"),
				-MUTED_FIELD_WIDTH,   (muted ? "true" : "false")
			);
		}
	}
}

void
dr_test_print_device_rxchannels
(
	dr_device_t * device
) {
	unsigned int i, n = dr_device_num_rxchannels(device);

	enum
	{
		ID_FIELD_WIDTH = 2,
		NAME_FIELD_WIDTH = 20,
		FORMAT_FIELD_WIDTH = 10,
		LATENCY_FIELD_WIDTH = 8,
		MUTED_FIELD_WIDTH = 8,
		SUBSCRIPTION_FIELD_WIDTH = 31,
		STATUS_FIELD_WIDTH = 20,
		FLOW_FIELD_WIDTH = 8,
	};

	if (dr_device_is_component_stale(device, DR_DEVICE_COMPONENT_RXCHANNELS))
	{
		DR_TEST_PRINT("WARNING: the RX_CHANNELS component has been marked as stale and needs updating\n");
	}
	
	DR_TEST_PRINT(" RX Channels for device %s:\n\n", dr_device_get_name(device));
	DR_TEST_PRINT("  %*s %*s %*s %*s %*s %*s %*s %*s\n",
		-ID_FIELD_WIDTH,           "ID", 
		-NAME_FIELD_WIDTH,         "Name",
		-FORMAT_FIELD_WIDTH,       "Format",
		-LATENCY_FIELD_WIDTH,      "Latency",
		-MUTED_FIELD_WIDTH,        "Muted",
		-SUBSCRIPTION_FIELD_WIDTH, "Subscription",
		-STATUS_FIELD_WIDTH,       "Status",
		-FLOW_FIELD_WIDTH,         "Flow ID"
	);

	for (i = 0; i < n; i++)
	{
		dr_rxchannel_t * rxc = dr_device_rxchannel_at_index(device, i);
		dante_id_t id = dr_rxchannel_get_id(rxc);
		if (dr_rxchannel_is_stale(rxc))
		{
			DR_TEST_PRINT("  %*d %*s %*s %*s %*s %*s %*s %*s\n",
				-ID_FIELD_WIDTH,           id,
				-NAME_FIELD_WIDTH,         "?",
				-FORMAT_FIELD_WIDTH,       "?",
				-LATENCY_FIELD_WIDTH,      "?",
				-MUTED_FIELD_WIDTH,        "?",
				-SUBSCRIPTION_FIELD_WIDTH, "?",
				-STATUS_FIELD_WIDTH,       "?",
				-FLOW_FIELD_WIDTH,         "?"
			);
		}
		else
		{
			const char * name = dr_rxchannel_get_name(rxc);
			char format_buf[512];
			dante_latency_us_t latency = dr_rxchannel_get_subscription_latency_us(rxc); 
			char latency_buf[32];
			aud_bool_t muted = dr_rxchannel_is_muted(rxc);
			const char * sub = dr_rxchannel_get_subscription(rxc);
			dante_rxstatus_t status = dr_rxchannel_get_status(rxc);
			char flow_buf[32];

			if (DR_TEST_PRINT_LEGACY_FORMATS)
			{
				dante_samplerate_t samplerate = dr_rxchannel_get_sample_rate(rxc);
				uint16_t e, num_encodings = dr_rxchannel_num_encodings(rxc);
				dante_encoding_t encodings[DR_TEST_MAX_ENCODINGS];
				for (e = 0; e < num_encodings && e < DR_TEST_MAX_ENCODINGS; e++)
				{
					encodings[e] = dr_rxchannel_encoding_at_index(rxc, e);
				}
				dr_test_print_legacy_channel_formats(samplerate, num_encodings, encodings, format_buf, sizeof(format_buf));
			}
			else
			{
				dr_test_print_formats(dr_rxchannel_get_formats(rxc), format_buf, sizeof(format_buf));
			}
			if (sub)
			{
				SNPRINTF(latency_buf, 32, "%d", latency);
			}
			else
			{
				SNPRINTF(latency_buf, 32, "-");
			}

			if (dr_device_is_component_stale(device, DR_DEVICE_COMPONENT_RXFLOWS))
			{
				SNPRINTF(flow_buf, 32, "?");
			}
			else
			{
				dr_rxflow_t * flow = NULL;
				dante_id_t flow_id;
				aud_error_t result = dr_device_rxflow_with_channel(device, rxc, &flow);
				if (result == AUD_SUCCESS)
				{
					result = dr_rxflow_get_id(flow, &flow_id);
					if (result == AUD_SUCCESS)
					{
						SNPRINTF(flow_buf, 32, "%d", flow_id);
					}
					else
					{
						DR_TEST_ERROR("Error getting flow for channel '%s': %s\n",
							name, dr_error_message(result, g_test_errbuf));
						SNPRINTF(flow_buf, 32, "?");
					}
					dr_rxflow_release(&flow);
				}
				else if (result == AUD_ERR_NOTFOUND)
				{
					SNPRINTF(flow_buf, 32, "-");
				}
				else
				{
					DR_TEST_ERROR("Error getting flow for channel '%s': %s\n",
						name, dr_error_message(result, g_test_errbuf));
					SNPRINTF(flow_buf, 32, "?");
				}
			}
			

			DR_TEST_PRINT("  %*d %*s %*s %*s %*s %*s %*s %*s\n",
				-ID_FIELD_WIDTH,           id,
				-NAME_FIELD_WIDTH,         name,
				-FORMAT_FIELD_WIDTH,       format_buf,
				-LATENCY_FIELD_WIDTH,      latency_buf,
				-MUTED_FIELD_WIDTH,        (muted ? "true" : "false"),
				-SUBSCRIPTION_FIELD_WIDTH, (sub ? sub : "-"),
				-STATUS_FIELD_WIDTH,       ((status != DANTE_RXSTATUS_NONE) ? dante_rxstatus_to_string(status) : "-"),
				-FLOW_FIELD_WIDTH,         flow_buf
			);
		}
	}
}

dr_txlabel_t g_test_labels[DR_TEST_MAX_TXLABELS];

void
dr_test_print_channel_txlabels
(
	dr_device_t * device,
	dante_id_t channel_id
) {
	aud_error_t result;
	uint16_t c, num_txlabels = DR_TEST_MAX_TXLABELS;
	dr_txchannel_t * txc = NULL;
	if (channel_id == 0)
	{
		unsigned int n = dr_device_num_txchannels(device);
		if (dr_device_is_component_stale(device, DR_DEVICE_COMPONENT_TXLABELS))
		{
			DR_TEST_PRINT("WARNING: this component has been marked as stale and needs updating\n");
		}
		for (c = 0; c < n; c++)
		{
			dr_txchannel_t * txc = dr_device_txchannel_at_index(device, c);
			dante_id_t curr_channel_id = dr_txchannel_get_id(txc);
			if (curr_channel_id)
			{
				dr_test_print_channel_txlabels(device, curr_channel_id);
			}
			else
			{
				DR_TEST_PRINT("NO DATA for channel %u\n", c+1);
			}
		}
		return;
	}

	txc = dr_device_txchannel_with_id(device, channel_id);
	result = dr_txchannel_get_txlabels(txc, &num_txlabels, g_test_labels);
	if (result != AUD_SUCCESS)
	{
		DR_TEST_ERROR("Error listing TX labels for channel with id %u: %s\n",
			channel_id, dr_error_message(result, g_test_errbuf));
		return;
	}
	if (num_txlabels > DR_TEST_MAX_TXLABELS)
	{
		DR_TEST_PRINT("Warning: only allocated space to list %u of %u labels!\n",
			num_txlabels, DR_TEST_MAX_TXLABELS);
	}

	// now print...
	DR_TEST_PRINT("%3u: \"%s\"", channel_id, dr_txchannel_get_canonical_name(txc));
	for (c = 0; c < num_txlabels; c++)
	{
		DR_TEST_PRINT(" \"%s\"", g_test_labels[c].name);
	}
	DR_TEST_PRINT("\n");
}


void
dr_test_print_device_txlabels
(
	dr_device_t * device,
	dante_id_t label_id,
	aud_bool_t brief
) {
	aud_error_t result;
	uint16_t i;
	dr_txlabel_t label;

	if (label_id == 0)
	{
		uint16_t n;
		result = dr_device_max_txlabels(device, &n);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Cannot get TX label range: %s\n",
				dr_error_message(result, g_test_errbuf)
			);
			return;
		}
		if (dr_device_is_component_stale(device, DR_DEVICE_COMPONENT_TXLABELS))
		{
			DR_TEST_PRINT("WARNING: this component has been marked as stale and needs updating\n");
		}
		for (i = 0; i < n; i++)
		{
			dr_test_print_device_txlabels(device, i+1, brief);
		}
		return;
	}

	result = dr_device_txlabel_with_id(device, label_id, &label);
	if (result != AUD_SUCCESS)
	{
		if (result == AUD_ERR_NOTFOUND)
		{
			if (! brief)
			{
				DR_TEST_PRINT("%3u: <none>\n", label_id);
			}
			return;
		}
		else
		{
			DR_TEST_ERROR("Error displaying TX label with id %u: %s\n",
				label_id, dr_error_message(result, g_test_errbuf)
			);
			return;
		}
	}

	// now print...
	DR_TEST_PRINT("%3u: \"%s\" -> %u \"%s\"\n"
		, label.id, label.name
		, dr_txchannel_get_id(label.tx), dr_txchannel_get_canonical_name(label.tx)
	);
}


void
dr_test_print_device_txflows
(
	dr_device_t * device
) {
	uint16_t nf, f;
	aud_error_t result;
	unsigned int nintf;
	int addr_field_width;
	
	enum
	{
		ID_FIELD_WIDTH = 2,
		NAME_FIELD_WIDTH = 16, // max is 32 but we wantto fit on the screen
		FLAG_FIELD_WIDTH = 8,
		FORMAT_FIELD_WIDTH = 10,
		LATENCY_FIELD_WIDTH = 8,
		FPP_FIELD_WIDTH = 4,
		ADDRESS_FIELD_WIDTH = 20, // per interface on device
		CHANNEL_FIELD_WIDTH = 25,
		DEST_FIELD_WIDTH = 25 // max is 64 (32+32) but most names will be much shorter
	};
	
	if (dr_device_is_component_stale(device, DR_DEVICE_COMPONENT_TXFLOWS))
	{
		DR_TEST_PRINT("WARNING: this component has been marked as stale and needs updating\n");
	}
	nintf = dr_device_num_interfaces(device);
	addr_field_width = ADDRESS_FIELD_WIDTH * nintf;

	DR_TEST_PRINT(" TX flows for device %s:\n\n", dr_device_get_name(device));
	DR_TEST_PRINT("  %*s %*s %*s %*s %*s %*s %*s %*s %*s\n",
		-ID_FIELD_WIDTH,      "ID",
		-NAME_FIELD_WIDTH,    "Name",
		-FLAG_FIELD_WIDTH,    "Flags",
		-FORMAT_FIELD_WIDTH,  "Format",
		-LATENCY_FIELD_WIDTH, "Latency",
		-FPP_FIELD_WIDTH,     "Fpp",
		-addr_field_width,    "Addresses",
		-DEST_FIELD_WIDTH,    "Destination",
		-CHANNEL_FIELD_WIDTH, "Channels"
	);

	nf = dr_device_num_txflows(device);
	for (f = 0; f < nf; f++)
	{
		aud_bool_t manual;
		dante_id_t id;
		uint16_t i, num_slots, num_interfaces;
		char * name = NULL;
		dante_samplerate_t samplerate;
		dante_encoding_t encoding;
		dante_latency_us_t latency;
		dante_fpp_t fpp;
		dante_ipv4_address_t addresses[DR_TEST_MAX_INTERFACES];
		char * dest_device;
		char * dest_flow;

		char format_buf[512];
		char address_buf[512];
		char dest_buf[DANTE_NAME_LENGTH*2];

		dr_txflow_t * flow = NULL;
		result = dr_device_txflow_at_index(device, f, &flow);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow info: %s\n",
				dr_error_message(result, g_test_errbuf));
			return;
		}
		result = dr_txflow_get_id(flow, &id);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow id: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		result = dr_txflow_get_name(flow, &name);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow name: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		result = dr_txflow_is_manual(flow, &manual);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow 'manual' flag: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		result = dr_txflow_get_format(flow, &samplerate, &encoding);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow format: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		result = dr_txflow_get_latency_us(flow, &latency);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow latency: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		result = dr_txflow_get_fpp(flow, &fpp);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow fpp: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		result = dr_txflow_num_slots(flow, &num_slots);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow slot count: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		result = dr_txflow_num_interfaces(flow, &num_interfaces);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_PRINT("Error getting flow interface count: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		for (i = 0; i < num_interfaces; i++)
		{
			result = dr_txflow_address_at_index(flow, i, addresses+i);
			if (result != AUD_SUCCESS)
			{
				DR_TEST_ERROR (" Error getting address %u: %s\n",
					i, dr_error_message(result, g_test_errbuf));
				dr_txflow_release(&flow);
				return;
			}
		}
		result = dr_txflow_get_destination(flow, &dest_device, &dest_flow);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_PRINT("Error getting flow destination: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_txflow_release(&flow);
			return;
		}
		if (dest_device[0] || dest_flow[0])
		{
			SNPRINTF(dest_buf, sizeof(dest_buf), "%s@%s",
				dest_flow, dest_device
			);
		}
		else
		{
			dest_buf[0] = 0;
		}

		SNPRINTF(format_buf, sizeof(format_buf), "%d/%d", samplerate, encoding);
		dr_test_print_addresses(num_interfaces, addresses, address_buf, 512);

		DR_TEST_PRINT("  %*d %*s %*s %*s %*u %*u %*s %*s",
			-ID_FIELD_WIDTH, id,
			-NAME_FIELD_WIDTH, (name ? name : ""),
			-FLAG_FIELD_WIDTH, (manual ? "manual" : "auto"),
			-FORMAT_FIELD_WIDTH, format_buf,
			-LATENCY_FIELD_WIDTH, latency,
			-FPP_FIELD_WIDTH, fpp,
			-addr_field_width, address_buf,
			-DEST_FIELD_WIDTH, dest_buf
		);
		
		for (i = 0; i < num_slots; i++)
		{
			dr_txchannel_t * tx;
			result = dr_txflow_channel_at_slot(flow, i, &tx);
			if (result != AUD_SUCCESS)
			{
				DR_TEST_ERROR(" Error getting slot %d: %s\n",
					i, dr_error_message(result, g_test_errbuf));
				dr_txflow_release(&flow);
				return;
			}
			DR_TEST_PRINT("%s%s",
				(i ? "," : " "),
				(tx ? dr_txchannel_get_canonical_name(tx) : "_"));
		}
		DR_TEST_PRINT ("\n");

		dr_txflow_release(&flow);
	}
}

void
dr_test_print_device_rxflows
(
	dr_device_t * device
) {
	uint16_t nf, f;
	aud_error_t result;
	
	enum
	{
		ID_FIELD_WIDTH = 2,
		NAME_FIELD_WIDTH = 16, // max is actually 32 but we want to fit on the screen...
		TYPE_FIELD_WIDTH = 10,
		SOURCE_FIELD_WIDTH = 20,   // max is actually 64 but we want to fit on the screen...
		FORMAT_FIELD_WIDTH = 10,
		ADDRESS_FIELD_WIDTH = 20,
		CONNECTIONS_FIELD_WIDTH = 10,
		LATENCY_FIELD_WIDTH = 10,
		CHANNEL_FIELD_WIDTH = 40,
	};
	
	if (dr_device_is_component_stale(device, DR_DEVICE_COMPONENT_RXFLOWS))
	{
		DR_TEST_PRINT("WARNING: this component has been marked as stale and needs updating\n");
	}
	DR_TEST_PRINT(" RX flows for device %s:\n\n", dr_device_get_name(device));
	DR_TEST_PRINT("  %*s %*s %*s %*s %*s %*s %*s %*s %*s\n",
		-ID_FIELD_WIDTH, "ID",
		-NAME_FIELD_WIDTH, "Name",
		-TYPE_FIELD_WIDTH, "Type",
		-SOURCE_FIELD_WIDTH, "Source",
		-FORMAT_FIELD_WIDTH, "Format",
		-ADDRESS_FIELD_WIDTH, "Addresses",
		-CONNECTIONS_FIELD_WIDTH, "Active", 
		-LATENCY_FIELD_WIDTH, "Latency", 
		-CHANNEL_FIELD_WIDTH, "Channels"
	);

	nf = dr_device_num_rxflows(device);
	for (f = 0; f < nf; f++)
	{
		aud_bool_t manual, multicast, unicast;
		dante_id_t id;
		uint16_t i, num_interfaces, num_slots;
		dante_samplerate_t samplerate;
		dante_encoding_t encoding;
		dante_ipv4_address_t addresses[DR_TEST_MAX_INTERFACES];
		dante_latency_us_t latency_us;
		uint16_t connections_active;
		char * name;
		char * tx_device_name;
		char * tx_flow_name;
		char source[DANTE_NAME_LENGTH*2];
		char format_buf[512];
		char address_buf[512];
		char connections_buf[512];

		dr_rxflow_t * flow = NULL;
		result = dr_device_rxflow_at_index(device, f, &flow);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow info: %s\n",
				dr_error_message(result, g_test_errbuf));
			return;
		}
		result = dr_rxflow_get_id(flow, &id);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow id: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}

		result = dr_rxflow_get_name(flow, &name);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR (" Error getting name: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}

		result = dr_rxflow_is_manual(flow, &manual);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow 'manual' setting: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}
		result = dr_rxflow_is_multicast_template(flow, &multicast);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow 'multicast template' setting: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}
		result = dr_rxflow_is_unicast_template(flow, &unicast);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow 'unicast template' setting: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}
		result = dr_rxflow_get_format(flow, &samplerate, &encoding);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow format: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}
		result = dr_rxflow_num_interfaces(flow, &num_interfaces);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_PRINT("Error getting flow interface count: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}

		for (i = 0; i < num_interfaces; i++)
		{
			result = dr_rxflow_address_at_index(flow, i, addresses+i);
			if (result != AUD_SUCCESS)
			{
				DR_TEST_ERROR (" Error getting address %u: %s\n",
					i, dr_error_message(result, g_test_errbuf));
				dr_rxflow_release(&flow);
				return;
			}
		}

		result = dr_rxflow_get_connections_active(flow, &connections_active);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR (" Error getting active connections: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}
		
		result = dr_rxflow_get_latency_us(flow, &latency_us);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR (" Error getting latency: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}

		result = dr_rxflow_get_tx_device_name(flow, &tx_device_name);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR (" Error getting tx device name: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}

		result = dr_rxflow_get_tx_flow_name(flow, &tx_flow_name);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR (" Error getting tx flow name: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}

		if (tx_device_name && tx_flow_name)
		{
			SNPRINTF(source, DANTE_NAME_LENGTH*2, "%s@%s", tx_flow_name, tx_device_name);
		}
		else if (tx_device_name)
		{
			SNPRINTF(source, DANTE_NAME_LENGTH*2, "%s", tx_device_name);
		}
		else
		{
			strcpy(source, "-");
		}
		dr_test_print_connections_active(num_interfaces, connections_active, connections_buf, 512);
		SNPRINTF(format_buf, sizeof(format_buf), "%d/%d", samplerate, encoding);
		dr_test_print_addresses(num_interfaces, addresses, address_buf, 512);

		DR_TEST_PRINT("  %*d %*s %*s %*s %*s %*s %*s %*d",
			-ID_FIELD_WIDTH, id,
			-NAME_FIELD_WIDTH, (name ? name : "-"),
			-TYPE_FIELD_WIDTH, (manual ? "manual" : (multicast ? "multicast" : (unicast ? "unicast" : "auto"))),
			-SOURCE_FIELD_WIDTH, source,
			-FORMAT_FIELD_WIDTH, format_buf,
			-ADDRESS_FIELD_WIDTH, address_buf,
			-CONNECTIONS_FIELD_WIDTH, connections_buf,
			-LATENCY_FIELD_WIDTH, latency_us
		);

		result = dr_rxflow_num_slots(flow, &num_slots);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow slot count: %s\n",
				dr_error_message(result, g_test_errbuf));
			dr_rxflow_release(&flow);
			return;
		}
		if (num_slots)
		{
			for (i = 0; i < num_slots; i++)
			{
				uint16_t j, nj;
				result = dr_rxflow_num_slot_channels(flow, i, &nj);
				if (result != AUD_SUCCESS)
				{
					DR_TEST_ERROR(" Error getting channel count for slot %d: %s\n",
						i, dr_error_message(result, g_test_errbuf));
					dr_rxflow_release(&flow);
					return;
				}
				DR_TEST_PRINT("%s[", (i ? "," : ""));
				for (j = 0; j < nj; j++)
				{
					dr_rxchannel_t * rx;
					result = dr_rxflow_slot_channel_at_index(flow, i, j, &rx);
					if (result != AUD_SUCCESS)
					{
						DR_TEST_ERROR(" Error getting channel %d for slot %d: %s\n",
							j, i, dr_error_message(result, g_test_errbuf));
						dr_rxflow_release(&flow);
						return;
					}
					DR_TEST_PRINT("%s%s",
						(j ? "," : ""),
						(dr_rxchannel_get_name(rx)));
				}
				DR_TEST_PRINT("]");
			}
		}
		else
		{
			DR_TEST_PRINT("-");
		}
		dr_rxflow_release(&flow);

		DR_TEST_PRINT ("\n");
	}
}

const char *
RXFLOW_ERROR_FLAG_STRINGS[] = 
{
	"EARLY_PACKETS",
	"LATE_PACKETS",
	"DROPPED_PACKETS",
	"OUT_OF_ORDER_PACKETS"
};

void
dr_test_print_device_rxflow_errors
(
	dr_device_t * device,
	dante_id_t flow_id
) {
	uint16_t ni, i, f;
	aud_error_t result;

	dr_rxflow_error_flags_t available_rxflow_error_flags 
		= dr_device_available_rxflow_error_flags(device);
	dr_rxflow_error_field_flags_t available_rxflow_error_fields
		= dr_device_available_rxflow_error_fields(device);

	if (flow_id)
	{
		dr_rxflow_t * flow;
		char * flow_name;
		result = dr_device_rxflow_with_id(device, flow_id, &flow);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow with id %d: %s\n",
				flow_id, dr_error_message(result, g_test_errbuf));
			return;
		}
		result = dr_rxflow_get_name(flow, &flow_name);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow interface name: %s\n",
				dr_error_message(result, g_test_errbuf));
			return;
		}

		// print values for one flow...
		DR_TEST_PRINT(" RX flow errors for device %s flow %s:\n\n",
			dr_device_get_name(device), flow_name);

		result = dr_rxflow_num_interfaces(flow, &ni);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting flow interface count: %s\n",
				dr_error_message(result, g_test_errbuf));
			return;
		}

		for (i = 0; i < ni; i++)
		{
			int e;
			dr_rxflow_error_flags_t error_flags;
			
			DR_TEST_PRINT("%d.%d", flow_id, i);

			result = dr_rxflow_get_error_flags(flow, i, &error_flags, NULL);
			if (result != AUD_SUCCESS)
			{
				DR_TEST_ERROR("Error getting flow interface count: %s\n",
					dr_error_message(result, g_test_errbuf));
				return;
			}

			for (e = 0; e < DR_RXFLOW_ERROR_FLAG_COUNT; e++)
			{
				if (available_rxflow_error_flags & (1 << e))
				{
					if (error_flags & (1 << e))
					{
						printf(" %s", RXFLOW_ERROR_FLAG_STRINGS[e]);
					}
				}
			}

			for (f = 0; f < DR_RXFLOW_ERROR_FIELD_COUNT; f++)
			{
				uint32_t value;
				dante_rxflow_error_timestamp_t timestamp;
				if (available_rxflow_error_fields & (1 << f))
				{
					result = dr_rxflow_get_error_field_uint32(flow, i, f, &value, &timestamp);
					if (result != AUD_SUCCESS)
					{
						DR_TEST_ERROR("Error getting flow interface field %d: %s\n",
							f, dr_error_message(result, g_test_errbuf));
						return;
					}
					DR_TEST_PRINT(" %d", value);
				}
			}

			DR_TEST_PRINT ("\n");
		}
	}
	else
	{
		uint16_t max_rxflows;
		uint16_t num_intfs = dr_device_num_interfaces(device);
		uint16_t i, f;
		dante_rxflow_error_flags_t * all_flags;
		uint32_t * all_fields;
		dante_rxflow_error_timestamp_t timestamp;
		//dante_rxflow_error_type_t e, f;

		result = dr_device_max_rxflows(device, &max_rxflows);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting device max rxflows: %s\n",
				dr_error_message(result, g_test_errbuf));
			return;
		}


		DR_TEST_PRINT(" RX flow errors for device %s:\n\n", dr_device_get_name(device));

		// Get and print flags
		result = dr_device_get_rxflow_error_flags(device, &all_flags, &timestamp);
		if (result != AUD_SUCCESS)
		{
			DR_TEST_ERROR("Error getting device rxflow error flags: %s\n",
				dr_error_message(result, g_test_errbuf));
			return;
		}
		DR_TEST_PRINT("Flags: %u.%u:", timestamp.seconds, timestamp.subseconds);
		for (i = 0; i < num_intfs; i++)
		{
			for (f = 0; f < max_rxflows; f++)
			{
				DR_TEST_PRINT(" %04x", all_flags[i*max_rxflows+f]);
			}
		}
		DR_TEST_PRINT("\n");
		
		// Get and print fields
		for (f = 0; f < DR_RXFLOW_ERROR_FIELD_COUNT; f++)
		{
			if (available_rxflow_error_fields & (1 << f))
			{
				result = dr_device_get_rxflow_error_fields(device, f, &all_fields, &timestamp);
				if (result != AUD_SUCCESS)
				{
					DR_TEST_ERROR("Error getting flow interface field %d: %s\n",
						f, dr_error_message(result, g_test_errbuf));
					return;
				}
				DR_TEST_PRINT("%s: %u.%u:", 
					dante_rxflow_error_type_to_string(f), timestamp.seconds, timestamp.subseconds);
				for (i = 0; i < num_intfs; i++)
				{
					for (f = 0; f < max_rxflows; f++)
					{
						DR_TEST_PRINT(" %d", all_fields[i*max_rxflows+f]);
					}
				}
				DR_TEST_PRINT("\n");
			}
		}
	}

}