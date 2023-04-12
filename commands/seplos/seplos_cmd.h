#include <stdbool.h>
#include <argp.h>

extern const struct argp	argp;

enum Format {
  TEXT,
  HTML,
  JSON
};

struct arguments
{
  char *	device;	/* Serial device connected to the battery */
  enum Format	format; /* text, HTML, or JSON. */
  bool		longer; /* More information but not necessarily verbose */
};

