#ifdef WIN32

#define FD_SETSIZE 2048

#include <winsock2.h>
#include <windows.h>

#define SNPRINTF _snprintf
#define LAST_ERROR GetLastError()

#else

#include <errno.h>
#define INVALID_SOCKET -1
#define SNPRINTF snprintf
#define LAST_ERROR errno

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>

#include "dns_sd.h"

#ifndef MIN
#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#endif

#define MAX_NAME 256
#define MAX_ADVERTS 2048
#define MAX_BROWSE 3
#define MAX_TXT 512

typedef enum brq_interface_filter_mode
{
	BRQ_INTERFACE_FILTER_MODE_NONE,
	BRQ_INTERFACE_FILTER_MODE_PRE,
	BRQ_INTERFACE_FILTER_MODE_POST,
	BRQ_INTERFACE_NUM_FILTER_MODES
} brq_interface_filter_mode_t;

static const brq_interface_filter_mode_t g_browse_interface_filter_mode  = BRQ_INTERFACE_FILTER_MODE_POST;
static const brq_interface_filter_mode_t g_resolve_interface_filter_mode = BRQ_INTERFACE_FILTER_MODE_NONE;
static const brq_interface_filter_mode_t g_query_interface_filter_mode   = BRQ_INTERFACE_FILTER_MODE_NONE;
static int g_interface_index = 0;

static const struct timeval ONE_SECOND = {1,0};

static int g_running = 1;
static int g_getaddrinfo = 0;
static int g_resolve = 0;
static int g_resolve_llq = 0;
static int g_query = 0;

void sig_handler(int sig)
{
	g_running = 0;
	signal(SIGINT, sig_handler);
}

typedef struct brq_ref brq_ref_t;
typedef struct brq_query brq_query_t;
typedef struct brq_resolve brq_resolve_t;
typedef struct brq_advert brq_advert_t;
typedef struct brq_browse brq_browse_t;
typedef struct brq_stats brq_stats_t;

struct brq_ref
{
	DNSServiceRef ref;
	int selected;
};

struct brq_query
{
	brq_ref_t ref;

	int interface_index;
	union
	{
		uint8_t addr8[4];
		uint32_t addr32;
	} _;
};

struct brq_resolve
{
	brq_ref_t ref;

	int interface_index;

	char target[MAX_NAME];
	uint16_t port;

	int txt_len;
	uint8_t txt[MAX_TXT];	
};

struct brq_advert
{
	int interface_index;
	char service_name[MAX_NAME];

	brq_resolve_t resolve;
	brq_query_t query;
};

struct brq_browse
{
	//int interface_index;
	const char * service_type;
	const char * service_domain;
	
	brq_ref_t ref;

	brq_advert_t adverts[MAX_ADVERTS];
};

struct brq_stats
{
	uint32_t num_fds_select;
	uint32_t num_fds_process;
	uint32_t num_browsed;
	uint32_t num_resolving;
	uint32_t num_resolved;
	uint32_t num_querying;
	uint32_t num_queried;
};

static int fds[1024];

static int select_ref(brq_ref_t * ref, fd_set * fds, int n)
{
	if (ref->ref)
	{
		int fd = DNSServiceRefSockFD(ref->ref);
		if (fd != INVALID_SOCKET)
		{
#ifdef WIN32
			if (n < FD_SETSIZE)
			{
				FD_SET(fd, fds);
				ref->selected++;
				n = n+1;
			}
#else
			FD_SET(fd, fds);
			ref->selected++;
			n = MAX(n, fd+1);
#endif
		}
	}
	return n;
}

static int process_ref(brq_ref_t * ref, fd_set * fds, int n)
{
	// are we active?
	if (ref->ref)
	{
		// were we in the select, ie. eligible for processing?
		if (ref->selected)
		{
			int fd;

			ref->selected = 0;
			
			// were we selected?
			fd = DNSServiceRefSockFD(ref->ref);
			if (FD_ISSET(fd, fds))
			{	
				DNSServiceRef curr = ref->ref; // could get deleted so cache!
				int res = DNSServiceProcessResult(curr);
				if (res)
				{
					fprintf(stderr, "%s: PROCESS ERROR: ref=%p res=%d\n", __FUNCTION__, curr, res);
				}
				assert(ref->selected == 0);
				return n + 1;
			}
			else
			{
				// eligble but nothing on socket, which is OK
				assert(ref->selected == 0);
				return n;
			}
		}
		else
		{
			// ref adding during processing, don't process now
			assert(ref->selected == 0);
			return n;
		}
	}
	else
	{
		// make sure we weren't selected
		assert(ref->selected == 0);
		return n;
	}
}

