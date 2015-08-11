typedef struct FFid	FFid;
typedef struct PFid 	PFid;

struct FFid
{
	FFid	*link;
	FFid	*pathlink;
	int	mode;
	u32int	fid;
	Qid	qid;
	u32int	iounit;
	u64int	offset;
	char	*path;
};

int	_9perrno;

int	_9pversion(u32int);
FFid	*_9pattach(FFid*, FFid*);
FFid	*_9pwalk(const char*);
FFid	*_9pwalkr(FFid*, char*);
int	_9pstat(FFid*, struct stat*);
int	_9pclunk(FFid*);
int	_9popen(FFid*, char);
FFid	*_9pcreate(FFid*, char*, int, int);
u32int	_9pread(FFid*, void*, u32int*);
u32int	_9pwrite(FFid*, void*, u32int*);
u32int	_9pdirread(FFid*, Dir**);

void	init9p(int);

FFid	*hasfid(const char*);
int	addfid(const char*, FFid*);
FFid	*fidclone(FFid*);

void	*emalloc(size_t);
void	*erealloc(void*, size_t);
void	*ereallocarray(void*, size_t, size_t);
void	*ecalloc(size_t, size_t);
char	*estrdup(const char *);
