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
  char  start;      /* Always '~' */
  char  version[2]; /* Always '2', '0' for protocol version 2.0 */
  char  address[2]; /* ASCII value from '0' to '15' */
  char  device[2];  /* Always '4', '6' for a battery */
  char  function[2];/* Command or reply ID */
  char  length[4];  /* Length (12 bits) and length checksum (4 bits). */
  char  info[4095 + 4 + 1];/* "info" field, checksum, 0xD to end the packet */
};

struct _Seplos_2_O_binary {
  uint8_t	version;
  uint8_t	address;
  uint8_t	device;
  uint8_t	function;
  uint16_t	length;
  uint8_t	info[2047 + 2];
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
length_checksum(unsigned int length)
{
  const unsigned int sum = ((length >> 8) & 0xf) + ((length >> 4) & 0x0f) + (length & 0x0f);
  return (((~(sum & 0xff)) + 1) << 12) & 0xf000;
}

static unsigned int
overall_checksum(const char * restrict data, unsigned int length)
{
  unsigned int sum = 0;

  for ( unsigned int i = 0; i < length; i++ ) {
    sum += *data++;
  }

  return (~sum) + 1;
}

static int
hextoi(char c)
{
  if ( c >= '0' && c <= '9' )
    return c - '0';
  else if ( c >= 'a' && c <= 'f' )
    return c - 'a' + 10;
  else if ( c >= 'A' && c <= 'F' )
    return c - 'A' + 10;
  else {
    fprintf(stderr, "Bad hex character \"%c\" (decimal %d).\n");
    return -1;
  }
}

static int
hex2toi(const char ascii[2])
{
  int high = hextoi(ascii[0]);
  int low = hextoi(ascii[1]);

  if ( high < 0 || low < 0 )
    return -1;

  return (high * 0x10) + low;
}

static int
hex4toi(const char ascii[4])
{
  int a = hextoi(ascii[0]);
  int b = hextoi(ascii[1]);
  int c = hextoi(ascii[2]);
  int d = hextoi(ascii[3]);

  if ( a < 0 || b < 0 || c < 0 || d < 0 )
    return -1;

  return (a * 0x1000) + (b * 0x0100) + (c * 0x0010) + d;
}

static int
bms_command(
 int			fd,
 const unsigned int	address,
 const unsigned int	command,
 const char * restrict	info,
 const unsigned int	info_length,
 Seplos_2_0 * restrict	r)
{
  Seplos_2_0    s = {};

  char *        i = s.info;

  s.start = '~';

  hex2(0x20, s.version); /* Protocol version 2.0 */
  hex2(address, s.address);
  hex2(0x46, s.device); /* It's a battery */
  hex2(command, s.function);

  assert(info_length < 4095);
  memcpy(i, info, info_length);
  i += info_length;

  /*
   * length is the ASCII representation of the length of the data in .info and
   * a checksum
   */
  unsigned int length_id = length_checksum(info_length) | (info_length & 0x0fff);
  hex4(length_id, s.length);

  hex4(overall_checksum(s.version, info_length + 17), i);
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
   * Becuase of the tcdrain() above, the BMC should have the command.
   * There should always be at least 18 bytes in a properly-formed packet.
   * Timeout of the read here is an unusual event, and likely means that the BMC got
   * unplugged or went into hibernation.
   * FIX: Use sigaction instead of signal.
   */
  signal(SIGALRM, got_alarm);
  alarm(10);
  result = read(fd, r, 18);
  alarm(0);
  signal(SIGALRM, SIG_DFL);
  /*
   * FIX: Validate the data here. If the response code isn't NORMAL,
   * Return the response code and no data.
   * If the length checksum isn't right, return an
   * error code.
   *
   * If data is valid, and there's more data, read the rest of the packet here.
   * Then validate the overall checksum.
   */
/*
  int version = hex2toi(r->version);
  int response = hex2toi(r->function);
  int address = hex2toi(r->address);
  int device = hex2toi(r->device);
  int status = hex2toi(r->function);
  int length = hex4toi(r->length);
  if ( version < 0 || response < 0 || address < 0 || device < 0
   || status < 0 || length < 0 ) {
    fprintf(stderr, "Non-hexidecimal character where only hexidecimal was expected.\n");
  }

  if ( length_checksum(length & 0x0fff) != (length & 0xf000) ) {
	  fprintf(stderr, "Length code incorrect.");
	  return -1; 
  }
 */
  
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

  return (response.version[0] - '0') + ((response.version[1] - '0') / 10.0);
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
  int fd = seplos_open("/dev/ttyUSB1");

  if ( fd < 0 )
    return 1;

  fprintf(stderr, "%3.1f\n", seplos_protocol_version(fd, 0));
  return 0;
}
