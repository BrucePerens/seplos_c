#include "./internal.h"
#include <stdarg.h>

void
_sp_error(const char * restrict pattern, ...)
{
  va_list args;

  va_start(args, pattern);
  fflush(stdout);
  vfprintf(stderr, pattern, args);
  fflush(stderr);
  va_end(args);
}

