enum
{
	RERROR,
	RMISS,
};

typedef struct FFid	FFid;
typedef struct PFid 	PFid;

struct FFid
{
	FFid		*link;
	int		mode;
	uint32_t	fid;
	Qid		qid;
	off_t		offset;
	PFid		*pfid;
};

struct PFid
{
	PFid	*link;
	char	*path;
	FFid	*ffid;
};

int	srvfd;
