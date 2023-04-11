#include "./internal.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>

int
_sp_read_serial(seplos_device fd, void * data, size_t size)
{
  /* FIX: Fill in the timeout here. */

  size_t received_amount = 0;

  while ( received_amount < size ) {
    int ret = read(fd, data, size - received_amount);
    if ( ret < 0 ) {
      _sp_error("Read failed: %s\n", strerror(errno));
      return ret;
    }
    else if ( ret == 0 ) {
      /* Read should always block until it receives at least one character */
      _sp_error("Serial end-of-file.\n");
      return -1;
    }
    else {
      received_amount += ret;
      data += ret;
    }
  }
  return received_amount;
}