static void cleanup_ref(brq_ref_t * ref)
{
	if (ref->ref)
	{
		DNSServiceRefDeallocate(ref->ref);
		ref->ref = NULL;
	}
	ref->selected = 0;
}


static const char *
sockaddr_to_string
(
	const struct sockaddr * address,
	char * buf,
	size_t len
) {
	if (address->sa_family == AF_INET)
	{
		struct sockaddr_in * in = (struct sockaddr_in *) (address->sa_data);
		uint32_t addr32 = in->sin_addr.s_addr;
		uint8_t * addr8 = (uint8_t *) &addr32;
		SNPRINTF(buf, len, "%u.%u.%u.%u:%u",
			addr8[0], addr8[1], addr8[2], addr8[3],
			ntohs(in->sin_port));
	}
	else
	{
		SNPRINTF(buf, len, "???");
	}
	return buf;
}


static void DNSSD_API
query_callback
(
	DNSServiceRef ref,
	DNSServiceFlags flags,
	uint32_t interface_index,
	DNSServiceErrorType error_code,
	const char * fullname,
	uint16_t rrtype,
	uint16_t rrclass,
	uint16_t rdlen,
	const void * rdata,
	uint32_t ttl,
	void * context
) {
	brq_advert_t * advert = (brq_advert_t *) context;
	brq_query_t * query = &advert->query;
	
	assert(query->ref.ref == ref);

	// not yet implemented!
	assert(g_query_interface_filter_mode != BRQ_INTERFACE_FILTER_MODE_POST);

	if (error_code)
	{
		fprintf(stderr, "%s: QUERY ERROR: error_code=%d\n", __FUNCTION__, error_code);
		return;
	}

	cleanup_ref(&query->ref);

	if (rdlen != 4)
	{
		fprintf(stderr, "%s: QUERY ERROR: rdlen=%d\n", __FUNCTION__, rdlen);
		return;
	}
	memcpy(&query->_.addr32, rdata, 4);
	fprintf(stderr, "%s: QUERIED: %08x %d %s %u.%u.%u.%u\n", __FUNCTION__,
		flags, interface_index, fullname,
		query->_.addr8[0], query->_.addr8[1], query->_.addr8[2], query->_.addr8[3]);		
}

