/*
 * Created  : September 2008
 * Author   : Andrew White <andrew.white@audinate.com>
 * Synopsis : Print various conmon message bodies
 *
 * This software is copyright (c) 2004-2009 Audinate Pty Ltd and/or its licensors
 *
 * Audinate Copyright Header Version 1
 */


//----------
// Include

#include "conmon_examples.h"
#include "conmon_aud_print_msg_internal.h"
#include "dapi_io.h"


//----------
// Types and Constants

static conmon_aud_print_msg_t k_print_table [] =
{
	{
		CONMON_AUDINATE_MESSAGE_TYPE_INTERFACE_STATUS,
		& conmon_aud_print_msg_interface_status,
		"INTERFACE_STATUS",
		"Interface status"
	},
	
	{
		CONMON_AUDINATE_MESSAGE_TYPE_TOPOLOGY_CHANGE,
		NULL,
		"TOPOLOGY_CHANGE"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_SWITCH_VLAN_STATUS,
		& conmon_aud_print_msg_switch_vlan_status,
		"SWITCH_VLAN_STATUS",
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_CLOCKING_STATUS,
		& conmon_aud_print_msg_clocking_status,
		"CLOCKING_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_CLOCKING_CONTROL,
		NULL,
		"CLOCKING_CONTROL"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_UNICAST_CLOCKING_STATUS,
		& conmon_aud_print_msg_unicast_clocking_status,
		"UNICAST_CLOCKING_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_UNICAST_CLOCKING_QUERY,
		NULL,
		"UNICAST_CLOCKING_QUERY"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_NAME_ID_STATUS,
		& conmon_aud_print_msg_name_id,
		"NAME_ID_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_NAME_ID_CONTROL,
		& conmon_aud_print_msg_name_id,
		"NAME_ID_CONTROL"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_MASTER_STATUS,
		& conmon_aud_print_msg_master_status,
		"MASTER_STATUS"
	}, 
	{
		CONMON_AUDINATE_MESSAGE_TYPE_MASTER_QUERY,
		NULL,
		"MASTER_QUERY"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_IDENTIFY_STATUS,
		NULL,
		"IDENTIFY_STATUS"
	}, 
	{
		CONMON_AUDINATE_MESSAGE_TYPE_IDENTIFY_QUERY,
		NULL,
		"IDENTIFY_QUERY"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_SRATE_STATUS,
		& conmon_aud_print_msg_srate_status,
		"SRATE_STATUS"
	}, 
	{
		CONMON_AUDINATE_MESSAGE_TYPE_SRATE_PULLUP_STATUS,
		& conmon_aud_print_msg_srate_pullup_status,
		"SRATE_PULLUP_STATUS"
	}, 
	{
		CONMON_AUDINATE_MESSAGE_TYPE_ENC_STATUS,
		& conmon_aud_print_msg_srate_status,
		"ENC_STATUS"
	}, 
	{
		CONMON_AUDINATE_MESSAGE_TYPE_AUDIO_INTERFACE_STATUS,
		& conmon_aud_print_msg_audio_interface_status,
		"AUDIO_INTERFACE_STATUS"
	}, 
	{
		CONMON_AUDINATE_MESSAGE_TYPE_RX_CHANNEL_CHANGE,
		& conmon_aud_print_msg_idset,
		"RX_CHANNEL_CHANGE"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_RX_CHANNEL_RX_ERROR,
		& conmon_aud_print_msg_idset,
		"RX_CHANNEL_RX_ERROR"
	},
	/*{
		CONMON_AUDINATE_MESSAGE_TYPE_RX_LABEL_CHANGE,
		& conmon_aud_print_msg_idset,
		"RX_LABEL_CHANGE"
	},*/
	
	{
		CONMON_AUDINATE_MESSAGE_TYPE_IFSTATS_STATUS,
		& conmon_aud_print_msg_ifstats_status,
		"IFSTATS_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_IFSTATS_QUERY,
		NULL,
		"IFSTATS_QUERY"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_IGMP_VERS_STATUS,
		& conmon_aud_print_msg_igmp_status,
		"IGMP_VERS_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_IGMP_VERS_CONTROL,
		NULL,
		"IGMP_VERS_CONTROL"
	},
	
	{
		CONMON_AUDINATE_MESSAGE_TYPE_VERSIONS_STATUS,
		& conmon_aud_print_msg_versions_status,
		"VERSIONS_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_VERSIONS_QUERY,
		NULL,
		"VERSIONS_QUERY",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_UPGRADE_STATUS,
		& conmon_aud_print_msg_upgrade_status,
		"UPGRADE_STATUS",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_CLEAR_CONFIG_STATUS,
		& conmon_aud_print_msg_clear_config,
		"CLEAR_CONFIG_STATUS",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_CONFIG_CONTROL,
		& conmon_aud_print_msg_config_control,
		"CONFIG_STATUS",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_SYS_RESET,
		NULL,
		"SYS_RESET",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_EDK_BOARD_STATUS,
		& conmon_aud_print_msg_edk_board_status,
		"EDK_BOARD_STATUS",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_EDK_BOARD_CONTROL,
		NULL,
		"EDK_BOARD_CONTROL",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_ACCESS_STATUS,
		& conmon_aud_print_msg_access_status,
		"ACCESS_STATUS",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_ACCESS_CONTROL,
		NULL,
		"ACCESS_CONTROL",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_MANF_VERSIONS_STATUS,
		& conmon_aud_print_msg_manf_versions_status,
		"MANF_VERSIONS_STATUS"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_MANF_VERSIONS_QUERY,
		NULL,
		"MANF_VERSIONS_QUERY",
		NULL
	},
	
	{
		CONMON_AUDINATE_MESSAGE_TYPE_TX_CHANNEL_CHANGE,
		& conmon_aud_print_msg_idset,
		"TX_CHANNEL_CHANGE"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_TX_LABEL_CHANGE,
		& conmon_aud_print_msg_idset,
		"TX_LABEL_CHANGE"
	},
	
	{
		CONMON_AUDINATE_MESSAGE_TYPE_RX_FLOW_CHANGE,
		& conmon_aud_print_msg_idset,
		"RX_FLOW_CHANGE"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_TX_FLOW_CHANGE,
		& conmon_aud_print_msg_idset,
		"TX_FLOW_CHANGE"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_PROPERTY_CHANGE,
		NULL,
		"PROPERTY_CHANGE"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_RX_FLOW_ERROR_CHANGE,
		NULL, 
		"RX_FLOW_ERROR_CHANGE"
	},

	
	{
		CONMON_AUDINATE_MESSAGE_TYPE_ROUTING_READY_STATUS,
		& conmon_aud_print_msg_routing_ready,
		"ROUTING_READY"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_RX_ERROR_THRES_STATUS,
		& conmon_aud_print_print_rx_error_thres_status,
		"RX_ERROR_THRES_STATUS"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_RX_ERROR_THRES_CONTROL,
		NULL,
		"RX_ERROR_THRES_CONTROL"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_ROUTING_DEVICE_CHANGE,
		NULL,
		"ROUTING_DEVICE_CHANGE"
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_METERING_STATUS,
		& conmon_aud_print_msg_metering_status,
		"METERING_STATUS"
	},
	
	{
		CONMON_AUDINATE_MESSAGE_TYPE_LED_STATUS,
		& conmon_aud_print_msg_led_status,
		"LED_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_LED_QUERY,
		NULL,
		"LED_QUERY",
		NULL
	},

	{
		CONMON_AUDINATE_MESSAGE_TYPE_SERIAL_PORT_STATUS,
		& conmon_aud_print_msg_serial_port_status,
		"SERIAL_PORT_STATUS"
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_SERIAL_PORT_CONTROL,
		NULL,
		"SERIAL_PORT_CONTROL",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_HAREMOTE_STATUS,
		& conmon_aud_print_msg_haremote_status,
		"HAREMOTE_STATUS",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_HAREMOTE_STATS_STATUS,
		& conmon_aud_print_msg_haremote_stats_status,
		"HAREMOTE_STATS_STATUS",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_DANTE_READY,
		& conmon_aud_print_msg_dante_ready,
		"DANTE_READY",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_DANTE_READY_QUERY,
		NULL,
		"DANTE_READY_QUERY",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_PTP_LOGGING_STATUS,
		& conmon_aud_print_msg_ptp_logging_status,
		"PTP_LOGGING_STATUS",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_SYS_RESET_STATUS,
		& conmon_aud_print_msg_sys_reset_status,
		"SYS_RESET_STATUS",
		NULL
	},
	{
		CONMON_AUDINATE_MESSAGE_TYPE_GPIO_STATUS,
		& conmon_aud_print_msg_gpio_status,
		"GPIO_STATUS",
		NULL
	},
	{ 0 }
};


