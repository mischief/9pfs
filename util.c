#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

void*
emalloc(size_t size)
{
	void	*v;

	if((v = malloc(size)) == NULL)
		err(1, "emalloc: out of memory");
	memset(v, 0, size);
	return v;
}

void*
erealloc(void *p, size_t size)
{
	void	*v;

	if((v = realloc(p, size)) == NULL)
		err(1, "emalloc: out of memory");
	return v;
}

void*
ecalloc(size_t nmemb, size_t size)
{
	void 	*v;

	if((v = calloc(nmemb, size)) == NULL)
		err(1, "ecalloc: out of memory");
	return v;
}

void*
ereallocarray(void *ptr, size_t nmemb, size_t size)
{
	void	*v;

	if((v = reallocarray(ptr, nmemb, size)) == NULL)
		err(1, "ereallocarray: out of memory");
	return v;
}

char*
estrdup(const char *s)
{
	char	*r;

	if((r = strdup(s)) == NULL)
		err(1, "estrdup: out of memory");
	return r;
}
