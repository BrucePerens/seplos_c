#include "./internal.h"
#include <termios.h>
#include <unistd.h>

void
_sp_discard_serial_input(seplos_device fd) {
  tcflush(fd, TCIOFLUSH); /* Throw away any pending I/O */
}

void
_sp_wait_until_serial_data_is_transmitted(seplos_device fd) {
  tcdrain(fd);
}

int
_sp_write_serial(seplos_device fd, void * data, size_t size)
{
  return write(fd, data, size);
}