// Inet address buffer

#ifndef INET_ADDRSTRLEN
	#define INET_ADDRSTRLEN 16
#endif

typedef char inet_buf_t [INET_ADDRSTRLEN];


//----------
// Local function prototypes

static void
print_msg_name (const char * name, uint16_t msg_type);


static const conmon_aud_print_msg_t *
info_for_type (conmon_audinate_message_type_t type);


//----------
// Functions

static void
print_msg_name (const char * name, uint16_t msg_type)
{
	printf ("> Audinate message: %s (0x%04x)\n", name, msg_type);
}

// assumes 'type' is in host order...
static const conmon_aud_print_msg_t *
info_for_type (conmon_audinate_message_type_t type)
{
	const conmon_aud_print_msg_t * curr;
	for (curr = k_print_table; curr->typename; curr ++)
	{
		if (curr->type == type)
		{
			return curr;
		}
	}
	
	return NULL;
}

AUD_INLINE const char *
safe_string(const char * str)
{
	return str ? str : "<null>";
}

typedef union in_addr_convert
{
	uint32_t in_addr;
	uint8_t term[4];
} in_addr_convert_t;


aud_bool_t
conmon_aud_print_msg 
(
	const conmon_message_body_t * aud_msg,
	uint16_t body_size
)
{
	conmon_audinate_message_type_t msg_type = conmon_audinate_message_get_type(aud_msg);
	const conmon_aud_print_msg_t * info = info_for_type (msg_type);

	if (info)
	{
		/*if(info->type == CONMON_AUDINATE_MESSAGE_TYPE_VERSIONS_STATUS)
		{
			printf ("> Audinate message: VERSIONS_STATUS\n");
			conmon_aud_print_msg_versions_status(aud_msg, body_size);
		}
		else
		{*/
			print_msg_name (info->typename, msg_type);
			if (info->print)
			{
				info->print (aud_msg, body_size);
				return AUD_TRUE;
			}
		//}
	}
	else
	{
		print_msg_name ("(Unknown message type)", msg_type);
	}
	return AUD_FALSE;
}

static char * interface_flags_to_string(uint16_t flags, char * buf, size_t len)
{
	size_t off = 0;
	off += SNPRINTF(buf+off, len-off, "0x%08x", flags);
	if (flags & CONMON_AUDINATE_INTERFACE_FLAG_UP)
	{
		off += SNPRINTF(buf+off, len-off, " UP");
	}
	if (flags & CONMON_AUDINATE_INTERFACE_FLAG_STATIC)
	{
		off += SNPRINTF(buf+off, len-off, " STATIC");
	}
	if (flags & CONMON_AUDINATE_INTERFACE_FLAG_LINK_LOCAL_DISABLED)
	{
		off += SNPRINTF(buf+off, len-off, " LINK_LOCAL_DISABLED");
	}
	if (flags & CONMON_AUDINATE_INTERFACE_FLAG_LINK_LOCAL_DELAYED)
	{
		off += SNPRINTF(buf+off, len-off, " LINK_LOCAL_DELAYED");
	}
	return buf;
}

static char * ip_address_to_string(uint32_t addr, char * buf, size_t len)
{
	uint8_t * pip = (uint8_t *) &addr;
	SNPRINTF(buf, len, "%d.%d.%d.%d", pip[0], pip[1], pip[2], pip[3]);
	return buf;
}

void
conmon_aud_print_msg_interface_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t p, np = conmon_audinate_interface_status_num_interfaces(aud_msg);
	uint16_t mode = conmon_audinate_interface_status_get_mode(aud_msg);
	conmon_audinate_interfaces_flags_t flags = conmon_audinate_interface_status_get_flags(aud_msg);
	if (mode == CONMON_AUDINATE_INTERFACE_MODE_DIRECT)
	{
		printf(">> mode=DIRECT\n");
	}
	else if (mode == CONMON_AUDINATE_INTERFACE_MODE_SWITCHED)
	{
		printf(">> mode=SWITCHED\n");
	}
	else
	{
		printf(">> mode=%u\n", mode);
	}
	printf(">> flags=0x%08x ", flags);
	if (flags & CONMON_AUDINATE_INTERFACES_SWITCH_REDUNDANCY)
	{
		printf(" SWITCH_REDUNDANCY is on ");
	}
	else
	{
		printf(" SWITCH_REDUNDANCY is off ");
	}
	if (flags & CONMON_AUDINATE_INTERFACES_SWITCH_REDUNDANCY_REBOOT)
	{
		printf("/ SWITCH_REDUNDANCY_REBOOT is on");
	}
	else
	{
		printf("/ SWITCH_REDUNDANCY_REBOOT is off");
	}
	printf("\n");
	printf(">> num ports=%u\n", np);
	for (p = 0; p < np; p++)
	{
		char buf[128];
		const conmon_audinate_interface_t * i = conmon_audinate_interface_status_interface_at_index(aud_msg, p);
		
		uint16_t flags = conmon_audinate_interface_get_flags(i, aud_msg);
		uint32_t link_speed = conmon_audinate_interface_get_link_speed(i, aud_msg);
		const uint8_t * mac = conmon_audinate_interface_get_mac_address(i, aud_msg);
		uint32_t ip = conmon_audinate_interface_get_ip_address(i, aud_msg);
		uint32_t netmask = conmon_audinate_interface_get_netmask(i, aud_msg);
		uint32_t dns = conmon_audinate_interface_get_dns_server(i, aud_msg);
		uint32_t gateway = conmon_audinate_interface_get_gateway(i, aud_msg);
		const char * domain_name = conmon_audinate_interface_status_get_domain_name(i, aud_msg,body_size);
		printf(">>  flags=%s\n", interface_flags_to_string(flags, buf, sizeof(buf)));
		printf(">>  link speed=%u\n", link_speed);
		printf(">>  mac=%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		printf(">>  ip=%s\n", ip_address_to_string(ip, buf, sizeof(buf)));
		printf(">>  netmask=%s\n", ip_address_to_string(netmask, buf, sizeof(buf)));
		printf(">>  dns=%s\n", ip_address_to_string(dns, buf, sizeof(buf)));
		printf(">>  gateway=%s\n", ip_address_to_string(gateway, buf, sizeof(buf)));
		if (domain_name)
		{
		  printf(">>  domain name =%s\n", domain_name);
		}

		if (conmon_audinate_interface_is_reboot_configured(i, aud_msg))
		{
			uint16_t reboot_flags = conmon_audinate_interface_get_reboot_flags(i, aud_msg);
			uint32_t reboot_ip = conmon_audinate_interface_get_reboot_ip_address(i, aud_msg);
			uint32_t reboot_netmask = conmon_audinate_interface_get_reboot_netmask(i, aud_msg);
			uint32_t reboot_dns = conmon_audinate_interface_get_reboot_dns_server(i, aud_msg);
			uint32_t reboot_gateway = conmon_audinate_interface_get_reboot_gateway(i, aud_msg);
			const char * domain_name = conmon_audinate_interface_status_get_reboot_domain_name(i, aud_msg,body_size);
			printf(">>  reboot_config=yes\n");
			printf(">>    flags=%s\n", interface_flags_to_string(reboot_flags, buf, sizeof(buf)));
			printf(">>    ip=%s\n", ip_address_to_string(reboot_ip, buf, sizeof(buf)));
			printf(">>    netmask=%s\n", ip_address_to_string(reboot_netmask, buf, sizeof(buf)));
			printf(">>    dns=%s\n", ip_address_to_string(reboot_dns, buf, sizeof(buf)));
			printf(">>    gateway=%s\n", ip_address_to_string(reboot_gateway, buf, sizeof(buf)));
			if (domain_name)
			{
				printf(">>  domain_name=%s\n", domain_name);
			}
		}
		else
		{
			printf(">>  reboot_config=no\n");
		}
	}
	fflush(stdout);
}

#define SWITCH_VLAN_NAME_COUNT 4

const char * SWITCH_VLAN_NAMES[] = 
{
	"PRIMARY",
	"SECONDARY",
	"USER_2",
	"USER_3"
};

