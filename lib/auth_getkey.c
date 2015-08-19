#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <err.h>

#include <stdlib.h>
#include <stdint.h>
#include "../libc.h"

int
auth_getkey(char *params)
{
	int pid, s;
	pid_t w;

	/* start /factotum to query for a key */
	switch(pid = fork()){
	case -1:
		return -1;
	case 0:
		if(execlp("factotum", "getkey", "-g", params, nil) == -1)
		err(1, "Could not exec factotum");
		exit(0);
	default:
		for(;;){
			w = wait(&s);
			if(w == -1)
				break;
			if(w == pid){
				return WIFEXITED(s) ? 0 : -1;
			}
		}
	}
	return 0;
}