static void DNSSD_API
resolve_common
(
	brq_advert_t * advert,
	const char * full_name,
	const char * host_target,
	uint16_t port,
	uint16_t txt_len,
	const unsigned char * txt_record
) {
	brq_resolve_t * resolve = &advert->resolve;

	resolve->target[0] = '\0';
	resolve->port = 0;
	cleanup_ref(&advert->query.ref);
	
	if (host_target)
	{
		SNPRINTF(resolve->target, MAX_NAME, "%s", host_target);
		resolve->port = port;

		if (g_query)
		{
			int res;
			assert(advert->query.ref.ref == NULL);
			res = DNSServiceQueryRecord(&(advert->query.ref.ref), 0, 				
				(g_browse_interface_filter_mode == BRQ_INTERFACE_FILTER_MODE_PRE ? g_interface_index : kDNSServiceInterfaceIndexAny),
				host_target, kDNSServiceType_A, kDNSServiceClass_IN, query_callback, advert);
			if (res)
			{
				fprintf(stderr, "%s: ERROR querying '%s': %d\n", __FUNCTION__, host_target, res);
				return;
			}
		}
	}

	if (!txt_len || txt_len != resolve->txt_len || !txt_record || memcmp(txt_record, resolve->txt, MAX_TXT))
	{
		resolve->txt_len = 0;
		memset(resolve->txt, 0, MAX_TXT);
	}

	if (txt_len && txt_record)
	{
		resolve->txt_len = txt_len;
		memcpy(resolve->txt, txt_record, MIN(MAX_TXT, txt_len));
	}

	/*fprintf(stderr, "%s: RESOLVED: %08x %d %s %s %d %d %p =>", __FUNCTION__, flags,
		interface_index, full_name,
		host_target, ntohs(port),
		txt_len, txt_record);*/
	fprintf(stderr, "%s: RESOLVED: %s => %s:%d\n", __FUNCTION__,
		full_name, resolve->target, resolve->port);
	{
		uint16_t i, count;
		
		if (resolve->txt_len)
		{
			count = TXTRecordGetCount(resolve->txt_len, resolve->txt);
			for (i = 0; i < count; i++)
			{
				char entry[MAX_TXT];
				char key[MAX_TXT];
				char * value;
				uint8_t tmp, len;
				memset(entry, 0, MAX_TXT);
				TXTRecordGetItemAtIndex(resolve->txt_len, resolve->txt, i, MAX_TXT, key, &len, (const void **) &value);
				SNPRINTF(entry, MAX_TXT, "%s='", key);
				tmp = (uint8_t) strlen(entry);
				memcpy(((char *) entry) + tmp, value, len);
				entry[tmp + len] = '\'';
				entry[tmp + len+1] = '\0';
				fprintf(stderr, " %s", entry);
			}
		}
	}
	fprintf(stderr, "\n");
}

static void DNSSD_API
resolve_callback
(
	DNSServiceRef ref,
	DNSServiceFlags flags,
	uint32_t interface_index,
	DNSServiceErrorType error_code,
	const char * full_name,
	const char * host_target,
	uint16_t port,
	uint16_t txt_len,
	const unsigned char * txt_record,
	void * context
) {
	brq_advert_t * advert = (brq_advert_t *) context;
	brq_resolve_t * resolve = &advert->resolve;

	assert(resolve->ref.ref == ref);

	// not yet implemented!
	assert(g_resolve_interface_filter_mode != BRQ_INTERFACE_FILTER_MODE_POST);

	cleanup_ref(&resolve->ref);

	if (error_code)
	{
		fprintf(stderr, "%s: RESOLVE ERROR: error_code=%d\n", __FUNCTION__, error_code);
		return;
	}

	// check host target
	if (!host_target)
	{
		fprintf(stderr, "%s: RESOLVE ERROR: target is NULL\n", __FUNCTION__);
		return;
	}
	else if (!(host_target[0]))
	{
		fprintf(stderr, "%s: RESOLVE ERROR: target is empty\n", __FUNCTION__);
		return;
	}
	else if (!isprint(host_target[0]))
	{
		fprintf(stderr, "%s: RESOLVE ERROR: target is non-printable\n", __FUNCTION__);
		return;
	}

	resolve_common(advert, full_name, host_target, ntohs(port), txt_len, txt_record);
}

static void DNSSD_API
resolve_llq_callback
(
	DNSServiceRef ref,
	DNSServiceFlags flags,
	uint32_t interface_index,
	DNSServiceErrorType error_code,
	const char * full_name,
	uint16_t rrtype,
	uint16_t rrclass,
	uint16_t rdlen,
	const void * rdata,
	uint32_t ttl,
	void * context
) {
	brq_advert_t * advert = (brq_advert_t *) context;
	brq_resolve_t * resolve = &advert->resolve;
	const uint16_t * data16 = (const uint16_t *) rdata;
	const uint8_t * data8 = (const uint8_t *) rdata;
	const char * host_target_raw = NULL;
	const char * cin, * cend;
	char * cout;
	char host_target[1024];
	uint16_t port = 0;


	assert(resolve->ref.ref == ref);

	// not yet implemented!
	assert(g_resolve_interface_filter_mode != BRQ_INTERFACE_FILTER_MODE_POST);

	// leave it open!
	//cleanup_ref(&resolve->ref);

	if (error_code)
	{
		fprintf(stderr, "%s: RESOLVE LLQ ERROR: error_code=%d\n", __FUNCTION__, error_code);
		return;
	}

	if (rrtype != kDNSServiceType_SRV || rrclass != kDNSServiceClass_IN)
	{
		fprintf(stderr, "%s: RESOLVE LLQ ERROR: rrtype=%d rrclass=%d\n", __FUNCTION__, rrtype, rrclass);
		return;
	}

	if (rdlen < 7)
	{
		fprintf(stderr, "%s: RESOLVE LLQ ERROR: rdlen=%d\n", __FUNCTION__, rdlen);
		return;
	}

	// skip priority and weight
	port = ntohs(data16[2]);
	host_target_raw = data8+6;

	// decode target string
	cin = host_target_raw;
	cout = host_target;
	while (*cin)
	{
		size_t len = *cin;
		cin++;
		cend = cin + len;
		while (cin < cend)
		{
			*cout = *cin;
			cin++;
			cout++;
		}
		*cout = '.';
		cout++;
	}
	*cout = '\0';

	resolve_common(advert, full_name, host_target, port, 0, NULL);
}