void
conmon_aud_print_msg_switch_vlan_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t current_config = conmon_audinate_switch_vlan_status_get_current_config_id(aud_msg);
	uint16_t reboot_config = conmon_audinate_switch_vlan_status_get_reboot_config_id(aud_msg);
	uint16_t v, max_vlans = conmon_audinate_switch_vlan_status_max_vlans(aud_msg);
	conmon_audinate_switch_vlan_port_mask_t ports_mask = conmon_audinate_switch_vlan_status_get_ports_mask(aud_msg);
	uint16_t c, num_configs = conmon_audinate_switch_vlan_status_num_configs(aud_msg);

	printf(">> current=%u reboot=%u\n", current_config, reboot_config);
	printf(">> max_vlans=%u\n", max_vlans);
	printf(">> ports_mask=0x%08x\n", ports_mask);
	printf(">> num_configs=%u\n", num_configs);
	for (c = 0; c < num_configs; c++)
	{
		const conmon_audinate_switch_vlan_config_t * config 
			= conmon_audinate_switch_vlan_status_config_at_index(aud_msg, c);
		uint16_t id = conmon_audinate_switch_vlan_config_get_id(config, aud_msg);
		const char * name = conmon_audinate_switch_vlan_config_get_name(config, aud_msg);
		printf(">>  id=%d name=%s\n", id, name);
		for (v = 0; v < max_vlans; v++)
		{
			conmon_audinate_switch_vlan_port_mask_t vlan_port_mask = 
				conmon_audinate_switch_vlan_config_get_vlan_port_mask(config, v, aud_msg);
			printf(">>  vlan %s ports=0x%08x\n", 
				SWITCH_VLAN_NAMES[v], vlan_port_mask);
		}
	}	
}

const char * CLOCK_CAPABILITY_NAMES[CONMON_AUDINATE_NUM_CLOCK_CAPABILITIES] = 
{
	"SUBDOMAINS(deprecated)",
	"RESERVED",
	"SUBDOMAIN_NAMES",
	"UNICAST_DELAY_REQUESTS",
	"SRATE PULLUPS HAVE SUBDOMAIN"
};

const char * CLOCK_MUTE_NAMES[CONMON_AUDINATE_NUM_CLOCK_MUTES] =
{
	"NO_SYNC",
	"EXT_CLOCK_UNLOCKED",
	"INT_CLOCK_UNLOCKED"
};

const char * EXT_WCLOCK_STATE_NAMES[CONMON_AUDINATE_NUM_EXTERNAL_WC_STATES] =
{
	"WC_STATE_UNKNOWN",
	"WC_STATE_NONE",
	"WC_STATE_INVALID",
	"WC_STATE_VALID",
	"WC_STATE_MISSING",
	
};

void
conmon_aud_print_msg_clocking_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//const conmon_audinate_clocking_status_t * m =
	//	(const conmon_audinate_clocking_status_t *) aud_msg;
	//const conmon_audinate_port_status_t * ports =
	//	(const conmon_audinate_port_status_t *)
	//		(((uint8_t *) aud_msg) + sizeof(conmon_audinate_clocking_status_t));
	uint16_t i, p, np = conmon_audinate_clocking_status_num_ports(aud_msg);
	conmon_audinate_clock_capabilities_t capabilities = conmon_audinate_clocking_status_get_capabilities(aud_msg);
	conmon_audinate_clock_source_t clock_source = conmon_audinate_clocking_status_get_clock_source(aud_msg);
	conmon_audinate_clock_state_t clock_state = conmon_audinate_clocking_status_get_clock_state(aud_msg);
	conmon_audinate_servo_state_t servo_state = conmon_audinate_clocking_status_get_servo_state(aud_msg);
	uint8_t clock_stratum = conmon_audinate_clocking_status_get_clock_stratum(aud_msg);
	aud_bool_t clock_preferred = conmon_audinate_clocking_status_is_clock_preferred(aud_msg);
	aud_bool_t unicast_delay_requests = conmon_audinate_clocking_status_get_unicast_delay_requests(aud_msg);
	aud_bool_t multicast_ports_enabled = conmon_audinate_clocking_status_get_multicast_ports_enabled(aud_msg);
	aud_bool_t slave_only_enabled = conmon_audinate_clocking_status_get_slave_only_enabled(aud_msg);
	int32_t drift = conmon_audinate_clocking_status_get_drift(aud_msg);
	int32_t max_drift = conmon_audinate_clocking_status_get_max_drift(aud_msg);

	const conmon_audinate_clock_uuid_t * uuid = conmon_audinate_clocking_status_get_uuid(aud_msg);
	const conmon_audinate_clock_uuid_t * muuid = conmon_audinate_clocking_status_get_master_uuid(aud_msg);
	const conmon_audinate_clock_uuid_t * guuid = conmon_audinate_clocking_status_get_grandmaster_uuid(aud_msg);

	const char * subdomain_name = conmon_audinate_clocking_status_get_subdomain_name(aud_msg);
	conmon_audinate_clock_subdomain_t subdomain_index = conmon_audinate_clocking_status_get_subdomain_index(aud_msg);
	uint16_t mute_flags = conmon_audinate_clocking_status_get_mute_flags(aud_msg);
	uint16_t wc_status = conmon_audinate_clocking_status_get_ext_wc_state(aud_msg);
	printf(">> capabilities=0x%04x\n", capabilities);
	for (i = 0; i < 16; i++)
	{
		if (capabilities & (1 << i))
		{
			printf(">>   0x%08x=%s\n", (1 << i), (i < CONMON_AUDINATE_NUM_CLOCK_CAPABILITIES ? CLOCK_CAPABILITY_NAMES[i] : "UNKNOWN"));
		}
	}
	printf(">> source=%s\n", conmon_audinate_clock_source_string(clock_source));
	printf(">> clock state=%s\n", conmon_audinate_clock_state_string(clock_state));
	printf(">> servo state=%s\n", conmon_audinate_servo_state_string(servo_state));
	printf(">> preferred=%s\n", clock_preferred ? "true" : "false");
	printf(">> unicast_delay_requests=%s\n", unicast_delay_requests ? "true" : "false");
	printf(">> multicast_ports_enabled=%s\n", multicast_ports_enabled ? "true" : "false");
	printf(">> stratum=%u\n", clock_stratum);
	printf(">> slave_only=%s\n", slave_only_enabled ? "true" : "false");
	printf(">> drift=%d\n", drift);
	printf(">> max_drift=%d\n", max_drift);
	if (uuid)
	{
		printf(">> uuid=0x%02x%02x%02x%02x%02x%02x\n",
			uuid->data[0], uuid->data[1], uuid->data[2], uuid->data[3], uuid->data[4], uuid->data[5]);
	}
	else
	{
		printf(">> uuid=???\n");
	}
	if (muuid)
	{
		printf(">> master uuid=0x%02x%02x%02x%02x%02x%02x\n",
			muuid->data[0], muuid->data[1], muuid->data[2], muuid->data[3], muuid->data[4], muuid->data[5]);
	}
	else
	{
		printf(">> master uuid=???\n");
	}
	if (guuid)
	{
		printf(">> grandmaster uuid=0x%02x%02x%02x%02x%02x%02x\n",
			guuid->data[0], guuid->data[1], guuid->data[2], guuid->data[3], guuid->data[4], guuid->data[5]);
	}
	else
	{
		printf(">> grandmaster uuid=???\n");
	}
	printf(">> subdomain='%s' (%d)\n", (subdomain_name ? subdomain_name : ""), subdomain_index);

	printf(">> mute_flags=0x%04x\n", mute_flags);
	for (i = 0; i < 16; i++)
	{
		if (mute_flags & (1 << i))
		{
			printf(">>   0x%08x=%s\n", (1 << i), (i < CONMON_AUDINATE_NUM_CLOCK_MUTES ? CLOCK_MUTE_NAMES[i] : "UNKNOWN"));
		}
	}
	printf(">> wc status=%s\n", (wc_status < CONMON_AUDINATE_NUM_EXTERNAL_WC_STATES ? EXT_WCLOCK_STATE_NAMES[wc_status] :"UNKNOWN"));


	printf(">> num ports=%u\n", np);
	for (p = 0; p < np; p++)
	{
		const conmon_audinate_port_status_t * port_status = conmon_audinate_clocking_status_port_at_index(aud_msg, p);
		if (port_status)
		{
			conmon_audinate_port_state_t state = conmon_audinate_port_status_get_port_state(port_status, aud_msg);
			printf(">>   %d: %s\n", p, conmon_audinate_port_state_string(state));
		}
		else
		{
			printf(">>   %d: ???\n", p);
		}
	}
	fflush(stdout);
}

void
conmon_aud_print_msg_unicast_clocking_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//aud_bool_t enabled = conmon_audinate_unicast_clocking_status_is_enabled(aud_msg);
	uint16_t p, np = conmon_audinate_unicast_clocking_status_num_ports(aud_msg);
	printf(">> num ports=%u\n", np);
	for (p = 0; p < np; p++)
	{
		const conmon_audinate_unicast_port_status_t * port_status = conmon_audinate_unicast_clocking_status_port_at_index(aud_msg, p);
		if (port_status)
		{
			uint16_t num_devices = conmon_audinate_unicast_port_status_num_devices(port_status, aud_msg);
			uint16_t max_devices = conmon_audinate_unicast_port_status_max_devices(port_status, aud_msg);
			conmon_audinate_port_state_t state = conmon_audinate_unicast_port_status_get_port_state(port_status, aud_msg);
			printf(">>   %d: devices=%d/%d state=%s\n", p, num_devices, max_devices, conmon_audinate_port_state_string(state));
		}
		else
		{
			printf(">>   %d: ???\n", p);
		}
	}
	fflush(stdout);
}

