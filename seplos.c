#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Seplos BMS communication protocol 2.0
 *
 * Although the SEPLOS document refers to this as a Modbus-ASCII protocol, it isn't
 * one. They're confusing the Modbus-ASCII _protocol_, which they don't use, with the
 * RS-485 _transport_, which they use. It seems common to confuse the two. This is more
 * properly called an ASCII-over-RS-485 protocol. Modbus-ASCII packets start with ':'
 * rather than the '~' used by SEPLOS, and the packet format is entirely different.
 */
struct _Seplos_2_0 {
  char  start_code;      /* Always '~' */
  char  version_code[2]; /* Always '2', '0' for protocol version 2.0 */
  char  address_code[2]; /* ASCII value from '0' to '15' */
  char  device_code[2];  /* Always '4', '6' for a battery */
  char  function_code[2];/* Command or reply ID */
  char  length_code[4];  /* Length (12 bits) and length checksum (4 bits). */
  char  info[4095 + 4 + 1];/* "info" field, checksum, 0xD to end the packet */
  const char guard;  /* Pointer to .info must not reach this address */
};
typedef struct _Seplos_2_0 Seplos_2_0;

/* The comments are as SEPLOS documented the names of these commands */
enum _seplos_commands {
  TELEMETERY_GET =     0x42,	/* Acquisition of telemetering information */
  TELECOMMAND_GET =    0x44,	/* Acquisition of telecommand information */
  TELECONTROL_CMD =    0x45,	/* Telecontrol command */
  TELEREGULATION_GET = 0x47,	/* Acquisition of teleregulation information */
  TELEREGULATION_SET = 0x49,	/* Setting of teleregulation information */
  PROTOCOL_VER_GET =   0x4F,	/* Acquisition of the communication protocol version number */
  VENDOR_GET =         0x51,	/* Acquisition of device vendor information */
  HISTORY_GET =        0x4B,	/* Acquisition of historical data */
  TIME_GET =           0x4D,	/* Acquisition time */
  TIME_SET =           0x4E,	/* Synchronization time */
  PRODUCTION_CAL =     0xA0,	/* Production calibration */
  PRODUCTION_SET =     0xA1,	/* Production setting */
  REGULAR_RECORDING =  0xA2	/* Regular recording */
};

enum _seplos_response {
  NORMAL = 0x00,                 /* Normal response */
  VERSION_ERROR = 0x01,          /* Protocol version error */
  CHECKSUM_ERROR = 0x02,         /* Checksum error */
  LENGTH_CHECKSUM_ERROR = 0x03,  /* Checksum value in length field error */
  CID2_ERROR = 0x04,		 /* Second byte or field is incorrect */
  COMMAND_FORMAT_ERROR = 0x05,   /* Command format error */
  DATA_INVALID = 0x06,           /* Data invalid (parameter setting) */
  NO_HISTORY = 0x07,             /* No historical data (NVRAM error?) */
  CID1_ERROR = 0xe1,             /* First byte or field is incorrect */
  EXECUTION_FAILURE = 0xe2,      /* Command execution failure */
  DEVICE_FAULT = 0xe3,           /* Device fault */
  PERMISSION_ERROR = 0xe4        /* Permission error */
};

int
main(int argc, char * * argv)
{
  static const char hex[] = "0123456789ABCDEF";

  Seplos_2_0	s = {};

  char *        i = s.info;
  const uint8_t address = 0;
  const uint8_t command = PROTOCOL_VER_GET;

  s.start_code = '~';

  s.version_code[0] = '2'; /* Protocol version 2.0 */
  s.version_code[1] = '0';

  s.address_code[0] = '0' + ((address >> 4) & 0xf);
  s.address_code[1] = '0' + (address & 0xf);

  s.device_code[0] = '4'; /* It's a battery */
  s.device_code[1] = '6';

  s.function_code[0] = '0' + ((command > 4) & 0xf);
  s.function_code[0] = '0' + (command & 0xf);

  /*
   * length_code is the ASCII representation of the length of the data in .info and
   * a checksum
   */
  const ptrdiff_t info_length = i - s.info;
  unsigned int set_bits = 0;

  /* Count the set bits in info_length using Brian Kernighan's algorithm */
  for (ptrdiff_t n = info_length; n != 0; set_bits++) {
    n &= (n - 1);
  }
  const unsigned int length_id = ((((~set_bits) + 1) << 12) & 0xf000) \
   | (info_length & 0x0fff);

  s.length_code[0] = hex[(length_id >> 12) & 0xf];
  s.length_code[1] = hex[(length_id >> 8) & 0xf];
  s.length_code[2] = hex[(length_id >> 4) & 0xf];
  s.length_code[3] = hex[length_id & 0xf];

  /* Place a 4-character ASCII checksum at the end of the info data */
  unsigned int sum = 0;

  for ( const char * b = &(s.start_code); b < i; b++ ) {
    sum += *b;
  }

  const unsigned int checksum = (~sum) + 1;
  *i++ = hex[(checksum >> 12) & 0xf];
  *i++ = hex[(checksum >> 8) & 0xf];
  *i++ = hex[(checksum >> 4) & 0xf];
  *i++ = hex[checksum & 0xf];

  /* Terminate the packet with a carriage-return */
  *i++ = '\r';

  int fd = open("/dev/ttyUSB0", O_RDWR|O_CLOEXEC|O_NOCTTY, 0);
  if ( fd < 0 ) {
    perror("/dev/ttyUSB0");
    exit(1);
  }
  int result = write(fd, &s, info_length + 18);
  if ( result < 18 ) {
    perror("/dev/ttyUSB0");
    exit(1);
  }

  Seplos_2_0 r = {};

  /*
   * This needs to be non-blocking, with a timeout, in the final version.
   * There should always be at least 18 bytes in a properly-formed packet.
   * at that point, we can parse the packet for correctness, read any additional
   * data indicated by the length code, and parse the final checksum for correctness.
   * We also should be draining any spurious characters from the wire before 
   * we send our command, and, once we send our command, we should drain anything
   * that's not a properly formatted packet.
   *
   * If the battery puts itself into hibernation, it responds for a few seconds
   * before the controller shuts down. So, we should poll it every second, and
   * save the last data, and the time of last response, to display to the user
   * so that they know what happened (presuming there is another power source
   * to keep the computer running).
   */
  result = read(fd, &r, 18);
  printf("%d\n", result);
  
  return 0;
}
