#include "./internal.h"
#include <stdio.h>

int
main(int argc, char * * argv)
{
  const char *		device = "/dev/ttyUSB0";
  struct arguments	arguments = {};

  arguments.device = "/dev/ttyUSB0";

  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  int fd = seplos_open(arguments.device);

  SeplosData d;

  if ( fd < 0 )
    return 1;

  seplos_data(fd, 0, 0x01, &d);
  seplos_text(stdout, &d, arguments.longer);
  return 0;
}
