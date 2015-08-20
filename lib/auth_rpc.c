#include <sys/types.h>

#include <unistd.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "../libc.h"
#include "../fcall.h"
#include "../auth.h"
#include "../util.h"

static struct {
	char *verb;
	int val;
} tab[] = {
	{ "ok",		ARok },
	{ "done",	ARdone },
	{ "error",	ARerror },
	{ "needkey",	ARneedkey },
	{ "badkey",	ARbadkey },
	{ "phase",	ARphase },
	{ "toosmall",	ARtoosmall },
	{ "error",	ARerror },
};

static int
classify(char *buf, uint n, AuthRpc *rpc)
{
	int i, len;

	for(i=0; i<nelem(tab); i++){
		len = strlen(tab[i].verb);
		if(n >= len && memcmp(buf, tab[i].verb, len) == 0 && (n==len || buf[len]==' ')){
			if(n==len){
				rpc->narg = 0;
				rpc->arg = "";
			}else{
				rpc->narg = n - (len+1);
				rpc->arg = (char*)buf+len+1;
			}
			return tab[i].val;
		}
	}
	return ARrpcfailure;
}

AuthRpc*
auth_allocrpc(int afd)
{
	AuthRpc *rpc;

	rpc = emalloc(sizeof(*rpc));
	if(rpc == nil)
		return nil;
	rpc->afd = afd;
	return rpc;
}

void
auth_freerpc(AuthRpc *rpc)
{
	free(rpc);
}

uint
auth_rpc(AuthRpc *rpc, char *verb, void *a, int na)
{
	int l, n;

	l = strlen(verb);
	if(na+l+1 > AuthRpcMax){
		return ARtoobig;
	}

	memmove(rpc->obuf, verb, l);
	rpc->obuf[l] = ' ';
	memmove(rpc->obuf+l+1, a, na);
	dprint("auth_rpc writing %s\n", rpc->obuf);
	if((n=write(rpc->afd, rpc->obuf, l+1+na)) != l+1+na){
		return ARrpcfailure;
	}

	if((n=read(rpc->afd, rpc->ibuf, AuthRpcMax)) < 0)
		return ARrpcfailure;
	rpc->ibuf[n] = '\0';
	dprint("auth_rpc read %s\n", rpc->ibuf);
	return classify(rpc->ibuf, n, rpc);
}
