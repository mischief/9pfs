#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

void*
emalloc(size_t size)
{
	void	*v;

	if((v = malloc(size)) == nil)
		err(1, "emalloc: out of memory");
	memset(v, 0, size);
	return v;
}
