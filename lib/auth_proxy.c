#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "../libc.h"
#include "../fcall.h"
#include "../9pfs.h"
#include "../auth.h"

enum { 
	ARgiveup = 100
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
	a = nil;
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
fauth_proxy(FFid *fid, AuthRpc *rpc, AuthGetkey *getkey, char *params)
{
	char *buf;
	int m, n, ret;
	AuthInfo *a;

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
			return a;
		case ARok:
			if(_9pwrite(fid, rpc->arg, rpc->narg) != rpc->narg){
				goto Error;
			}
			break;
		case ARphase:
			n = 0;
			memset(buf, 0, AuthRpcMax);
			while((ret = dorpc(rpc, "write", buf, n, getkey)) == ARtoosmall){
				if(atoi(rpc->arg) > AuthRpcMax)
					break;
				m = _9pread(fid, buf+n, atoi(rpc->arg)-n);
				if(m <= 0){
					if(m == 0)
					goto Error;
				}
				n += m;
			}
			if(ret != ARok){
				goto Error;
			}
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
auth_proxy(FFid *fid, AuthGetkey *getkey, char *fmt, ...)
{
	char *p;
	va_list arg;
	AuthInfo *ai;
	AuthRpc *rpc;

	quotefmtinstall();	/* just in case */
	va_start(arg, fmt);
	p = vsmprint(fmt, arg);
	va_end(arg);

	rpc = auth_allocrpc();
	if(rpc == nil){
		free(p);
		return nil;
	}

	ai = fauth_proxy(fid, rpc, getkey, p);
	free(p);
	auth_freerpc(rpc);
	return ai;
}
