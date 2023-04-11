#include "./internal.h"
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

seplos_device
seplos_open(const char * serial_device)
{
  struct termios t = {};

  const int fd = open(serial_device, O_RDWR|O_CLOEXEC|O_NOCTTY, 0);
  if ( fd < 0 ) {
    _sp_error("%s: %s\n", serial_device, strerror(errno));
    return -1;
  }

  tcgetattr(fd, &t);
  cfsetspeed(&t, 19200);
  cfmakeraw(&t);
  tcflush(fd, TCIOFLUSH); /* Throw away any pending I/O */
  tcsetattr(fd, TCSANOW, &t);

  return fd;
}
