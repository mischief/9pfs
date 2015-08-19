#include <sys/types.h>

#include <string.h>

#define nil NULL

char*
strecpy(char *to, char *e, char *from)
{
	if(to >= e)
		return to;
	to = memccpy(to, from, '\0', e - to);
	if(to == nil){
		to = e - 1;
		*to = '\0';
	}else{
		to--;
	}
	return to;
}
