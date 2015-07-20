typedef struct Fidl Fidl;

struct Fidl
{
	char		*path;
	uint32_t	fid;
	Fidl		*link;
};

Fidl	fidhash[NHASH];
