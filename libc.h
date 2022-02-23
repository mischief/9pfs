#define nil NULL

#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))

#define	ERRMAX	256
#define	OREAD	0	/* open for read */
#define	OWRITE	1	/* write */
#define	ORDWR	2	/* read and write */
#define	OEXEC	3	/* execute, == read but check execute permission */
#define	OTRUNC	16	/* or'ed in (except for exec), truncate file first */
#define	OCEXEC	32	/* or'ed in, close on exec */
#define	ORCLOSE	64	/* or'ed in, remove on close */
#define	ODIRECT	128	/* or'ed in, direct access */
#define	ONONBLOCK 256	/* or'ed in, non-blocking call */
#define	OEXCL	0x1000	/* or'ed in, exclusive use (create only) */
#define	OLOCK	0x2000	/* or'ed in, lock after opening */
#define	OAPPEND	0x4000	/* or'ed in, append only */

/* bits in Qid.type */
#define QTDIR		0x80		/* type bit for directories */
#define QTAPPEND	0x40		/* type bit for append only files */
#define QTEXCL		0x20		/* type bit for exclusive use files */
#define QTMOUNT		0x10		/* type bit for mounted channel */
#define QTAUTH		0x08		/* type bit for authentication file */
#define QTTMP		0x04		/* type bit for non-backed-up file */
#define QTSYMLINK	0x02		/* type bit for symbolic link */
#define QTFILE		0x00		/* type bits for plain file */

/* bits in Dir.mode */
#define DMDIR		0x80000000	/* mode bit for directories */
#define DMAPPEND	0x40000000	/* mode bit for append only files */
#define DMEXCL		0x20000000	/* mode bit for exclusive use files */
#define DMMOUNT		0x10000000	/* mode bit for mounted channel */
#define DMAUTH		0x08000000	/* mode bit for authentication file */
#define DMTMP		0x04000000	/* mode bit for non-backed-up file */
#define DMSYMLINK	0x02000000	/* mode bit for symbolic link (Unix, 9P2000.u) */
#define DMDEVICE	0x00800000	/* mode bit for device file (Unix, 9P2000.u) */
#define DMNAMEDPIPE	0x00200000	/* mode bit for named pipe (Unix, 9P2000.u) */
#define DMSOCKET	0x00100000	/* mode bit for socket (Unix, 9P2000.u) */
#define DMSETUID	0x00080000	/* mode bit for setuid (Unix, 9P2000.u) */
#define DMSETGID	0x00040000	/* mode bit for setgid (Unix, 9P2000.u) */

#define DMREAD		0x4		/* mode bit for read permission */
#define DMWRITE		0x2		/* mode bit for write permission */
#define DMEXEC		0x1		/* mode bit for execute permission */

#define	STATMAX	65535U	/* max length of machine-independent stat structure */
#define	DIRMAX	(sizeof(Dir)+STATMAX)	/* max length of Dir structure */

typedef uint64_t u64int;
typedef int64_t s64int;
typedef uint8_t u8int;
typedef int8_t s8int;
typedef uint16_t u16int;
typedef int16_t s16int;
typedef uintptr_t uintptr;
typedef intptr_t intptr;
typedef uint32_t u32int;
typedef int32_t s32int;
typedef unsigned char uchar;
typedef unsigned long long uvlong;
typedef long long vlong;

typedef
struct Qid
{
	uvlong	path;
	u32int	vers;
	uchar	type;
} Qid;

typedef
struct Dir {
	/* system-modified data */
	ushort	type;	/* server type */
	uint	dev;	/* server subtype */
	/* file data */
	Qid	qid;	/* unique id from server */
	u32int	mode;	/* permissions */
	u32int	atime;	/* last read time */
	u32int	mtime;	/* last write time */
	vlong	length;	/* file length */
	char	*name;	/* last element of path */
	char	*uid;	/* owner name */
	char	*gid;	/* group name */
	char	*muid;	/* last modifier name */
} Dir;

char	*strecpy(char*, char*, char*);
long	readn(int, void*, long);
int	fauth(int fd, char *aname);