static void DNSSD_API
getaddrinfo_callback
(
	DNSServiceRef ref,
	DNSServiceFlags flags,
	uint32_t interface_index,
	DNSServiceErrorType error_code,
	const char * hostname,
	const struct sockaddr * address,
	uint32_t ttl,
	void * context
) {
	brq_advert_t * advert = (brq_advert_t *) context;
	char addr_str[128];

	assert(ref = advert->resolve.ref.ref);
	
	// not yet implemented!
	assert(g_resolve_interface_filter_mode != BRQ_INTERFACE_FILTER_MODE_POST);

	cleanup_ref(&advert->resolve.ref);

	if (error_code)
	{
		fprintf(stderr, "%s: GETADDRINFO ERROR: error_code=%d\n", __FUNCTION__, error_code);
		return;
	}

	sockaddr_to_string(address, addr_str, sizeof(addr_str));

	fprintf(stderr, "%s: GETADDRINFO: %08x %d %s %s %u\n", __FUNCTION__,
		flags, interface_index,
		hostname, addr_str, ttl);

	if (address->sa_family == AF_INET)
	{
		struct sockaddr_in * in = (struct sockaddr_in *) address;
		advert->resolve.port = ntohs(in->sin_port);
		advert->query._.addr32 = in->sin_addr.s_addr;
	}
	else
	{
		advert->resolve.port = 0;
		advert->query._.addr32 = 0;
	}
}

