This is a fuse filesystem for attaching a 9p server to your unix file
tree. In other words a fuse to 9p translator.

"But doesn't a 9pfuse implementation already exist?", you may ask. Yes,
plan9port includes 9pfuse. However, 9pfuse reads the fuse device directly
instead of using the C library interface. Thus it may or may not work
on a given platform given that the semantics of the device itself are
not specified.

This fuse implementation uses the C library interface as exposed in
`<fuse.h>`. This API is standardized across many OSs and so this
filesystem works equally well on OpenBSD, FreeBSD, or Linux. It was
expressly written for OpenBSD and developed primarily on that OS.
