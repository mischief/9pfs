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

int	_9perrno;

int	_9pversion(FFid*);
FFid	*_9pattach(FFid*, FFid*);
FFid	*_9pwalk(const char*);
int	_9pstat(FFid*, struct stat*);
int	_9pclunk(FFid*);
int	_9popen(FFid*, int);

void	init9p(int);

FFid	*hasfid(const char*);
int	addfid(const char*, FFid*);
FFid	*fidclone(FFid*);

void	*emalloc(size_t);
void	*erealloc(void*, size_t);
void	*ereallocarray(void*, size_t, size_t);
void	*ecalloc(size_t, size_t);
char	*estrdup(const char *);
