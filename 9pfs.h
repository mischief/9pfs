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
	PFid		**pfid;
};

struct PFid
{
	PFid	*link;
	char	*path;
	FFid	**ffid;
};

int	srvfd;
int	_9perrno;

int	_9pversion(uint32_t);
FFid	*_9pattach(uint32_t, uint32_t);
FFid	**_9pwalk(const char*);
int	_9pstat(FFid*, struct stat*);
int	_9pclunk(uint32_t);
int	_9popen(FFid*, int);

void	init9p(int);

FFid	**hasfid(const char*);
int	addfid(const char*, FFid**);
FFid	**fidclone(FFid*);

void	*emalloc(size_t);
void	*erealloc(void*, size_t);
char	*estrdup(const char *);