static void DNSSD_API
browse_callback
(
	DNSServiceRef ref,
	DNSServiceFlags flags,
    uint32_t interface_index,
    DNSServiceErrorType error_code,
    const char * service_name,
    const char * service_type,
    const char * service_domain,
    void * context
) {
	int i, res;
	brq_browse_t * browse = (brq_browse_t *) context;
	brq_advert_t * advert = NULL;

	assert (ref == browse->ref.ref);

	if (error_code)
	{
		fprintf(stderr, "%s: BROWSE ERROR: error_code=%d\n", __FUNCTION__, error_code);
		return;
	}
	if (g_browse_interface_filter_mode == BRQ_INTERFACE_FILTER_MODE_POST && interface_index && g_interface_index && interface_index != g_interface_index)
	{
		// ignore result
		return;
	}
	if (flags & kDNSServiceFlagsAdd)
	{	
		fprintf(stderr, "%s: DISCOVERED %d %08x %s %s %s\n", __FUNCTION__,
			interface_index, flags,
			service_name, service_type, service_domain);
		for (i = 0; i < MAX_ADVERTS; i++)
		{
			advert = browse->adverts + i;
			if (!(browse->adverts[i].service_name[0]))
			{
				break;
			}
		}
		if (!advert)
		{
			fprintf(stderr, "%s: BROWSE ERROR: too many adverts!\n", __FUNCTION__);
			return;
		}

		cleanup_ref(&advert->resolve.ref);
		cleanup_ref(&advert->query.ref);
		memset(advert, 0, sizeof(brq_advert_t));

		advert->interface_index = interface_index;
		SNPRINTF(advert->service_name, MAX_NAME, "%s", service_name);

		if (g_getaddrinfo)
		{
			// JHW: this seems to not work on windows w/ a patched 258.13 mDNSResponder...
			char hostname[kDNSServiceMaxDomainName];
			DNSServiceConstructFullName(hostname, advert->service_name, service_type, service_domain);

			res = DNSServiceGetAddrInfo(&(advert->resolve.ref.ref),
				0,
				(g_resolve_interface_filter_mode == BRQ_INTERFACE_FILTER_MODE_PRE ? interface_index : kDNSServiceInterfaceIndexAny),
				0, // kDNSServiceProtocol_IPv4,
				hostname,
				getaddrinfo_callback, advert);
			if (res)
			{
				fprintf(stderr, "%s: ERROR getting address info %s: %d\n", __FUNCTION__,
					hostname, res);
				exit(res);
			}
		}
		else if (g_resolve_llq)
		{
			char hostname[kDNSServiceMaxDomainName];
			DNSServiceConstructFullName(hostname, advert->service_name, service_type, service_domain);

			res = DNSServiceQueryRecord(&(advert->resolve.ref.ref),
				kDNSServiceFlagsLongLivedQuery, 
				(g_resolve_interface_filter_mode == BRQ_INTERFACE_FILTER_MODE_PRE ? interface_index : kDNSServiceInterfaceIndexAny),
				hostname, kDNSServiceType_SRV, kDNSServiceClass_IN,
				resolve_llq_callback, advert);
			if (res)
			{
				fprintf(stderr, "%s: ERROR llq resolving %s: %d\n", __FUNCTION__, hostname, res);
				exit(res);
			}
		}
		else if (g_resolve)
		{
			res = DNSServiceResolve(&(advert->resolve.ref.ref),
				0, 
				(g_resolve_interface_filter_mode == BRQ_INTERFACE_FILTER_MODE_PRE ? interface_index : kDNSServiceInterfaceIndexAny),
				advert->service_name, service_type, service_domain,
				resolve_callback, advert);
			if (res)
			{
				fprintf(stderr, "%s: ERROR resolving %s %s %s: %d\n", __FUNCTION__,
					advert->service_name, service_type, service_domain, res);
				exit(res);
			}
			//fprintf(stderr, "%s: Resolving %s %s %s at %p %p with ref %p\n",
			//	__FUNC__, advert->service_name, regtype, replyDomain, advert, &advert->resolve, advert->resolve.ref);
		}
	}
	else
	{
		fprintf(stderr, "%s: UNDISCOVERED %d %08x %s %s %s\n", __FUNCTION__,
			interface_index, flags,
			service_name, service_type, service_domain);
		for (i = 0; i < MAX_ADVERTS; i++)
		{
			advert = browse->adverts + i;
			if (!strcmp(browse->adverts[i].service_name, service_name))
			{
				break;
			}
		}
		if (!advert)
		{
			fprintf(stderr, "%s: BROWSE ERROR: advert %s not found!\n", __FUNCTION__, service_name);
			return;
		}

		cleanup_ref(&advert->resolve.ref);
		cleanup_ref(&advert->query.ref);
		memset(advert, 0, sizeof(brq_advert_t));
	}
}



void usage(char * bin)
{
	fprintf(stderr, "Usage: %s OPTIONS\n", bin);
	fprintf(stderr, "OPTIONS:\n");
	fprintf(stderr, "-a        Use GetAddrInfo (disables -r and -q)\n");
	fprintf(stderr, "-i=INDEX  Specify interface index\n");
	fprintf(stderr, "-b=TYPE   Browse for advertisements of type TYPE\n");
	fprintf(stderr, "-d=DOMAIN Use a specific DNS domain\n");
	fprintf(stderr, "-r        Resolve advertisement SRV/TXT records\n");
	fprintf(stderr, "-rl       LLQ Resolve advertisement SRV records\n");
	fprintf(stderr, "-q        Query A records\n");
	fprintf(stderr, "-u        Print incomplete operations when system stabilises\n");
	fprintf(stderr, "-x        Exit when system stabilises\n");
	exit(0);
}

static int g_num_browses = 0;
static brq_browse_t g_browses[MAX_BROWSE];

