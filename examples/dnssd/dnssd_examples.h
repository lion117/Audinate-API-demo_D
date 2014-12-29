#ifndef DNSSD_EXAMPLES
#define DNSSD_EXAMPLES

// include the dapi headers
#include "audinate/dante_api.h"

// include the DNS-SD header (part of the Bonjour SDK) and check the version
#include "dns_sd.h"
#if (_DNS_SD_H+0) < AUD_ENV_MIN_DNSSD_VERSION
#error The Dante API requires a newer version of dns_sd.h. Please update to latest version of the Bonjour SDK.
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <libgen.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <errno.h>


#endif
