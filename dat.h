enum
{
	NHASH=1009
};

typedef struct FFid	FFid;
struct FFid
{
	FFid		*link;
	char		*path;
	int		mode;
	uint32_t	fid;
	Qid		qid;
};

FFid	*fidhash[NHASH];

int	msize;
int	srvfd;
