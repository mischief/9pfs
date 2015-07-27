int	_9perr;

int	_9pversion(int, uint32_t);
FFid	*_9pattach(int, uint32_t, uint32_t);
FFid	*_9pwalk(char*, FFid*);
int	_9pstat(FFid*, struct stat*);
void	init9p(int);