void
conmon_aud_print_msg_master_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//const conmon_audinate_master_status_t * m =
	//	(const conmon_audinate_master_status_t *) aud_msg;
	//const conmon_audinate_port_status_t * ports =
	//	(const conmon_audinate_port_status_t *)
	//		(((uint8_t *) aud_msg) + sizeof(conmon_audinate_master_status_t));
	uint16_t p, np = conmon_audinate_master_status_num_ports(aud_msg);
	const char * clock_subdomain_name = conmon_audinate_master_status_get_clock_subdomain_name(aud_msg);

	printf(">> clock subdomain name=%s\n", (clock_subdomain_name ? clock_subdomain_name : ""));
	printf(">> num ports=%u\n", np);
	for (p = 0; p < np; p++)
	{
		conmon_audinate_port_state_t port_state = conmon_audinate_master_status_port_state_at_index(aud_msg, p);
		printf(">>   %d: %s\n", p, conmon_audinate_port_state_string(port_state));
	}
}


void
conmon_aud_print_msg_name_id
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t d, nd = conmon_audinate_name_id_num_devices(aud_msg);

	printf(">> num devices=%u\n", nd);
	for (d = 0; d < nd; d++)
	{
		conmon_instance_id_t instance_id;
		const char * name = conmon_audinate_name_id_device_name_at_index(aud_msg, d);
		//const conmon_device_id_t * device_id = conmon_audinate_name_id_device_id_at_index(aud_msg, d);
		//const conmon_process_id_t process_id = conmon_audinate_name_id_process_id_at_index(aud_msg, d);
		const conmon_audinate_clock_uuid_t * uuid = conmon_audinate_name_id_ptp_uuid_at_index(aud_msg, d);

		printf(">>   %d:\n", d);
		if (name)
		{
			printf(">>     name=%s\n", name);
		}
		if (conmon_audinate_name_id_instance_id_at_index(aud_msg, d, &instance_id))
		{
			char id_buf[64];
			printf(">>     instance_id=%s\n", 
				conmon_example_instance_id_to_string(&instance_id, id_buf, sizeof(id_buf)));
		}
		if (uuid)
		{
			printf(">>     uuid=%02x:%02x:%02x:%02x:%02x:%02x\n", 
				uuid->data[0], uuid->data[1], uuid->data[2],
				uuid->data[3], uuid->data[4], uuid->data[5]);
		}
	}
}

void
conmon_aud_print_msg_idset
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
)
{
	//const conmon_audinate_id_set_msg_t * m = (void *) aud_msg;
	//unsigned int count = ntohs(m->count);
//	printf (">> bytes: %u", count);
	unsigned int count = conmon_audinate_id_set_num_elements(aud_msg);
	if (count)
	{
		conmon_audinate_id_t lo = 0;
		conmon_audinate_id_t hi = 0;
			// current id range, base 1
		uint16_t i;
		
		fputs (" =", stdout);
		for (i = 0; i < count; i++)
		{
			//conmon_audinate_id_set_elem_t set = m->set [i];
			conmon_audinate_id_set_elem_t set =
				conmon_audinate_id_set_element_at_index(aud_msg, i);
			printf (" %02x", (unsigned int) set);
		}
		fputs ("\n>>   ids:", stdout);
		
		for (i = 0; i < count; i++)
		{
			unsigned int bit;

			//conmon_audinate_id_set_elem_t set = m->set [i];
			conmon_audinate_id_set_elem_t set =
				conmon_audinate_id_set_element_at_index(aud_msg, i);

			for (bit = 1; bit <= 8; bit ++)
			{
				if (set & 1)
				{
					hi = (conmon_audinate_id_t) (i * 8 + bit);
					if (! lo)
					{
						lo = hi;
						printf (" %u", (unsigned int) lo);
					}
				}
				else
				{
					if (lo)
					{
						if (hi != lo)
						{
							printf ("-%u", (unsigned int) hi);
						}
						lo = 0;
					}
					
					if (! set)
					{
						break;
							// don't bother processing remaining bits
					}
				}
				
				set >>= 1;
			}
		}

		if (lo && lo != hi)
		{
			printf ("-%u", (unsigned int) hi);
		}
	}
	putchar ('\n');
	fflush(stdout);
}

void
conmon_aud_print_msg_bool
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
)
{
	//const conmon_audinate_bool_msg_t * m = (const void *) aud_msg;
	aud_bool_t value = conmon_audinate_bool_msg_get_value(aud_msg);
	puts (
		(value ? ">> true" : ">> false")
	);
}

void
conmon_aud_print_msg_routing_ready
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
)
{
	aud_bool_t ready = conmon_audinate_routing_ready_status_is_ready(aud_msg);
	uint16_t num_txchannels = conmon_audinate_routing_ready_status_num_txchannels(aud_msg);
	uint16_t num_rxchannels = conmon_audinate_routing_ready_status_num_rxchannels(aud_msg);
	uint8_t  link_status = conmon_audinate_routing_ready_status_link(aud_msg);

	printf(">> ready=%s\n", (ready ? "true" : "false"));
	printf(">> num_txchannels=%u\n", num_txchannels);
	printf(">> num_rxchannels=%u\n", num_rxchannels);
	printf(">> link_status=%x\n", link_status);
}

const char * CAPABILITY_NAMES[CONMON_AUDINATE_NUM_CAPABILITIES] =
{
	"CAN_IDENTIFY",
	"CAN_SYS_RESET",
	"HAS_WEBSERVER",
	"CAN_SET_SRATE",

	"CAN_SET_ENCODING",
	"HAS_EDK_BOARD",
	"HAS_TFTPCLIENT",
	"HAS_NO_EXT_WORD_CLOCK",

	"HAS_LEDS",
	"CAN_SET_PULLUPS",
	"HAS_SERIAL_PORTS",
	"HAS_UNICAST_CLOCKING",

	"HAS_MANF_VERSIONS",
	"HAS_SWITCH_REDUNDANCY",
	"HAS_STATIC_IP",
	"HAS_METERING",

	"CAN_DISABLE_LL",
	"HAS_HAREMOTE",
	"HAS_SWITCH_VLAN",
	"HAS_AUDIO_INTERFACE",
	"CAN_SET_PTP_LOGGING",
	"CAN_DELAY_LINK_LOCAL",
	"CAN_CLEAR_CONFIG",
	"CAN_GPIO_CONTROL"
};

const char * DEVICE_STATUS_NAMES[CONMON_AUDINATE_NUM_DEVICE_STATUSES] =
{
	"CAPABILITY_PARTITION_ERROR",
	"USER_PARTITION_ERROR"
};

static const char *
upgrade_version_to_string
(
	unsigned upgrade
)
{
	upgrade >>= 8;
	if (upgrade < 4)
	{
		static const char * upgrade_strings[] =
			{
				"Unknown",
				"Unsupported",
				"Legacy",
				"Version 3"
			};
		return upgrade_strings[upgrade];
	}
	else
		return "Unknown code";
}

static void
print_utf8
(
	const unsigned char * utf8
) {
	int i;
	if (utf8)
	{
		for (i = 0; utf8[i]; i++)
		{
			if (utf8[i] < 0x80)
			{
				printf("%c", (char) utf8[i]);
			}
			else
			{
				printf("(%02x)", utf8[i]);
			}
		}
	}
}

