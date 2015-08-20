#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../libc.h"
#include "../fcall.h"
#include "../auth.h"
#include "../util.h"

enum {
	ARgiveup = 100,
};

static uchar*
gstring(uchar *p, uchar *ep, char **s)
{
	uint n;

	if(p == nil)
		return nil;
	if(p+BIT16SZ > ep)
		return nil;
	n = GBIT16(p);
	p += BIT16SZ;
	if(p+n > ep)
		return nil;
	*s = emalloc(n+1);
	memmove((*s), p, n);
	(*s)[n] = '\0';
	p += n;
	return p;
}

static uchar*
gcarray(uchar *p, uchar *ep, uchar **s, int *np)
{
	uint n;

	if(p == nil)
		return nil;
	if(p+BIT16SZ > ep)
		return nil;
	n = GBIT16(p);
	p += BIT16SZ;
	if(p+n > ep)
		return nil;
	*s = emalloc(n);
	if(*s == nil)
		return nil;
	memmove((*s), p, n);
	*np = n;
	p += n;
	return p;
}

void
auth_freeAI(AuthInfo *ai)
{
	if(ai == nil)
		return;
	free(ai->cuid);
	free(ai->suid);
	free(ai->cap);
	free(ai->secret);
	free(ai);
}

static uchar*
convM2AI(uchar *p, int n, AuthInfo **aip)
{
	uchar *e = p+n;
	AuthInfo *ai;

	ai = emalloc(sizeof(*ai));
	if(ai == nil)
		return nil;

	p = gstring(p, e, &ai->cuid);
	p = gstring(p, e, &ai->suid);
	p = gstring(p, e, &ai->cap);
	p = gcarray(p, e, &ai->secret, &ai->nsecret);
	if(p == nil)
		auth_freeAI(ai);
	else
		*aip = ai;
	return p;
}

AuthInfo*
auth_getinfo(AuthRpc *rpc)
{
	AuthInfo *a;

	if(auth_rpc(rpc, "authinfo", nil, 0) != ARok)
		return nil;
	if(convM2AI((uchar*)rpc->arg, rpc->narg, &a) == nil){
		return nil;
	}
	return a;
}

static int
dorpc(AuthRpc *rpc, char *verb, char *val, int len, AuthGetkey *getkey)
{
	int ret;

	for(;;){
		if((ret = auth_rpc(rpc, verb, val, len)) != ARneedkey && ret != ARbadkey)
			return ret;
		if(getkey == nil)
			return ARgiveup;	/* don't know how */
		if((*getkey)(rpc->arg) < 0)
			return ARgiveup;	/* user punted */
	}
}

/*
 *  this just proxies what the factotum tells it to.
 */
AuthInfo*
fauth_proxy(int fd, AuthRpc *rpc, AuthGetkey *getkey, char *params)
{
	char *buf;
	int m, n, ret;
	AuthInfo *a;

	if(rpc == nil){
		return nil;
	}

	if(dorpc(rpc, "start", params, strlen(params), getkey) != ARok){
		return nil;
	}

	buf = emalloc(AuthRpcMax);
	if(buf == nil)
		return nil;
	for(;;){
		switch(dorpc(rpc, "read", nil, 0, getkey)){
		case ARdone:
			free(buf);
			a = auth_getinfo(rpc);
			/* no error, restore whatever was there */
			return a;
		case ARok:
			if(write(fd, rpc->arg, rpc->narg) != rpc->narg){
				goto Error;
			}
			break;
		case ARphase:
			n = 0;
			memset(buf, 0, AuthRpcMax);
			while((ret = dorpc(rpc, "write", buf, n, getkey)) == ARtoosmall){
				m = atoi(rpc->arg);
				if(m <= n || m > AuthRpcMax)
					break;
				m = read(fd, buf + n, m - n);
				if(m <= 0)
					goto Error;
				n += m;
			}
			if(ret != ARok)
				goto Error;
			break;
		default:
			goto Error;
		}
	}
Error:
	free(buf);
	return nil;
}

AuthInfo*
auth_proxy(int fd, AuthGetkey *getkey, char *fmt, ...)
{
	int afd;
	char *p, *ftm, *rpcpath;
	va_list arg;
	AuthInfo *ai;
	AuthRpc *rpc;

	va_start(arg, fmt);
	vasprintf(&p, fmt, arg);
	va_end(arg);

	ai = nil;
	ftm = getenv("factotum");
	asprintf(&rpcpath, "%s/rpc", ftm);
	afd = open(rpcpath, ORDWR);
	if(afd < 0){
		free(p);
		free(rpcpath);
		return nil;
	}

	rpc = auth_allocrpc(afd);
	if(rpc){
		ai = fauth_proxy(fd, rpc, getkey, p);
		auth_freerpc(rpc);
	}
	close(afd);
	free(p);
	free(rpcpath);
	return ai;
}
