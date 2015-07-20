enum
{
	NHASH=1009
};

typedef struct PFid	PFid;
struct PFid
{
	char		*path;
	uint32_t	fid;
	PFid		*link;
};

PFid	*fidhash[NHASH];
