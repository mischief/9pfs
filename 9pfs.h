enum
{
	ROOTFID = 0,
	AUTHFID,
	PUT = 0,
	DEL,
	GET,
	NHASH = 1009
};

#define	 FDEL	((void*)~0)

typedef struct FFid	FFid;
typedef struct FDir	FDir;

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

struct FDir
{
	FDir	*link;
	char	*path;
	Dir	*dirs;
	long	ndirs;
};

extern FILE	*logfile;

extern FFid	*rootfid;
extern FFid	*authfid;
extern int	msize;
extern int	srvfd;
extern int	debug;

void	init9p();
int	_9pversion(u32int);
FFid	*_9pauth(u32int, char*, char*);
FFid	*_9pattach(u32int, u32int, char*, char*);
FFid	*_9pwalk(const char*);
FFid	*_9pwalkr(FFid*, char*);
FFid	*fidclone(FFid*);
Dir	*_9pstat(FFid*);
int	_9pwstat(FFid*, Dir*);
int	_9pclunk(FFid*);
int	_9popen(FFid*);
FFid	*_9pcreate(FFid*, char*, int, int);
int	_9premove(FFid*);
int	_9pread(FFid*, char*, u32int);
int	_9pwrite(FFid*, char*, u32int);
long	_9pdirread(FFid*, Dir**);

int	dircmp(const void*, const void*);
FDir	*lookupdir(const char*, int);

#define DPRINT(...)				\
do{						\
	if(debug)				\
		fprintf(logfile, __VA_ARGS__);	\
}while(0)
