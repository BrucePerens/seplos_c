#include <argp.h>

extern const struct argp	argp;

struct arguments
{
  char *	device;	/* Serial device connected to the battery */
  bool		longer; /* More information but not necessarily verbose */
};

