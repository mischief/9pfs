/*
 * Interface for typical callers.
 */

typedef struct	AuthInfo	AuthInfo;
typedef struct	AuthRpc		AuthRpc;

enum
{
	ARok = 0,			/* rpc return values */
	ARdone,
	ARerror,
	ARneedkey,
	ARbadkey,
	ARwritenext,
	ARtoosmall,
	ARtoobig,
	ARrpcfailure,
	ARphase,

	AuthRpcMax = 4096
};

struct AuthRpc
{
	int afd;
	char ibuf[AuthRpcMax+1];
	char obuf[AuthRpcMax];
	char *arg;
	uint narg;
};

struct AuthInfo
{
	char	*cuid;		/* caller id */
	char	*suid;		/* server id */
	char	*cap;		/* capability (only valid on server side) */
	int	nsecret;	/* length of secret */
	uchar	*secret;	/* secret */
};

typedef int AuthGetkey(char*);

extern AuthInfo*	fauth_proxy(FFid*, AuthRpc *rpc, AuthGetkey *getkey, char *params);
extern AuthInfo*	auth_proxy(FFid*, AuthGetkey *getkey, char *fmt, ...);
extern int		auth_getkey(char*);
extern void		auth_freeAI(AuthInfo *ai);
extern AuthInfo*	auth_getinfo(AuthRpc *rpc);
extern AuthRpc*		auth_allocrpc(int afd);
extern void		auth_freerpc(AuthRpc *rpc);
extern uint		auth_rpc(AuthRpc *rpc, char *verb, void *a, int n);
