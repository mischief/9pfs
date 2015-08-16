typedef struct FFid	FFid;

struct FFid
{
	FFid	*link;
	uchar	mode;
	u32int	fid;
	Qid	qid;
	u32int	iounit;
	u64int	offset;
	char	*path;
};

FILE	*logfile;

int	_9pversion(u32int);
FFid	*_9pattach(FFid*, FFid*);
FFid	*_9pwalk(const char*);
FFid	*_9pwalkr(FFid*, char*);
int	_9pstat(FFid*, struct stat*);
int	_9pclunk(FFid*);
int	_9popen(FFid*);
FFid	*_9pcreate(FFid*, char*, int);
int	_9premove(FFid*);
int	_9pread(FFid*, void*, u32int);
int	_9pwrite(FFid*, void*, u32int);
int	_9pdirread(FFid*, Dir**);

void	init9p(int);

FFid	*fidclone(FFid*);
int	getstat(struct stat*, char*);
void	*emalloc(size_t);
void	*erealloc(void*, size_t);
void	*ereallocarray(void*, size_t, size_t);
void	*ecalloc(size_t, size_t);
char	*estrdup(const char *);

int	dprint(char*, ...);
