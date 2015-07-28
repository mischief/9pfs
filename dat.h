enum
{
	RERROR,
	RMISS,
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

int	srvfd;
