This is a fuse filesystem for attaching a 9p server to your unix file
tree. In other words a fuse to 9p translator.

"But doesn't a 9pfuse implementation already exist?", you may ask. Yes,
plan9port includes 9pfuse. However, 9pfuse reads the fuse device directly
instead of using the C library interface. Thus it may or may not work
on a given platform given that the semantics of the device itself are
not specified.

This fuse implementation uses the C library interface as exposed
in `<fuse.h>`. This API is standardized across various OSs and so
it works equally well on OpenBSD, FreeBSD, or Linux. It was expressly
written for OpenBSD and developed primarily on that OS.

It is also faster than 9pfuse. Below is the time it took to run du
-a on my home directory on a plan 9 installation mounted by 9pfs.

```
$ time du -a > /dev/null

real    0m2.909s
user    0m0.047s
sys     0m0.107s
$ time du -a > /dev/null

real    0m0.194s
user    0m0.007s
sys     0m0.023s
$ time du -a > /dev/null

real    0m0.212s
user    0m0.013s
sys     0m0.020s
```

The first time trial above is slow as 9pfs builds a cache as it reads
directories. The second and third trials are subsequently much faster.

Below is the time it takes for 9pfuse:

```
$ time du -a > /dev/null

real    0m10.959s
user    0m0.017s
sys     0m0.033s
$ time du -a > /dev/null

real    0m13.239s
user    0m0.000s
sys     0m0.050s
$ time du -a > /dev/null

real    0m13.082s
user    0m0.007s
sys     0m0.040s
```

Installation
------------
If you are using OpenBSD, then
```
$ make
$ sudo make install
```
will perform the installation. If you are using another operating
system of your choice, edit the the BIN and MAN variables of the
Makefile to choose where you want to install the 9pfs binary and
man page, respectively.
