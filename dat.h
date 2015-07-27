enum
{
	RERROR,
	RMISS,
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
	off_t		offset;
};

FFid	fidhash[1009];
int	srvfd;
