9pfs mounts a 9P service using the FUSE file system driver. It is a
replacement for 9pfuse from [plan9port](https://swtch.com/plan9port/).
Unlike 9pfuse, it works equally well on Linux, OpenBSD, FreeBSD, and
any other OS with a FUSE implementation.

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