void
conmon_aud_print_msg_versions_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	char buf[BUFSIZ];
	uint16_t i;
	dante_version_t sw_version;
	dante_version_build_t sw_build;

	dante_version_t fw_version;
	dante_version_build_t fw_build;

	dante_version_t api, uboot;
	uint32_t api32 = conmon_audinate_versions_status_get_dante_api_version(aud_msg);
	uint32_t uboot32 = conmon_audinate_versions_status_get_uboot_version(aud_msg);
	unsigned upgrade = conmon_audinate_versions_status_get_upgrade_version(aud_msg);
	uint32_t cap = conmon_audinate_versions_status_get_capability_flags(aud_msg);
	uint32_t icap = conmon_audinate_versions_status_infer_all_capability_flags(aud_msg);
	uint32_t rocap = conmon_audinate_versions_status_get_readonly_capability_flags(aud_msg);
	uint32_t preferred_link_speed = conmon_audinate_versions_status_get_preferred_link_speed(aud_msg);
	const conmon_audinate_model_id_t * model_id 
		= conmon_audinate_versions_status_get_dante_model_id(aud_msg);
	const char * model_name 
		= conmon_audinate_versions_status_get_dante_model_name(aud_msg);
	uint32_t device_status = conmon_audinate_versions_status_get_device_status(aud_msg);
	conmon_audinate_clock_protocol_flags_t clock_protocols = conmon_audinate_versions_status_get_clock_protocols(aud_msg);

	conmon_audinate_versions_status_get_dante_software_version_build(aud_msg, &sw_version, &sw_build);
	conmon_audinate_versions_status_get_dante_firmware_version_build(aud_msg, &fw_version, &fw_build);
	dante_version_from_uint32_8_8_16(api32, &api);
	dante_version_from_uint32_8_8_16(uboot32, &uboot);

	printf(">> dante software version=%u.%u.%u build=%u\n", 
		sw_version.major, sw_version.minor, sw_version.bugfix, sw_build.build_number);
	printf(">> dante firmware version=%u.%u.%u build=%u\n",
		fw_version.major, fw_version.minor, fw_version.bugfix, fw_build.build_number);
	printf(">> dante api version=0x%08x (%u.%u.%u)\n", api32, api.major, api.minor, api.bugfix);
	printf(">> uboot version=0x%08x (%u.%u.%u)\n", uboot32, uboot.major, uboot.minor, uboot.bugfix);
	printf(">> upgrade version=0x%04x (%s)\n", upgrade, upgrade_version_to_string(upgrade));
	printf(">> dante model id=%s\n", conmon_example_model_id_to_string(model_id, buf, BUFSIZ));
	printf(">> dante model name=\""); print_utf8((const unsigned char *) model_name); printf("\"\n");
	printf(">> capabilities=0x%08x\n", cap);
	for (i = 0; i < 32; i++)
	{
		if (cap & (1 << i))
		{
			printf(">>   0x%08x=%s\n", (1 << i), (i < CONMON_AUDINATE_NUM_CAPABILITIES ? CAPABILITY_NAMES[i] : "UNKNOWN"));
		}
	}
	printf(">> inferred capabilities=0x%08x\n", icap);
	for (i = 0; i < 32; i++)
	{
		if (icap & (1 << i))
		{
			printf(">>   0x%08x=%s\n", (1 << i), (i < CONMON_AUDINATE_NUM_CAPABILITIES ? CAPABILITY_NAMES[i] : "UNKNOWN"));
		}
	}
	printf(">> readonly capabilities=0x%08x\n", rocap);
	for (i = 0; i < 32; i++)
	{
		if (rocap & (1 << i))
		{
			// remove negation if printing  EXT_WORD_CLOCK has read-only
			if (i == CONMON_AUDINATE_CAPABILITY_HAS_NO_EXT_WORD_CLOCK)
			{
				printf(">>   0x%08x=EXT_WORD_CLOCK\n", (1 << i));
			}
			else
			{
				printf(">>   0x%08x=%s\n", (1 << i), (i < CONMON_AUDINATE_NUM_CAPABILITIES ? CAPABILITY_NAMES[i] : "UNKNOWN"));
			}
		}
	}
	printf(">> preferred link speed=0x%08x\n", preferred_link_speed);
	printf(">> device status=0x%08x\n", device_status);
	for (i = 0; i < 32; i++)
	{
		if (device_status & (1 << i))
		{
			printf(">>   0x%08x=%s\n", (1 << i), (i < CONMON_AUDINATE_NUM_DEVICE_STATUSES ? DEVICE_STATUS_NAMES[i] : "UNKNOWN"));
		}
	}
	printf(">> clock_protocols=0x%08x\n", clock_protocols);
	fflush(stdout);
}

void
conmon_aud_print_msg_manf_versions_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	char buf[BUFSIZ];

	const conmon_vendor_id_t * manufacturer = conmon_audinate_manf_versions_status_get_manufacturer(aud_msg);
	const char * manufacturer_name = conmon_audinate_manf_versions_status_get_manufacturer_name(aud_msg);
	const conmon_audinate_model_id_t * model_id = conmon_audinate_manf_versions_status_get_model_id(aud_msg);
	const char * model_name = conmon_audinate_manf_versions_status_get_model_name(aud_msg);
	const conmon_device_id_t * serial_id = conmon_audinate_manf_versions_status_get_serial_id(aud_msg);
	dante_version_t model_version, sw_version, fw_version;
	dante_version_build_t sw_build, fw_build;
	uint32_t model_version_raw = conmon_audinate_manf_versions_status_get_model_version(aud_msg);
	uint32_t capabilities = conmon_audinate_manf_versions_status_get_capabilities(aud_msg);
	const char * model_version_string = conmon_audinate_manf_versions_status_get_model_version_string(aud_msg);

	conmon_audinate_manf_versions_status_get_software_version_build(aud_msg, &sw_version, &sw_build);
	conmon_audinate_manf_versions_status_get_firmware_version_build(aud_msg, &fw_version, &fw_build);
	dante_version_from_uint32_8_8_16(model_version_raw, &model_version);

	printf(">> manufacturer=%s\n", conmon_example_vendor_id_to_string(manufacturer, buf, BUFSIZ));
	printf(">> manufacturer name=\""); print_utf8((const unsigned char *) manufacturer_name); printf("\"\n");
	printf(">> model id=%s\n", conmon_example_model_id_to_string(model_id, buf, BUFSIZ));
	printf(">> model name=\""); print_utf8((const unsigned char *) model_name); printf("\"\n");
	printf(">> model version=%u.%u.%u\n", model_version.major, model_version.minor, model_version.bugfix);
	printf(">> model version string=\"%s\"\n", model_version_string ? model_version_string : "");
	printf(">> serial id=%s\n", conmon_example_device_id_to_string(serial_id, buf, BUFSIZ));

	printf(">> software version=%u.%u.%u.%u\n", 
		sw_version.major, sw_version.minor, sw_version.bugfix, sw_build.build_number);
	printf(">> firmware version=%u.%u.%u.%u\n", 
		fw_version.major, fw_version.minor, fw_version.bugfix, fw_build.build_number);
	
	printf(">> capabilities=0x%08x\n", capabilities);

}

void
conmon_aud_print_msg_ifstats_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//int i;
	//const conmon_audinate_ifstats_status_t * m =
	//	(const conmon_audinate_ifstats_status_t *) aud_msg;
	uint16_t i, ni = conmon_audinate_ifstats_status_num_interfaces(aud_msg);
	aud_bool_t first;
	static const char * k_ifstats_capability_name[] =
		{ "Utilization", "Errors", "Clear errors", NULL };

	conmon_audinate_ifstats_capability_t capabilities =
		conmon_audinate_ifstats_status_get_capabilities(aud_msg, body_size);
	printf(">> Capabilities=%04x", (unsigned) capabilities);
	first = AUD_TRUE;
	for(i = 0; k_ifstats_capability_name[i]; i++)
	{
		conmon_audinate_ifstats_capability_t mask = 1 << i;
		if (capabilities & mask)
		{
			fputs((first ? ": " : ", "), stdout);
			first = AUD_FALSE;
			fputs(k_ifstats_capability_name[i], stdout);
		}
	}
	putchar('\n');

	for(i = 0; i < ni; i++)
	{
		uint16_t p, np = conmon_audinate_ifstats_status_num_interface_ports(aud_msg, i);
		printf(">> Dante Interface %d\n", i);
		for (p = 0; p < np; p++)
		{
			const conmon_audinate_ifstats_t * ifstats = 
				conmon_audinate_ifstats_status_interface_port_at_index(aud_msg, i, p);
			uint32_t tx_util = conmon_audinate_ifstats_get_tx_util(ifstats, aud_msg);
			uint32_t rx_util = conmon_audinate_ifstats_get_rx_util(ifstats, aud_msg);
			uint32_t tx_errors = conmon_audinate_ifstats_get_tx_errors(ifstats, aud_msg);
			uint32_t rx_errors = conmon_audinate_ifstats_get_rx_errors(ifstats, aud_msg);
			uint8_t port_type = conmon_audinate_ifstats_get_port_type(ifstats, aud_msg);
			uint8_t port_index = conmon_audinate_ifstats_get_port_type_index(ifstats, aud_msg);
			uint16_t flags = conmon_audinate_ifstats_get_flags(ifstats, aud_msg);
			uint32_t link_speed = conmon_audinate_ifstats_get_link_speed(ifstats, aud_msg);

			printf(">>> Port %d\n", p);
			if (port_type == CONMON_AUDINATE_IFSTATS_PORT_TYPE_DANTE)
			{
				printf(">>>> Id=Dante %u\n", port_index);
			}
			else if (port_type == CONMON_AUDINATE_IFSTATS_PORT_TYPE_PHYSICAL)
			{
				printf(">>>> Id=Physical %u\n", port_index);
			}
			else
			{
				printf(">>>> Id=Unknown Type (%u) %u\n", port_type, port_index);
			}
			printf(">>>> Tx util Kbps=%u\n", (tx_util*8)>>10); 
			printf(">>>> Rx util Kbps=%u\n", (rx_util*8)>>10); 
			printf(">>>> Tx Errors=%u\n", tx_errors);
			printf(">>>> Rx Errors=%u\n", rx_errors);
			printf(">>>> Flags=0x%04x\n", flags);
			printf(">>>> Link Speed=%u\n", link_speed);
		}
	}
	fflush(stdout);
}

