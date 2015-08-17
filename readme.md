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

It is also faster than 9pfuse. Below is the time it took to run du -a on my home directory on a plan 9 installation mounted by 9pfs:

```
$ time du -a
real    0m1.473s
user    0m0.007s
sys     0m0.027s
$ time du -a
real    0m1.467s
user    0m0.013s
sys     0m0.027s
$ time du -a
real    0m1.447s
user    0m0.013s
sys     0m0.023s
```

And this is the time it takes for 9pfuse:

```
$ time du -a
real    0m9.905s
user    0m0.010s
sys     0m0.033s
$ time du -a
real    0m12.016s
user    0m0.000s
sys     0m0.050s
$ time du -a
real    0m11.999s
user    0m0.003s
sys     0m0.047s
```
