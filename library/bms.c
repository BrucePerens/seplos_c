#include <assert.h>
#include <errno.h>	/* FIX: Abstract away POSIX */
#include <string.h>
#include "./internal.h"
#include "./communication.h"

int
_sp_bms_command(
 seplos_device	       fd,
 const unsigned int    address,
 const unsigned int    command,
 const void * restrict info,
 const unsigned int    info_length,
 Seplos_2_0 *	       result)
{
  Seplos_2_0        encoded = {};
  Seplos_2_0_Binary s = {};
  Seplos_2_0_Binary r = {};

  s.version = 0x20; /* Protocol version 2.0 */
  s.address = address;
  s.device = 0x46;  /* Code for a battery */
  s.function = command;
  s.length = _sp_length_checksum(info_length) | (info_length & 0x0fff);

  _sp_hex2(s.version, encoded.version);
  _sp_hex2(s.address, encoded.address);
  _sp_hex2(s.device, encoded.device);
  _sp_hex2(s.function, encoded.function);
  _sp_hex4(s.length, encoded.length);

  encoded.start = '~';
  assert(info_length < 4096);

  uint8_t * i = encoded.info;
  memcpy(i, info, info_length);
  i += info_length;

  uint16_t checksum = _sp_overall_checksum(encoded.version, info_length + 12);
  _sp_hex4(checksum, i);
  i += 4;

  *i++ = '\r';

  _sp_discard_serial_input(fd); /* Throw away any pending I/O */

  int ret = _sp_write_serial(fd, &encoded, info_length + 18);
  if ( ret != info_length + 18 ) {
    _sp_error("Write: %s\n", strerror(errno)); /* FIX: Abstract away POSIX */
    return -1;
  }
  _sp_wait_until_serial_data_is_transmitted(fd);

  /*
   * Becuase of the the wait for data to be transmitted, above, the BMC should have
   * the command.
   * There should always be at least 18 bytes in a properly-formed packet.
   * Timeout of the read here is an unusual event, and likely means that the BMC got
   * unplugged or went into hibernation.
   */
  ret = _sp_read_serial(fd, result, 18);

  if ( ret != 18 ) {
    _sp_error("Read: %s\n", strerror(errno)); /* FIX: Abstract away POSIX */
    return -1;
  }

  bool invalid = false;

  if ( result->start != '~' )
    invalid = true;

  r.version = _sp_hex2b(result->version, &invalid);
  r.address = _sp_hex2b(result->address, &invalid);
  r.device = _sp_hex2b(result->device, &invalid);
  r.function = _sp_hex2b(result->function, &invalid);
  r.length = _sp_hex4b(result->length, &invalid);

  /* Abort if the major protocol version isn't 2. Accept any minor version */
  if ( r.version > 0x2f || r.version < 0x20 ) {
    _sp_error("SEPLOS protocol %x not implemented.\n");
    return -1;
  }

  if ( invalid ) {
    _sp_error("Non-hexidecimal character where only hexidecimal was expected: %18s.\n", (const char *)&result);
    return -1;
  }

  if ( length_checksum(r.length & 0x0fff) != (r.length & 0xf000) ) {
    _sp_error("Length code incorrect.");
    return -1; 
  }
 
  r.length &= 0x0fff;
  
  if ( r.length > 0 ) {
    ret = read_serial(fd, &(result->info[5]), r.length);
    if ( ret != r.length ) {
      _sp_error("Info read: %s\n", strerror(errno));
      return -1;
    }
  }
  
  for ( unsigned int j = 0; j < r.length + 4; j++ ) {
    uint8_t c = result->info[j];
    if ( !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) ) {
      _sp_error("Non-hexidecimal character where only hexidecimal was expected: %s.\n", (const char *)&result);
      return -1;
    }
  }

  checksum = _sp_hex4b(&(result->info[r.length]), &invalid);
  if ( invalid || checksum != overall_checksum(result->version, r.length + 12) ) {
    _sp_error("Checksum mismatch.\n");
    return -1;
  }

  if ( r.function != NORMAL ) {
    _sp_error("Return code %x.\n", r.function);
  }
  return r.function;
}