#if 0
#define NUM_UPGRADE_STATUS_OLD 5
static const char * UPGRADE_STATUS_NAMES_OLD[] =
{
	"Undef",
	"Start",
	"End-Success",
	"End-Fail",
	"Flash-Start"
};
#endif

#define NUM_UPGRADE_STATUS 5
static const char * UPGRADE_STATUS_NAMES[] =
{
	"None",
	"Get-file",
	"End-Done",
	"End-Fail",
	"Flash-write"
};


void
conmon_aud_print_upgrade_file_info
(
	const conmon_audinate_upgrade_source_file_t * file_info
)
{
	if (file_info->protocol == CONMON_AUDINATE_UPGRADE_PROTOCOL_LOCAL)
	{
		printf(
			">>> Local file: %s\n"
			, safe_string(file_info->filename)
		);
	}
	else
	{
		in_addr_convert_t addr;
		addr.in_addr = file_info->addr_inet.s_addr;

		if (file_info->protocol == CONMON_AUDINATE_UPGRADE_PROTOCOL_TFTP_GET)
		{
			printf(">>> tftp");
		}
		else
		{
			printf(">>> <unknown %u>", file_info->protocol);
		}

		printf(
			"://%u.%u.%u.%u:%u/%s\n"
			, addr.term[0], addr.term[1], addr.term[2], addr.term[3]
			, file_info->port
			, safe_string(file_info->filename)
		);
	}
	
	if (file_info->file_len)
	{
		printf(
			">>> File len = %u bytes\n"
			, file_info->file_len
		);
	}
}


void
conmon_aud_print_id64
(
	const dante_id64_t * src_id
)
{
	dante_id64_str_buf_t id_buf = "";

	fputs(dante_id64_to_str(src_id, id_buf), stdout);
	id_buf[DANTE_ID64_LEN] = 0;
	fputs(" '", stdout);
	fputs(dante_id64_to_ascii(src_id, id_buf), stdout);
	puts("'");
}


void
conmon_aud_print_msg_upgrade_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
)
{
	aud_error_t result;
	conmon_audinate_upgrade_status_t status_info;
	conmon_audinate_upgrade_source_file_t file_info;
	const char * upgrade_status_str;

	result = conmon_audinate_upgrade_status_get_upgrade_status(aud_msg, body_size, &status_info);
	if (result != AUD_SUCCESS)
	{
		printf(">> invalid status message\n");
		return;
	}

	if (status_info.status < NUM_UPGRADE_STATUS)
	{
		upgrade_status_str = UPGRADE_STATUS_NAMES[status_info.status];
	}
	else
	{
		upgrade_status_str = "???";
	}

	printf(
		">> upgrade state = 0x%x (%s)\n"
		">> last error    = 0x%x\n"
		">> progress      = %u of %u\n"
		, status_info.status
		, upgrade_status_str
		, status_info.last_error
		, status_info.progress.curr, status_info.progress.total
	);

	fputs(">> manufacturer  = ", stdout);
	conmon_aud_print_id64(&status_info.manufacturer);
	fputs(">> model         = ", stdout);
	conmon_aud_print_id64(&status_info.model);

	result = conmon_audinate_upgrade_status_get_source_file_info(aud_msg, body_size, &file_info);
	if (result == AUD_SUCCESS)
	{
		conmon_aud_print_upgrade_file_info(&file_info);
	}

	fflush(stdout);
}

#define NUM_CLEAR_CONFIG_NAMES 2
const char * CLEAR_CONFIG_NAMES[] =
{
	"all",
	"keep-ip"
};

void
conmon_aud_print_msg_clear_config
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
)
{
	uint32_t supported, executed;
	int i;
	conmon_audinate_clear_config_status_get_status(aud_msg, body_size, &supported, &executed);

	printf(">> supported     = ");
	for (i=0; i<NUM_CLEAR_CONFIG_NAMES; i++)
		if ((supported>>i) & 0x1)
			printf("%s ",CLEAR_CONFIG_NAMES[i]);
	printf("(0x%x)\n", supported);

	printf(">> executed      = ");
	for (i=0; i<NUM_CLEAR_CONFIG_NAMES; i++)
		if ((executed>>i) & 0x1)
			printf("%s ",CLEAR_CONFIG_NAMES[i]);
	printf("(0x%x)\n", executed);

	fflush(stdout);
}


#define NUM_SRATE_MODES 3
const char * SRATE_MODE_NAMES[] =
{
	"Fixed",
	"Reboot",
	"Immediate"
};

void
conmon_aud_print_msg_srate_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t mode = conmon_audinate_srate_get_mode(aud_msg);
	unsigned long value_curr = conmon_audinate_srate_get_current (aud_msg);
	unsigned long value_next = conmon_audinate_srate_get_new (aud_msg);
	unsigned int avail_count = conmon_audinate_srate_get_available_count (aud_msg);

	printf (">> mode = %lu (%s)\n", (unsigned long) mode, 
		(mode < NUM_SRATE_MODES ? SRATE_MODE_NAMES[mode] : "???"));
	printf (">> current value   = %lu\n", value_curr);
	printf (">> value on reboot = %lu\n", value_next);
	if (avail_count)
	{
		unsigned int i;
		printf (">> available values [%u]:", avail_count);
		for (i = 0; i < avail_count; i++)
		{
			unsigned long rate =
				conmon_audinate_srate_get_available (aud_msg, i);
			printf (" %lu", rate);
		}
		putchar ('\n');
	}
	else
	{
		puts (">> no options available");
	}
	fflush(stdout);
}

void
conmon_aud_print_msg_enc_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t mode = conmon_audinate_enc_get_mode(aud_msg);
	unsigned long value_curr = conmon_audinate_enc_get_current (aud_msg);
	unsigned long value_next = conmon_audinate_enc_get_new (aud_msg);
	unsigned int avail_count = conmon_audinate_enc_get_available_count (aud_msg);

	printf (">> mode = %lu (%s)\n", (unsigned long) mode, 
		(mode < NUM_SRATE_MODES ? SRATE_MODE_NAMES[mode] : "???"));
	printf (">> current value   = %lu\n", value_curr);
	printf (">> value on reboot = %lu\n", value_next);
	if (avail_count)
	{
		unsigned int i;
		printf (">> available values [%u]:", avail_count);
		for (i = 0; i < avail_count; i++)
		{
			unsigned long rate =
				conmon_audinate_enc_get_available (aud_msg, i);
			printf (" %lu", rate);
		}
		putchar ('\n');
	}
	else
	{
		puts (">> no options available");
	}
	fflush(stdout);
}

void
conmon_aud_print_msg_audio_interface_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint8_t chans_per_tdm = conmon_audinate_audio_interface_get_chans_per_tdm(aud_msg);
	uint8_t frame_type = conmon_audinate_audio_interface_get_framing(aud_msg);
	uint8_t align_type = conmon_audinate_audio_interface_get_alignment(aud_msg);
	uint8_t chan_map_type = conmon_audinate_audio_interface_get_channel_mapping(aud_msg);

	printf (">> audio chans per tdm = %hhd\n", chans_per_tdm);
	printf (">> audio framing = %hhd\n", frame_type);
	printf (">> audio alignment = %hhd\n", align_type);
	printf (">> audio channel mapping  = %hhd\n", chan_map_type);
	fflush(stdout);
}

// void
// conmon_aud_print_msg_srate_control
// (
// 	const conmon_message_body_t * aud_msg
// ) {
// }

#define NUM_SRATE_PULLUP_MODES 3
const char * SRATE_PULLUP_MODE_NAMES[] =
{
	"Fixed",
	"Reboot",
	"Immediate"
};

#define NUM_SRATE_PULLUP_NAMES 5
const char * SRATE_PULLUP_NAMES[] =
{
	"None",
	"4.1667%",
	"0.1%",
	"-0.1%",
	"-4.0%",
};

