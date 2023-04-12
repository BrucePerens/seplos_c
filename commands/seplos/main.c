#include "./seplos_cmd.h"
#include <stdio.h>
#include "seplos.h"

int
main(int argc, char * * argv)
{
  const char *		device = "/dev/ttyUSB0";
  struct arguments	arguments = {};

  arguments.device = "/dev/ttyUSB0";
  arguments.format = TEXT;

  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  int fd = seplos_open(arguments.device);

  SeplosData d;

  if ( fd < 0 )
    return 1;

  seplos_data(fd, 0, 0x01, &d);

  switch ( arguments.format ) {
  case TEXT:
    seplos_text(stdout, &d, arguments.longer);
    break;
  case HTML:
    seplos_html(stdout, &d, arguments.longer);
    break;
  case JSON:
    seplos_json(stdout, &d, arguments.longer);
    break;
  }
  return 0;
}
