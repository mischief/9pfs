This is a fuse filesystem for attaching a 9p server to your file tree. In
other words a fuse to 9p translator.

"But doesn't a 9pfuse implementation already exist?", you may ask. Yes,
plan9port includes 9pfuse. However, 9pfuse reads the fuse device directly
instead of using the standardize fuse library interface. Thus it does
not work on OpenBSD (or FreeBSD) which implement different semantics
for the fuse messages written to and read from the fuse device.

This fuse  implementation uses the fuse library (as exposed in `<fuse.h>`)
and so works equally well on OpenBSD, FreeBSD, or Linux. It was expressly
written for OpenBSD and developed primarily on that OS.