static const char *
srate_pullup_value_to_string
(
	char * buf,
	size_t len,
	uint32_t value
) {
	if (value < NUM_SRATE_PULLUP_NAMES)
	{
		return SRATE_PULLUP_NAMES[value];
	}
	SNPRINTF(buf, len, "UNKNOWN(%u)", value);
	return buf;
}

void
conmon_aud_print_msg_srate_pullup_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	char buf[32];
	uint16_t mode = conmon_audinate_srate_pullup_get_mode(aud_msg);
	uint32_t value_curr = conmon_audinate_srate_pullup_get_current (aud_msg);
	uint32_t value_next = conmon_audinate_srate_pullup_get_new (aud_msg);
	uint16_t avail_count = conmon_audinate_srate_pullup_get_available_count (aud_msg);
	uint32_t flags_known = conmon_audinate_srate_pullup_get_flags_known(aud_msg);
	uint32_t flags = conmon_audinate_srate_pullup_get_flags(aud_msg);
	aud_bool_t has_subdomain = conmon_audinate_srate_pullup_has_subdomain(aud_msg);

	printf (">> mode = %lu (%s)\n", (unsigned long) mode, 
		(mode < NUM_SRATE_PULLUP_MODES ? SRATE_PULLUP_MODE_NAMES[mode] : "???"));
	printf (">> current value   = %s\n", srate_pullup_value_to_string(buf, sizeof(buf), value_curr));
	printf (">> value on reboot = %s\n", srate_pullup_value_to_string(buf, sizeof(buf), value_next));
	printf(">> flags = 0x%04x of 0x%04x\n", flags, flags_known);
	if (avail_count)
	{
		unsigned int i;
		printf (">> available values [%u]:", avail_count);
		for (i = 0; i < avail_count; i++)
		{
			uint32_t pullup =
				conmon_audinate_srate_get_available (aud_msg, i);
			printf(" %s", srate_pullup_value_to_string(buf, sizeof(buf), pullup));
		}
		putchar ('\n');
	}
	else
	{
		puts (">> no options available");
	}
	if (has_subdomain)
	{
		const char * subdomain_name = conmon_audinate_srate_pullup_get_subdomain(aud_msg);
		printf(">> subdomain='%s' \n", (subdomain_name ? subdomain_name : ""));
	}
	fflush(stdout);
}

void
conmon_aud_print_msg_config_control
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//const conmon_audinate_config_status_t * m =
	//	(const conmon_audinate_config_status_t *) aud_msg;
	uint8_t status = conmon_audinate_config_control_get_state(aud_msg);
	printf(">> config state = 0x%x\n", status);
	fflush(stdout);
}

void
conmon_aud_print_msg_access_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//const conmon_audinate_access_status_t * m =
	//	(const conmon_audinate_access_status_t *) aud_msg;
	uint16_t mode = conmon_audinate_access_status_get_mode(aud_msg);
	printf(">> access mode=0x%02x\n", mode);
	fflush(stdout);
}

void
conmon_aud_print_msg_igmp_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//const conmon_audinate_igmp_version_status_t * m =
	//	(const conmon_audinate_igmp_version_status_t *) aud_msg;
	uint16_t version = conmon_audinate_igmp_version_status_get_version(aud_msg);
	printf(">> igmp_version =%u\n", version);
	fflush(stdout);
}

void
conmon_aud_print_msg_edk_board_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	//const conmon_audinate_edk_board_status_t * m =
	//	(const conmon_audinate_edk_board_status_t *) aud_msg;
	uint16_t rev = conmon_audinate_edk_board_status_get_rev(aud_msg);
	uint16_t pad = conmon_audinate_edk_board_status_get_pad(aud_msg);
	uint16_t dig = conmon_audinate_edk_board_status_get_dig(aud_msg);
	uint16_t src = conmon_audinate_edk_board_status_get_src(aud_msg);
	printf(">> board rev=0x%02x\n", rev);
	printf(">> pad =%d\n", -(pad));
	printf(">> dig input=0x%02x\n", dig);
	printf(">> src sync=0x%02x\n", src);
	fflush(stdout);
}

void
conmon_aud_print_print_rx_error_thres_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t thres = conmon_audinate_rx_error_threshold_status_get_threshold(aud_msg);
	uint16_t win = conmon_audinate_rx_error_threshold_status_get_window(aud_msg);
	uint16_t seconds = conmon_audinate_rx_error_threshold_status_get_reset_time(aud_msg);
	printf(">> rx error threshold (samples) %d\n", thres);
	printf(">> rx error window (samples) %d\n", win);
	printf(">> error reset time (seconds) %d\n", seconds);
	fflush(stdout);
}

void
conmon_aud_print_msg_sys_reset_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t mode = conmon_audinate_sys_reset_status_get_mode(aud_msg);
	printf(">> reset mode=0x%02x\n", mode);
	fflush(stdout);
}

const char * GPIO_RESPONSE_TYPES[] =
{
	"QUERY",
	"OUTPUT",
	"INPUT",
	"INTERRUPT"
};

void
conmon_aud_print_msg_gpio_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t i;
	uint16_t field = conmon_audinate_gpio_status_get_fields(aud_msg);
	uint16_t num_state = conmon_audinate_gpio_status_get_state_num(aud_msg);

	uint32_t gpio_trigger_mask, gpio_input_mask, gpio_input_value, gpio_output_mask, gpio_output_value;

	if(!num_state)
		return;

	for(i=0; i<num_state; i++)
	{
		conmon_audinate_gpio_status_state_get_at_index(aud_msg, i, &gpio_trigger_mask, &gpio_input_mask, &gpio_input_value, &gpio_output_mask, &gpio_output_value);

		printf(">> field = 0x%02x (%s)\n", field, (field <= CONMON_AUDINATE_GPIO_STATUS_FIELD_INTERRUPT_STATE)?GPIO_RESPONSE_TYPES[field]:"????");
		printf(">> state num      =0x%02x\n", num_state);
		printf(">> interrupt mask =0x%08x\n", gpio_trigger_mask);
		printf(">> input mask     =0x%08x\n", gpio_input_mask);
		printf(">> input value    =0x%08x\n", gpio_input_value);
		printf(">> output mask    =0x%08x\n", gpio_output_mask);
		printf(">> output value   =0x%08x\n", gpio_output_value);
	}

	fflush(stdout);
}

const char * LED_TYPES[] =
{
	"UNKNOWN",
	"SYSTEM",
	"SYNC",
	"SERIAL",
	"ERROR"
};

#define LED_TYPE_MAX CONMON_AUDINATE_LED_TYPE_ERROR

const char * LED_COLOURS[] = 
{
	"NONE",
	"GREEN",
	"ORANGE"
};

#define LED_COLOUR_MAX CONMON_AUDINATE_LED_COLOUR_ORANGE

const char * LED_STATES[] =
{
	"OFF",
	"ON",
	"BLINK"
};

#define LED_STATE_MAX CONMON_AUDINATE_LED_STATE_BLINK

void
conmon_aud_print_msg_led_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t i, num_leds = conmon_audinate_led_status_num_leds(aud_msg);
	printf(">> num_leds=%u\n", num_leds);
	for (i = 0; i < num_leds; i++)
	{
		conmon_audinate_led_type_t type 
			= conmon_audinate_led_status_led_type_at_index(aud_msg, i);
		conmon_audinate_led_colour_t colour
			= conmon_audinate_led_status_led_colour_at_index(aud_msg, i);
		conmon_audinate_led_state_t state
			= conmon_audinate_led_status_led_state_at_index(aud_msg, i);
		printf(">>  %d: type=%s (0x%04x) colour=%s (0x%04x) state=%s (0x%04x)", i,
			(type <= LED_TYPE_MAX ? LED_TYPES[type] : "???"), type,
			(colour <= LED_COLOUR_MAX ? LED_COLOURS[colour] : "???"), colour,
			(state <= LED_STATE_MAX ? LED_STATES[state] : "???"), state);
	}
	fflush(stdout);
}

void
conmon_aud_print_msg_metering_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint32_t update_rate = conmon_audinate_metering_status_get_update_rate(aud_msg);
	float32_t peak_holdoff = conmon_audinate_metering_status_get_peak_holdoff(aud_msg);
	float32_t peak_decay = conmon_audinate_metering_status_get_peak_decay(aud_msg);
	printf(">> update_rate=%u\n", update_rate);
	printf(">> peak_holdoff=%f\n", peak_holdoff);
	printf(">> peak_decay=%f\n", peak_decay);
	fflush(stdout);
}

