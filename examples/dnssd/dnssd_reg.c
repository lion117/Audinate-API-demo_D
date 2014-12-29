#include "dnssd_examples.h"

typedef struct runtime
{
	int running;
	aud_bool_t status;
	
	DNSServiceRef sdRef;
	unsigned int port;
	const char * service_type;
	
	fd_set rfd;
	int fd_max;
} runtime_t;



static aud_bool_t
run_loop (runtime_t * r)
{
	r->running = AUD_TRUE;
	r->status = AUD_TRUE;

	while (r->running)
	{
		fd_set r_curr;
		int sel_result;

		AUD_FD_COPY (& r->rfd, & r_curr);
		
		sel_result = select (r->fd_max, & r_curr, NULL, NULL, NULL);
		if (sel_result > 0)
		{
			int fd = DNSServiceRefSockFD (r->sdRef);
			if (FD_ISSET (fd, & r_curr))
			{
				DNSServiceProcessResult (r->sdRef);
			}
		}
		else
		{
			int err = errno;
			if (err != EINTR)
			{
				fprintf (stderr,
					"Select error: %s (%d)\n"
					, strerror (err), err
				);
				r->running = AUD_FALSE;
				r->status = AUD_FALSE;
			}
		}
	}
	
	return r->status;
}


static void
add_fd (runtime_t * r, int fd)
{
	FD_SET (fd, & r->rfd);
	if (fd + 1 > r->fd_max)
	{
		r->fd_max = fd+1;
	}
}

#if 0
// this function is unused at the moment
static void
remove_fd (runtime_t * r, int fd)
{
	FD_CLR (fd, & r->rfd);
}
#endif

static void DNSSD_API 
register_reply (
	DNSServiceRef sdRef,
	DNSServiceFlags flags,
	DNSServiceErrorType errCode,
	const char * name,
	const char * regtype,
	const char * domain,
	void * context
)
{
	runtime_t * r = context;
	if (errCode == kDNSServiceErr_NoError)
	{
		printf (
			"Registered %s.%s%s\n"
			, name, regtype, domain
		);
	}
	else
	{
		fprintf (stderr,
			"Registeration failed: %d\n"
			, errCode
		);
		r->running = AUD_FALSE;
		r->status = AUD_FALSE;
	}
}


static aud_bool_t
register_service (runtime_t * r)
{
	DNSServiceErrorType errcode;
	
	errcode =
		DNSServiceRegister (
			& r->sdRef,
			0,	// no flags
			0,	// all interfaces
			NULL,	// hostname
			r->service_type,
			NULL,	// default domain 
			NULL,	// default host
			htons (r->port),
			0,	// no TXT record
			NULL,	// no TXT record
			register_reply,	// callback
			r	// context
		);
	if (errcode != kDNSServiceErr_NoError)
	{
		fprintf (stderr,
			"Failed to register %s on port %u: %d\n"
			, r->service_type, r->port, errcode
		);
		return AUD_FALSE;
	}

	add_fd (r, DNSServiceRefSockFD (r->sdRef));
	
	return AUD_TRUE;
}


int
main (void)
{
	aud_bool_t success = AUD_FALSE;

	runtime_t r = {0};
	
	r.port = 6789;
	r.service_type = "_example._udp";
	
	if (register_service (& r))
	{
		success = run_loop (& r);
	}
	
	return ! success;
}
