#include <sys/types.h>

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
fs_getattr(const char *path, struct stat *st)
{
	if(strcmp(path, "/") == 0){
		st->st_mode = S_IFDIR | 0555;
		st->st_nlink = 2;
		st->st_size = 20;
	}else if(strcmp(path, "/hello") == 0){
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = 6;
	}else
		return -ENOENT;
	return (0);
}
	
int
fs_readdir(const char *path, void *data, fuse_fill_dir_t f,
           off_t off, struct fuse_file_info *ffi)
{
	if(strcmp(path, "/") != 0)
		return -ENOENT;
	f(data, ".", NULL, 0);
	f(data, "..", NULL, 0);
	f(data, "hello", NULL, 0);
	return 0;
}

int
fs_open(const char *path, struct fuse_file_info *ffi)
{
	if(strcmp(path, "/hello") != 0)
		return -ENOENT;
	if((ffi->flags & 3) != O_RDONLY)
		return -EACCES;
	return 0;
}

int
fs_read(const char *path, char *buf, size_t size, off_t off,
        struct fuse_file_info *ffi)
{
	if(off >= 6)
		return 0;
	size = 6 - off;
	memcpy(buf, "world\n" + off, size);
	return size;
}

struct fuse_operations fsops = {
	.readdir	= fs_readdir,
	.getattr	= fs_getattr,
	.open		= fs_open,
	.read		= fs_read
};

int
main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &fsops, NULL);
}