const char * SERIAL_PORT_MODES[] =
{
	"UNKNOWN",
	"UNATTACHED",
	"CONSOLE",
	"BRIDGE",
	"METERING"
};

#define SERIAL_PORT_MODE_MAX  CONMON_AUDINATE_SERIAL_PORT_MODE_METERING

const char * SERIAL_PORT_PARITIES[] =
{
	"EVEN",
	"ODD",
	"NONE"
};

#define SERIAL_PORT_PARITY_MAX CONMON_AUDINATE_SERIAL_PORT_PARITY_NONE

void
conmon_aud_print_msg_serial_port_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t i;
	uint16_t num_ports = conmon_audinate_serial_port_status_num_ports(aud_msg);
	uint16_t num_available_baud_rates = conmon_audinate_serial_port_status_num_available_baud_rates(aud_msg);
	uint16_t num_available_bits = conmon_audinate_serial_port_status_num_available_bits(aud_msg);
	uint16_t num_available_parities = conmon_audinate_serial_port_status_num_available_parities(aud_msg);
	uint16_t num_available_stop_bits = conmon_audinate_serial_port_status_num_available_stop_bits(aud_msg);
	printf(">> num_ports=%u\n", num_ports);
	for (i = 0; i < num_ports; i++)
	{
		const conmon_audinate_serial_port_t * serial_port = conmon_audinate_serial_port_status_port_at_index(aud_msg, i);
		conmon_audinate_serial_port_mode_t mode = conmon_audinate_serial_port_get_mode(serial_port);
		conmon_audinate_serial_port_baud_rate_t baud_rate = conmon_audinate_serial_port_get_baud_rate(serial_port);
		conmon_audinate_serial_port_bits_t bits = conmon_audinate_serial_port_get_bits(serial_port);
		conmon_audinate_serial_port_parity_t parity = conmon_audinate_serial_port_get_parity(serial_port);
		conmon_audinate_serial_port_stop_bits_t stop_bits = conmon_audinate_serial_port_get_stop_bits(serial_port);
		aud_bool_t is_configurable = conmon_audinate_serial_port_is_configurable(serial_port);
		printf(">>   %d: mode=%s baud_rate=%d bits=%d parity=%s stop_bits=%d configurable=%s\n",
			i, 
			(mode <= SERIAL_PORT_MODE_MAX ? SERIAL_PORT_MODES[mode] : "???"),
			baud_rate,
			bits,
			(parity <= SERIAL_PORT_PARITY_MAX ? SERIAL_PORT_PARITIES[parity] : "???"),
			stop_bits,
			(is_configurable ? "yes" : "no"));
	}

	printf(">> num_available_baud_rates=%u\n", num_available_baud_rates);
	for (i = 0; i < num_available_baud_rates; i++)
	{
		conmon_audinate_serial_port_baud_rate_t baud_rate = 
			conmon_audinate_serial_port_status_available_baud_rate_at_index(aud_msg, i);
		printf(" >>   %d\n", baud_rate);
	}
	printf(">> num_available_bits=%u\n", num_available_bits);
	for (i = 0; i < num_available_bits; i++)
	{
		conmon_audinate_serial_port_bits_t bits = 
			conmon_audinate_serial_port_status_available_bits_at_index(aud_msg, i);
		printf(" >>   %d\n", bits);
	}
	printf(">> num_available_parities=%u\n", num_available_parities);
	for (i = 0; i < num_available_parities; i++)
	{
		conmon_audinate_serial_port_parity_t parity = 
			conmon_audinate_serial_port_status_available_parity_at_index(aud_msg, i);
		printf(" >>   %s\n", (parity <= SERIAL_PORT_PARITY_MAX ? SERIAL_PORT_PARITIES[parity] : "???"));
	}
	printf(">> num_available_stop_bits=%u\n", num_available_stop_bits);
	for (i = 0; i < num_available_stop_bits; i++)
	{
		conmon_audinate_serial_port_stop_bits_t stop_bits = 
			conmon_audinate_serial_port_status_available_stop_bits_at_index(aud_msg, i);
		printf(" >>   %d\n", stop_bits);
	}
}

static const char * HAREMOTE_BRIDGE_MODE_STRINGS[] = 
{
	"ALL",
	"NONE",
	"SLOT_DB9",
	"NETWORK_SLOT",
	"NETWORK_DB9"
};

void
conmon_aud_print_msg_haremote_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	conmon_audinate_haremote_bridge_mode_flags_t supported_bridge_modes =
		conmon_audinate_haremote_status_get_supported_bridge_modes(aud_msg);
	conmon_audinate_haremote_bridge_mode_t bridge_mode = 
		conmon_audinate_haremote_status_get_bridge_mode(aud_msg);
	int i;
	printf(">> supported_bridge_modes=0x%08x\n", supported_bridge_modes);
	for (i = 0; i < CONMON_AUDINATE_HAREMOTE_NUM_BRIDGE_MODES; i++)
	{
		if (supported_bridge_modes & (1 << i))
		{
			printf(">>   0x%08x=%s\n", (1 << i), HAREMOTE_BRIDGE_MODE_STRINGS[i]);
		}
	}
	printf(">> bridge_mode=%s\n", HAREMOTE_BRIDGE_MODE_STRINGS[bridge_mode]);
}


void
conmon_aud_print_msg_haremote_stats_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	uint16_t i, num_ports = conmon_audinate_haremote_stats_status_num_ports(aud_msg);
	printf(">> num_ports=%u\n", num_ports);
	for (i = 0; i < num_ports; i++)
	{
		const conmon_audinate_haremote_port_stats_t * port_stats = conmon_audinate_haremote_stats_status_port_stats_at_index(aud_msg, i);
		int port_number = conmon_audinate_haremote_port_stats_get_port_number(port_stats);
		uint32_t num_recv_packets = conmon_audinate_haremote_port_stats_num_recv_packets(port_stats);
		uint32_t num_sent_packets = conmon_audinate_haremote_port_stats_num_sent_packets(port_stats);
		uint32_t num_checksum_fails = conmon_audinate_haremote_port_stats_num_checksum_fails(port_stats);
		uint32_t num_timeouts = conmon_audinate_haremote_port_stats_num_timeouts(port_stats);

		printf(">>   %d: port=%d recv=%u sent=%u checksum=%u timeout=%u\n", i, port_number, num_recv_packets, num_sent_packets, num_checksum_fails, num_timeouts);
	}
}

void
conmon_aud_print_msg_dante_ready
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
)
{
	AUD_UNUSED(aud_msg);

	printf(">> DANTE_READY\n");
}

void
conmon_aud_print_msg_ptp_logging_status
(
	const conmon_message_body_t * aud_msg,
	size_t body_size
) {
	
	aud_bool_t value = conmon_audinate_ptp_logging_network_is_enabled(aud_msg);
	puts (
		(value ? ">> enabled" : ">> disabled")
	);
}

//----------
// Other printing

void
conmon_aud_print_networks
(
	const conmon_networks_t * networks,
	const char * prefix
)
{
	int i = 0;

	if (networks && networks->num_networks)
	{
		if (prefix)
		{
			fputs (prefix, stdout);
		}
		else
		{
			fputs ("[ {", stdout);
		}
		
		for (i = 0; i < networks->num_networks; i++)
		{
			struct in_addr a;
			inet_buf_t in_buf, mask_buf, gw_buf, dns_buf;

			const conmon_network_t * n = networks->networks + i;

			// Convert addresses to strings
			a.s_addr = n->ip_address;
			strcpy (in_buf, inet_ntoa (a));

			a.s_addr = n->netmask;
			strcpy (mask_buf, inet_ntoa (a));

			a.s_addr = n->gateway;
			strcpy (gw_buf, inet_ntoa (a));

			a.s_addr = n->dns_server;
			strcpy (dns_buf, inet_ntoa (a));
			
			// Prefix, if any
			if (i)
			{
				if (prefix)
				{
					putchar ('\n');
					fputs (prefix, stdout);
				}
				else
				{
					fputs ("}, {", stdout);
				}
			}
			
			printf ("%d: %s 0x%08x %d %02x:%02x:%02x:%02x:%02x:%02x addr:%s mask:%s gw:%s dns:%s",
				n->interface_index,
				n->is_up ? "up" : "down", 
				n->flags,
				n->link_speed,
				n->mac_address[0], n->mac_address[1], n->mac_address[2],
				n->mac_address[3], n->mac_address[4], n->mac_address[5],
				in_buf, mask_buf, gw_buf, dns_buf
			);
		}

		if (prefix)
		{
			putchar ('\n');
		}
		else
		{
			fputs ("} ]", stdout);
		}
	}
	else if (! prefix)
	{
		fputs ("[]", stdout);
	}
}

//----------