int main(int argc, char * argv[])
{
	int res;
	int a, b;
	int interface_index = kDNSServiceInterfaceIndexAny;
	const char * service_domain = NULL;
	int print_incomplete = 0;
	int exit_stable = 0;

	brq_stats_t curr_stats, last_stats;

	int changes, stable = 0;

	for (b = 0; b < MAX_BROWSE; b++)
	{
		memset(g_browses+b, 0, sizeof(brq_browse_t));
	}
	memset(&last_stats, 0, sizeof(brq_stats_t));

	for (a = 1; a < argc; a++)
	{
		if (!strcmp(argv[a], "-a"))
		{
			g_getaddrinfo = 1;
		}
		else if (!strncmp(argv[a], "-b=", 3) && strlen(argv[a]) > 3)
		{
			if (g_num_browses < MAX_BROWSE)
			{
				g_browses[g_num_browses].service_type = argv[a] + 3;
				g_num_browses++;
			}
		}
		else if (!strncmp(argv[a], "-i=", 3) && strlen(argv[a]) > 3)
		{
			interface_index = atoi(argv[a]+3);
		}
		else if (!strncmp(argv[a], "-d=", 3) && strlen(argv[a]) > 3)
		{
			service_domain = argv[a] + 3;
		}
		else if (!strcmp(argv[a], "-r"))
		{
			g_resolve = 1;
		}
		else if (!strcmp(argv[a], "-rl"))
		{
			g_resolve_llq = 1;
		}
		else if (!strcmp(argv[a], "-q"))
		{
			g_query = 1;
		}
		else if (!strcmp(argv[a], "-u"))
		{
			print_incomplete = 1;
		}
		else if (!strcmp(argv[a], "-x"))
		{
			exit_stable = 1;
		}
		else
		{
			usage(argv[0]);
		}
	}

	if (g_num_browses == 0)
	{
		usage(argv[0]);
	}
	
	for (b = 0; b < g_num_browses; b++)
	{
		brq_browse_t * browse = g_browses + b;

		browse->service_domain = service_domain;
		//browse->interface_index = interface_index;
		assert(browse->ref.ref == NULL);

		res = DNSServiceBrowse(&(browse->ref.ref),
			0,
			(g_browse_interface_filter_mode == BRQ_INTERFACE_FILTER_MODE_PRE ? g_interface_index : kDNSServiceInterfaceIndexAny),
			browse->service_type, browse->service_domain,
			browse_callback, browse);
		if (res)
		{
			fprintf(stderr, "%s: ERROR browsing for '%s.%s': %d\n", __FUNCTION__,
				browse->service_type, browse->service_domain ? browse->service_domain : "", res);
			exit(res);
		}
		fprintf(stderr, "%s: Browsing for '%s.%s'\n", __FUNCTION__,
			browse->service_type, browse->service_domain ? browse->service_domain : "");
	}

	signal(SIGINT, sig_handler);
	while (g_running)
	{
		struct timeval timeout = ONE_SECOND;
		fd_set fds;
		int n_select = 0;
		int n_process = 0;

		FD_ZERO(&fds);

		// Get the currently active fds...
		for (b = 0; b < g_num_browses; b++)
		{	
			brq_browse_t * browse = g_browses + b;
			n_select = select_ref(&browse->ref, &fds, n_select);
			
			for (a = 0; a < MAX_ADVERTS; a++)
			{
				brq_advert_t * advert = browse->adverts + a;
				n_select = select_ref(&advert->resolve.ref, &fds, n_select);
				n_select = select_ref(&advert->query.ref, &fds, n_select);
			}
		}

		if (n_select > 0)
		{
			res = select(n_select, &fds, NULL, NULL, &timeout);
			if (res < 0)
			{
				fprintf(stderr, "%s: select returned %d (%d)\n", __FUNCTION__, res, LAST_ERROR);
				break;
			}
			else if (res >= 0)
			{
				for (b = 0; b < g_num_browses; b++)
				{	
					brq_browse_t * browse = g_browses + b;
					n_process = process_ref(&browse->ref, &fds, n_process);
					
					for (a = 0; a < MAX_ADVERTS; a++)
					{
						brq_advert_t * advert = browse->adverts + a;
						n_process = process_ref(&advert->resolve.ref, &fds, n_process);
						n_process = process_ref(&advert->query.ref, &fds, n_process);
					}
				}
			}
		}
		else
		{
			// nothing to do 
		}

		// update stats
		memset(&curr_stats, 0, sizeof(brq_stats_t));
		curr_stats.num_fds_select = n_select;
		curr_stats.num_fds_process = n_process;

		for (b = 0; b < g_num_browses; b++)
		{
			brq_browse_t * browse = g_browses + b;
			for (a = 0; a < MAX_ADVERTS; a++)
			{
				brq_advert_t * advert = browse->adverts + a;
				if (advert->service_name[0])
				{
					curr_stats.num_browsed++;
					if (g_resolve_llq)
					{
						if (advert->resolve.target[0])
						{
							curr_stats.num_resolved++;
						}
						if (advert->resolve.ref.ref)
						{
							curr_stats.num_resolving++;
						}
					}
					else if (g_resolve)
					{
						if (advert->resolve.target[0] && advert->resolve.txt_len)
						{
							curr_stats.num_resolved++;
						}
						else if (advert->resolve.ref.ref)
						{
							curr_stats.num_resolving++;
						}
					}
					if (advert->query._.addr32)
					{
						curr_stats.num_queried++;
					}
					else if (advert->query.ref.ref)
					{
						curr_stats.num_querying++;
					}
				}
			}
		}
		changes = 0;
		if (memcmp(&curr_stats, &last_stats, sizeof(brq_stats_t)))
		{
			changes = 1;
			last_stats = curr_stats;
			if (g_getaddrinfo)
			{
				fprintf(stderr, "%s: STATS s=%d,%d  b=%d  a=%d,%d\n", __FUNCTION__,
					curr_stats.num_fds_select, curr_stats.num_fds_process,
					curr_stats.num_browsed,
					curr_stats.num_resolving, curr_stats.num_resolved);
			}
			else
			{
				fprintf(stderr, "%s: STATS s=%d,%d  b=%d  r=%d,%d  q=%d,%d\n", __FUNCTION__,
					curr_stats.num_fds_select, curr_stats.num_fds_process,
					curr_stats.num_browsed,
					curr_stats.num_resolving, curr_stats.num_resolved,
					curr_stats.num_querying, curr_stats.num_queried);
			}
			fprintf(stderr, "\n");
		}

		if (changes)
		{
			stable = 0;
		}
		else
		{
			stable++;
		}
		if (stable == 1)
		{
			if (print_incomplete)
			{
				for (b = 0; b < g_num_browses; b++)
				{
					brq_browse_t * browse = g_browses + b;
					for (a = 0; a < MAX_ADVERTS; a++)
					{
						brq_advert_t * advert = browse->adverts + a;
						if (g_getaddrinfo)
						{
							if (advert->resolve.ref.ref)
							{
								fprintf(stderr, "%s: GETTINGADDRINFO: %s.%s.%s\n", __FUNCTION__,
									advert->service_name,
									browse->service_type,
									browse->service_domain ? browse->service_domain : "");
							}
						}
						else if (g_resolve_llq)
						{
							if (advert->resolve.ref.ref && !advert->resolve.target[0])
							{
								fprintf(stderr, "%s: UNRESOLVED: %s.%s.%s\n", __FUNCTION__,
									advert->service_name,
									browse->service_type,
									browse->service_domain ? browse->service_domain : "");
							}
						}
						else if (g_resolve)
						{
							if (advert->resolve.ref.ref)
							{
								fprintf(stderr, "%s: UNRESOLVED: %s.%s.%s\n", __FUNCTION__,
									advert->service_name,
									browse->service_type,
									browse->service_domain ? browse->service_domain : "");
							}
						}

						if (g_query)
						{
							if (advert->query.ref.ref)
							{
								fprintf(stderr, "%s: UNQUERIED: %s\n", __FUNCTION__, advert->resolve.target);
							}
						}
					}
				}	
			}
			if (exit_stable)
			{
				break;
			}
		}
	}

	// TODO: cleanup
	
	return 0;
}