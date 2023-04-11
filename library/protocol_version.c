#include "./internal.h"
#include "./communication.h"

float
seplos_protocol_version(seplos_device fd, unsigned int address)
{
  Seplos_2_0	response = {};
  /*
   * For this command: BMS parses the address, but not the pack number.
   */
  uint8_t	pack_info[2] = "00";

  const unsigned int status = _sp_bms_command(
   fd,
   address,		/* Address */
   PROTOCOL_VER_GET,	/* command */
   &pack_info,		/* pack number */
   sizeof(pack_info),	/* length of the above */
   &response);

  if ( status != NORMAL ) {
    _sp_error("Bad response %x from SEPLOS BMS.\n", status);
    return -1.0;
  }

  bool invalid = false;
  uint16_t version = _sp_hex2b(response.version, &invalid);

  return ((version >> 4) & 0xf) + ((version & 0xf) * 0.1);
}
