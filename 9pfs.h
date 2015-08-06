typedef struct FFid	FFid;
typedef struct PFid 	PFid;

struct FFid
{
	FFid		*link;
	FFid		*pathlink;
	int		mode;
	uint32_t	fid;
	Qid		qid;
	uint32_t	iounit;
	off_t		offset;
	char		*path;
};

int	_9perrno;

int	_9pversion(uint32_t);
FFid	*_9pattach(FFid*, FFid*);
FFid	*_9pwalk(const char*);
int	_9pstat(FFid*, struct stat*);
int	_9pclunk(FFid*);
int	_9popen(FFid*, char);
long	_9pread(FFid*, void*, size_t, off_t);
long	_9pdirread(FFid*, Dir**);

void	init9p(int);

FFid	*hasfid(const char*);
int	addfid(const char*, FFid*);
FFid	*fidclone(FFid*);

void	*emalloc(size_t);
void	*erealloc(void*, size_t);
void	*ereallocarray(void*, size_t, size_t);
void	*ecalloc(size_t, size_t);
char	*estrdup(const char *);
