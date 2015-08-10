#include <sys/types.h>

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum
{
	ROOT,
	HELLO,
	FOO,
	BAR
};

int
fs_getattr(const char *path, struct stat *st)
{
	if(strcmp(path, "/") == 0){
		st->st_mode = S_IFDIR | 0555;
		st->st_nlink = 2;
		st->st_size = 0;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = st->st_mtime = st->st_ctime = time(0);
	}else if(strcmp(path, "/foo") == 0){
		st->st_mode = S_IFDIR | 0555;
		st->st_nlink = 2;
		st->st_size = 0;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = st->st_mtime = st->st_ctime = time(0);
	}else if(strcmp(path, "/hello") == 0){
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = 6;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = st->st_mtime = st->st_ctime = time(0);
	}else if(strcmp(path, "/foo/bar") == 0){
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = 7;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = st->st_mtime = st->st_ctime = time(0);
	}else
		return -ENOENT;
	return 0;
}

int
fs_fgetattr(const char *path, struct stat *st, struct fuse_file_info *ffi)
{
	fprintf(stderr, "USING FGETATTR\n");
	switch(ffi->fh){
	case ROOT:
	case FOO:
		st->st_mode = S_IFDIR | 0555;
		st->st_nlink = 2;
		st->st_size = 0;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = st->st_mtime = st->st_ctime = time(0);
	case HELLO:
	case BAR:
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = 6;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = st->st_mtime = st->st_ctime = time(0);
	default:
		return -ENOENT;
	}
	return 0;
}
		
int
fs_opendir(const char *path, struct fuse_file_info *ffi)
{
	if((ffi->flags & 3) != O_RDONLY)
		return -EACCES;
	if(strcmp(path, "/") == 0){
		ffi->fh = ROOT;
	}else if(strcmp(path, "/foo") == 0){
		ffi->fh = FOO;
	}else
		return -ENOENT;
	return 0;
}	
	
int
fs_readdir(const char *path, void *data, fuse_fill_dir_t f,
           off_t off, struct fuse_file_info *ffi)
{
	switch(ffi->fh){
	case ROOT:
		f(data, ".", NULL, 0);
		f(data, "..", NULL, 0);
		f(data, "hello", NULL, 0);
		f(data, "foo", NULL, 0);
		break;
	case FOO:
		f(data, ".", NULL, 0);
		f(data, "..", NULL, 0);
		f(data, "bar", NULL, 0);
		break;
	default:
		return -ENOENT;
		break;
	}
	return 0;
}

int
fs_open(const char *path, struct fuse_file_info *ffi)
{
	if((ffi->flags & 3) != O_RDONLY)
		return -EACCES;
	if(strcmp(path, "/hello") == 0)
		ffi->fh = HELLO;
	else if(strcmp(path, "/foo/bar") == 0)
		ffi->fh = BAR;
	else
		return -ENOENT;
	return 0;
}

int
fs_read(const char *path, char *buf, size_t size, off_t off,
        struct fuse_file_info *ffi)
{
	switch(ffi->fh){
	case HELLO:
		if(off >= 6)
			return 0;
		size = 6 - off;
		memcpy(buf, "world\n" + off, size);
		break;
	case BAR:
		if(off >= 7)
			return 0;
		size = 7 - off;
		memcpy(buf, "FOOBAR\n" + off, size);
		break;
	default:
		return 0;
	}
	return size;
}

struct fuse_operations fsops = {
	.getattr	= fs_getattr,
	.fgetattr	= fs_fgetattr,
	.opendir	= fs_opendir,
	.readdir	= fs_readdir,
	.open		= fs_open,
	.read		= fs_read
};

int
main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &fsops, NULL);
}
