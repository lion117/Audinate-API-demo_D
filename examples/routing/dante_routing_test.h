#ifndef _DANTE_ROUTING_TEST_H
#define _DANTE_ROUTING_TEST_H

#include "audinate/dante_api.h"
#include <stdio.h>

#ifdef WIN32
#define SNPRINTF _snprintf
#else
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SNPRINTF snprintf
#endif
#include <ctype.h>

#define DR_TEST_DEBUG printf
#define DR_TEST_PRINT printf
#define DR_TEST_ERROR printf

// constants to simplify printing...
#define DR_TEST_MAX_INTERFACES 2
#define DR_TEST_MAX_ENCODINGS 10
#define DR_TEST_MAX_TXLABELS 128

#define DR_TEST_PRINT_LEGACY_FORMATS 0


extern aud_errbuf_t g_test_errbuf;

//----------------------------------------------------------
// Print functions
//----------------------------------------------------------

void
dr_test_print_sockets
(
	const dante_sockets_t * sockets,
	char * buf,
	size_t len
);

void
dr_test_print_connections_active
(
	uint16_t num_connections,
	uint16_t connections_active,
	char * buf,
	size_t len
);

void
dr_test_print_formats
(
	const dante_formats_t * formats,
	char * buf,
	size_t len
);

void
dr_test_print_legacy_channel_formats
(
	dante_samplerate_t samplerate,
	uint16_t num_encodings,
	dante_encoding_t * encodings,
	char * buf,
	size_t len
);

void
dr_test_print_addresses
(
	uint16_t num_addresses,
	dante_ipv4_address_t * addresses,
	char * buf,
	size_t len
);

void
dr_test_print_device_names
(
	const dr_device_t * device
);

void
dr_test_print_device_info
(
	const dr_device_t * device
);

void
dr_test_print_device_txchannels
(
	dr_device_t * device
);

void
dr_test_print_device_rxchannels
(
	dr_device_t * device
);

void
dr_test_print_device_txlabels
(
	dr_device_t * device,
	dante_id_t label_id,
	aud_bool_t brief
);

void
dr_test_print_channel_txlabels
(
	dr_device_t * device,
	dante_id_t channel_id
);

void
dr_test_print_device_names
(
	const dr_device_t * device
);

void
dr_test_print_device_txflows
(
	dr_device_t * device
); 

void
dr_test_print_device_rxflows
(
	dr_device_t * device
);

void
dr_test_print_device_rxflow_errors
(
	dr_device_t * device,
	dante_id_t flow_id // 0 == print all
);

//----------------------------------------------------------
// Actions functions
//----------------------------------------------------------

#endif

