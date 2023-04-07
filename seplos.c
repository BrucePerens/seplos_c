#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
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
  TELEMETERY_GET =     0x42,    /* Acquisition of telemetering information */
  TELECOMMAND_GET =    0x44,    /* Acquisition of telecommand information */
  TELECONTROL_CMD =    0x45,    /* Telecontrol command */
  TELEREGULATION_GET = 0x47,    /* Acquisition of teleregulation information */
  TELEREGULATION_SET = 0x49,    /* Setting of teleregulation information */
  PROTOCOL_VER_GET =   0x4F,    /* Acquisition of the communication protocol version number */
  VENDOR_GET =         0x51,    /* Acquisition of device vendor information */
  HISTORY_GET =        0x4B,    /* Acquisition of historical data */
  TIME_GET =           0x4D,    /* Acquisition time */
  TIME_SET =           0x4E,    /* Synchronization time */
  PRODUCTION_CAL =     0xA0,    /* Production calibration */
  PRODUCTION_SET =     0xA1,    /* Production setting */
  REGULAR_RECORDING =  0xA2     /* Regular recording */
};

enum _seplos_response {
  NORMAL = 0x00,                 /* Normal response */
  VERSION_ERROR = 0x01,          /* Protocol version error */
  CHECKSUM_ERROR = 0x02,         /* Checksum error */
  LENGTH_CHECKSUM_ERROR = 0x03,  /* Checksum value in length field error */
  CID2_ERROR = 0x04,             /* Second byte or field is incorrect */
  COMMAND_FORMAT_ERROR = 0x05,   /* Command format error */
  DATA_INVALID = 0x06,           /* Data invalid (parameter setting) */
  NO_HISTORY = 0x07,             /* No historical data (NVRAM error?) */
  CID1_ERROR = 0xe1,             /* First byte or field is incorrect */
  EXECUTION_FAILURE = 0xe2,      /* Command execution failure */
  DEVICE_FAULT = 0xe3,           /* Device fault */
  PERMISSION_ERROR = 0xe4        /* Permission error */
};

static const char hex[] = "0123456789ABCDEF";

static void
got_alarm()
{
}

static void
hex2(uint8_t value, char ascii[2])
{
  ascii[0] = hex[(value >> 4) & 0xf];
  ascii[1] = hex[value & 0xf];
}

static void
hex4(unsigned int value, char ascii[2])
{
  ascii[0] = hex[(value >> 12) & 0xf];
  ascii[1] = hex[(value >> 8) & 0xf];
  ascii[2] = hex[(value >> 4) & 0xf];
  ascii[3] = hex[value & 0xf];
}

static unsigned int
bms_command(
 int			fd,
 const unsigned int	address,
 const unsigned int	command,
 const char * restrict	info,
 const unsigned int	info_length,
 Seplos_2_0 * restrict	response)
{
  Seplos_2_0    s = {};

  char *        i = s.info;

  s.start_code = '~';

  hex2(0x20, s.version_code); /* Protocol version 2.0 */
  hex2(address, s.address_code);
  hex2(0x46, s.device_code); /* It's a battery */
  hex2(command, s.function_code);

  assert(info_length < 4095);
  memcpy(i, info, info_length);
  i += info_length;

  /*
   * length_code is the ASCII representation of the length of the data in .info and
   * a checksum
   */
  unsigned int sum = ((info_length >> 8) & 0xf) + ((info_length >> 4) & 0x0f) + (info_length & 0x0f);
  const unsigned int length_id = ((((~(sum & 0xff)) + 1) << 12) & 0xf000) \
   | (info_length & 0x0fff);
  hex4(length_id, s.length_code);

  /* Place a 4-character ASCII checksum at the end of the info data */
  sum = 0;

  for ( const char * b = s.version_code; b < i; b++ ) {
    sum += *b;
  }

  const unsigned int checksum = (~sum) + 1;

  hex4(checksum, i);
  i += 4;

  /* Terminate the packet with a carriage-return */
  *i++ = '\r';

  tcflush(fd, TCIOFLUSH); /* Throw away any pending I/O */

  int result = write(fd, &s, info_length + 18);
  if ( result < 18 ) {
    perror("write to SEPLOS BMS");
    exit(1);
  }
  tcdrain(fd);

  /*
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

  /*
   * Becuase of the tcdrain() above, the BMC should have the command.
   * Timeout of the read here is an unusual event, and likely means that the BMC got
   * unplugged or went into hibernation.
   * FIX: Use sigaction instead of signal.
   */
  signal(SIGALRM, got_alarm);
  alarm(10);
  result = read(fd, response, 18);
  alarm(0);
  signal(SIGALRM, SIG_DFL);
  
  return 0;
}

static float
seplos_protocol_version(int fd, unsigned int address)
{
  Seplos_2_0	response = {};
  /*
   * For this command: BMS parses the address, but not the pack number.
   */
  char		pack_info[2] = "00";

  const unsigned int status = bms_command(
   fd,
   address,		/* Address */
   PROTOCOL_VER_GET,	/* command */
   pack_info,		/* pack number */
   sizeof(pack_info),	/* length of the above */
   &response);

  if ( status != 0 ) {
    fprintf(stderr, "Bad response %x from SEPLOS BMS.\n", status);
    return -1.0;
  }

  return (response.version_code[0] - '0') + ((response.version_code[1] - '0') / 10.0);
}

int seplos_open(const char * serial_device)
{
  struct termios t = {};

  const int fd = open(serial_device, O_RDWR|O_CLOEXEC|O_NOCTTY, 0);
  if ( fd < 0 ) {
    perror(serial_device);
    return -1;
  }

  tcgetattr(fd, &t);
  cfsetspeed(&t, 19200);
  cfmakeraw(&t);
  tcflush(fd, TCIOFLUSH); /* Throw away any pending I/O */
  tcsetattr(fd, TCSANOW, &t);

  return fd;
}

int
main(int argc, char * * argv)
{
  int fd = seplos_open("/dev/ttyUSB0");

  if ( fd < 0 )
    return 1;

  fprintf(stderr, "%3.1f\n", seplos_protocol_version(fd, 0));
  return 0;
}
