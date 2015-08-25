enum
{
	ROOTFID,
	AUTHFID
};

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
	int	ndirs;
	Dir	**sdirs;
};

FILE	*logfile;

FFid	*rootfid;
FFid	*authfid;
int	msize;
int	srvfd;
int	debug;

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
int	_9pread(FFid*, void*, u32int);
int	_9pwrite(FFid*, void*, u32int);
long	_9pdirread(FFid*, Dir**);

Dir	*isdircached(const char*);
