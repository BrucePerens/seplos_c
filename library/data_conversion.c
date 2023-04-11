#include "./internal.h"

static const char hex[] = "0123456789ABCDEF";

float
_sp_farenheit(float c)
{
  return (c * 1.8) + 32;
}

void
_sp_hex1(uint8_t value, char ascii[1])
{
  ascii[0] = hex[value & 0xf];
}

void
_sp_hex2(uint8_t value, char ascii[2])
{
  ascii[0] = hex[(value >> 4) & 0xf];
  ascii[1] = hex[value & 0xf];
}

void
_sp_hex4(uint16_t value, char ascii[4])
{
  ascii[0] = hex[(value >> 12) & 0xf];
  ascii[1] = hex[(value >> 8) & 0xf];
  ascii[2] = hex[(value >> 4) & 0xf];
  ascii[3] = hex[value & 0xf];
}

uint8_t
_sp_hex1b(uint8_t c, bool * invalid)
{
  if ( c >= '0' && c <= '9' )
    return c - '0';
  else if ( c >= 'a' && c <= 'f' )
    return c - 'a' + 10;
  else if ( c >= 'A' && c <= 'F' )
    return c - 'A' + 10;
  else {
    *invalid = true;
    return 0;
  }
}

uint8_t
_sp_hex2b(const char ascii[2], bool * invalid)
{
  return (hex1b(ascii[0], invalid) << 4) | hex1b(ascii[1], invalid);
}

uint16_t
_sp_hex4b(const char ascii[4], bool * invalid)
{
  return (hex1b(ascii[0], invalid) << 12) | (hex1b(ascii[1], invalid) << 8) | \
   (hex1b(ascii[2], invalid) << 4) | hex1b(ascii[3], invalid);
}

unsigned int
_sp_length_checksum(unsigned int length)
{
  const unsigned int sum = ((length >> 8) & 0xf) + ((length >> 4) & 0x0f) + (length & 0x0f);
  return (((~(sum & 0xff)) + 1) << 12) & 0xf000;
}

unsigned int
_sp_overall_checksum(const char * restrict data, unsigned int length)
{
  unsigned int sum = 0;

  for ( unsigned int i = 0; i < length; i++ ) {
    sum += *data++;
  }

  return ((~sum) & 0xffff) + 1;
}
