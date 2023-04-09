/*
 * Seplos BMS communication protocol 2.0
 * Bruce Perens K6BP <bruce@perens.com>
 *
 * Copyright (C) 2023 Algorithmic LLC. 
 * Licensed under the Affero GPL 3.0
 *
 * I use share-and-share-alike licenses on work I've not been paid for.
 * Want this under a gift-style license like Apache 2? Pay me for the work I've done.
 *
 * Although the SEPLOS document refers to this as a Modbus-ASCII protocol, it isn't
 * one. They're confusing the Modbus-ASCII _protocol_, which they don't use, with the
 * RS-485 _transport_, which they use. It seems common to confuse the two. This is more
 * properly called an ASCII-over-RS-485 protocol. Modbus-ASCII packets start with ':'
 * rather than the '~' used by SEPLOS, and the packet format is entirely different.
 *
 * Limitations:
 * * I haven't tested this with a second battery connected to the first.
 * * At this writing, I've not installed the battery where the charger is. Thus
 *   some issues of what is a normal vs. alarm condition, and how the numeric
 *   conversions actually work, will wait until I have the battery installed at
 *   my remote site.
 *
 * WARNING:
 * The battery is a high-energy device. It's dangerous!
 * Please read the warnings in the README file carefully and completely.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef struct _Seplos_2_0_telemetry {
  uint8_t	data_flag[2];
  uint8_t	command_group[2];
  uint8_t	number_of_cells[2];
  uint8_t	cell_voltage[16][4];
  uint8_t	number_of_temperatures[2];
  uint8_t	temperature[6][4];
  uint8_t	charge_discharge_current[4];
  uint8_t	total_battery_voltage[4];
  uint8_t	residual_capacity[4];
  uint8_t	number_of_custom_fields[2];
  uint8_t	battery_capacity[4];
  uint8_t	state_of_charge[4];
  uint8_t	rated_capacity[4];
  uint8_t	number_of_cycles[4];
  uint8_t	state_of_health[4];
  uint8_t	port_voltage[4];
  uint8_t	reserved[4][4];
} Seplos_2_0_telemetry;

typedef struct _Seplos_2_0_telecommand {
} Seplos_2_0_telecommand;

typedef struct _Seplos_2_0 {
  char  start;      /* Always '~' */
  char  version[2]; /* Always '2', '0' for protocol version 2.0 */
  char  address[2]; /* ASCII value from '0' to '15' */
  char  device[2];  /* Always '4', '6' for a battery */
  char  function[2];/* Command or reply ID */
  char  length[4];  /* Length (12 bits) and length checksum (4 bits). */
  union _data {
    char  info[4095 + 4 + 1];/* "info" field, checksum, 0xD to end the packet */
    Seplos_2_0_telemetry telemetry;
    Seplos_2_0_telecommand telecommand;
  } data;
} Seplos_2_0;

typedef struct _Seplos_2_O_binary {
  uint8_t	version;
  uint8_t	address;
  uint8_t	device;
  uint8_t	function;
  uint16_t	length;
} Seplos_2_0_binary;

/*
 * Monitoring information from the battery pack.
 */

#define SEPLOS_N_CELLS 16
#define SEPLOS_N_TEMPERATURES 16
#define SEPLOS_N_BYTE_ALARMS 24
#define SEPLOS_N_BIT_ALARMS 73

const char const * seplos_byte_alarm_names[SEPLOS_N_BYTE_ALARMS] = {
  "Cell alarm 1",
  "Cell alarm 2",
  "Cell alarm 3",
  "Cell alarm 4",
  "Cell alarm 5",
  "Cell alarm 6",
  "Cell alarm 7",
  "Cell alarm 8",
  "Cell alarm 9",
  "Cell alarm 10",
  "Cell alarm 11",
  "Cell alarm 12",
  "Cell alarm 13",
  "Cell alarm 14",
  "Cell alarm 15",
  "Cell alarm 16",
  "Cell temperature alarm 1",
  "Cell temperature alarm 2",
  "Cell temperature alarm 3",
  "Cell temperature alarm 4",
  "Environment temperature alarm",
  "Power temperature alarm 1",
  "Charge/discharge current alarm",
  "Total battery voltage alarm"
};

const char const * seplos_bit_alarm_names[SEPLOS_N_BIT_ALARMS] = {
  /* Alarm event 1 */
  "Voltage sensor fault",
  "Temperature sensor fault",
  "Current sensor fault",
  "Key switch fault",
  "Cell voltage dropout fault",
  "Charge switch fault",
  "Discharge switch fault",
  "Current-limit switch fault",
  /* Alarm event 2 */
  "Monomer high-voltage alarm",
  "Monomer overvoltage protection",
  "Monomer low-voltage alarm",
  "Monomer under-voltage protection",
  "High voltage alarm for total voltage",
  "Overvoltage protection for total voltage",
  "Low voltage alarm for total voltage",
  "Under voltage protection for total voltage",
  /* Alarm event 3 */
  "Charge high-temperature alarm",
  "Charge over-temperature protection",
  "Charge low-temperature alarm",
  "Charge under-temperature protection",
  "Discharge high-temperature alarm",
  "Discharge over-temperature protection",
  "Discharge low-temperature alarm",
  "Discharge under-temperature protection",
  /* Alarm event 4 */
  "Environment high-temperature alarm",
  "Environment over-temperature protection",
  "Environment low-temperature alarm",
  "Environment under-temperature protection",
  "Power over-temperature protection",
  "Power high-temperature alarm",
  "Cell low-temperature heating",
  0,
  /* Alarm event 5 */
  "Charge over-current alarm",
  "Charge over-current protection",
  "Discharge over-current alarm",
  "Discharge over-current protection",
  "Transient over-current protection",
  "Output short-circuit protection",
  "Transient over-current lockout",
  "Output short-circuit lockout",
  /* Alarm event 6 */
  "Charge high-voltage protection",
  "Intermittent recharge waiting",
  "Residual capacity alarm",
  "Residual capacity protection",
  "Cell low-voltage charging prohibition",
  "Output reverse-polarity protection",
  "Output connection fault",
  0,
  /*
   * Skip alarm event 7, the only defined bits are represented as bools
   * in Seplos_monitoring.
   */
  /* Alarm Event 8 */
  "EEPROM storage fault",
  "Real Time Clock error",
  "Voltage calibration not performed",
  "Current calibration not performed",
  "Zero calibration not performed",
  0,
  0,
  0,
  /*
   * I don't know if the cell disconnection state happens normally during equalization,
   * or if it's an error state. I only keep things that are actual alarm conditions
   * in .bit_alarms[], so if this happens normally, it should be moved to its own array.
   */
  /* Disconnection state 1 */
  "Cell 01 disconnection",
  "Cell 02 disconnection",
  "Cell 03 disconnection",
  "Cell 04 disconnection",
  "Cell 05 disconnection",
  "Cell 06 disconnection",
  "Cell 07 disconnection",
  "Cell 08 disconnection",
  /* Disconnection state 2 */
  "Cell 09 disconnection",
  "Cell 10 disconnection",
  "Cell 11 disconnection",
  "Cell 12 disconnection",
  "Cell 13 disconnection",
  "Cell 14 disconnection",
  "Cell 15 disconnection",
  "Cell 16 disconnection",
};

const char const * seplos_temperature_names[SEPLOS_N_TEMPERATURES] = {
  "Cell temperature 1",
  "Cell temperature 2",
  "Cell temperature 3",
  "Cell temperature 4",
  "Environment temperature",
  "Power temperature"
};

/*
 * This is the structure that all other software will use to montior the battery.
 * All of the communications, validation, and data conversion to the native data
 * format are done for the user.
 *
 * Alarms are an array, their names are in seplos_byte_alarm_names[] and
 * seplos_bit_alarm_names[]. Byte alarms are normal if 0, 1 means the lower
 * limit was reached, 2 means the upper limit was reached, 0xf0 means
 * "other alarms". Bit alarms are represented as C bool, and are in alarm
 * state if they are true.
 * 
 * The documentation leaves a few issues of the numeric conversion for clarification
 * via experimentation. Or maybe it's wrong. Here's what it claims:
 *
 * Temperatures are in Celsius and are signed, they are based on 4-digit BASE-10,
 * not hexidecimal, ASCII unsigned Kelvin temperature in a representation of xxx.y .
 *
 * All of the other float values are based upon 4-digit BASE-10, not hexidecimal, ASCII
 * numbers with a fixed-point representation * of xx.yy, so have a range of 0.0 to 99.99;
 * _except_ that the documentation claims that total current is signed! Is it
 * twos complement? Or do they stick a '-' in an otherwise hexidecimal stream?
 *
 * The one lonely integer value, the cycle count, may be BASE-10 or hexidecimal.
 */
typedef struct _Seplos_monitor {
  unsigned int	number_of_cells;
  float		charge_discharge_current;
  float		total_battery_voltage;
  float		residual_capacity; /* amp hours */
  float		battery_capacity; /* amp hours */
  float		state_of_charge; /* percentage */
  float		rated_capacity; /* amp hours */
  unsigned int	number_of_cycles;
  float		state_of_health; /* Ratio of current maximum charge to rated capacity */
  float		port_voltage;
  bool		discharge;
  bool		charge;
  bool		floating_charge;
  bool		standby;
  bool		shutdown;
  bool		discharge_switch;
  bool		charge_switch;
  bool		current_switch;
  bool		heating_switch;
  bool		automatic_charging_waiting;
  bool		c_charging_waiting;
  bool		cell_equilibrium[SEPLOS_N_CELLS];
  float		cell_voltage[SEPLOS_N_CELLS];
  float		temperature[SEPLOS_N_TEMPERATURES];
  /*
   * An alarm state is abnormal. All of the status that would be set in normal
   * operations is stored elsewhere in this structure, so if any of the byte
   * or bit alarms are set, the user software should indicate an alarm state,
   * notify the user, etc.
   */
  uint8_t	byte_alarm[SEPLOS_N_BYTE_ALARMS];
  /* Bit alarms are in a bit-field here, rather than bool, to make them quick to scan. */
  uint32_t	bit_alarms[(SEPLOS_N_BIT_ALARMS / 32) + !!(SEPLOS_N_BIT_ALARMS % 32)];
} Seplos_monitor;

/* The comments are as SEPLOS documented the names of these commands */
enum _seplos_commands {
  TELEMETRY_GET =     0x42,    /* Acquisition of telemetering information */
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
error(const char * restrict pattern, ...)
{
  va_list args;

  va_start(args, pattern);
  fflush(stdout);
  vfprintf(stderr, pattern, args);
  fflush(stderr);
  va_end(args);
}

static void
hex1(uint8_t value, char ascii[1])
{
  ascii[0] = hex[value & 0xf];
}

static void
hex2(uint8_t value, char ascii[2])
{
  ascii[0] = hex[(value >> 4) & 0xf];
  ascii[1] = hex[value & 0xf];
}

static void
hex4(uint16_t value, char ascii[4])
{
  ascii[0] = hex[(value >> 12) & 0xf];
  ascii[1] = hex[(value >> 8) & 0xf];
  ascii[2] = hex[(value >> 4) & 0xf];
  ascii[3] = hex[value & 0xf];
}

static uint8_t
hex1b(uint8_t c, bool * invalid)
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

static uint8_t
hex2b(char ascii[2], bool * invalid)
{
  return (hex1b(ascii[0], invalid) << 4) | hex1b(ascii[1], invalid);
}

static uint16_t
hex4b(char ascii[4], bool * invalid)
{
  return (hex1b(ascii[0], invalid) << 12) | (hex1b(ascii[1], invalid) << 8) | \
   (hex1b(ascii[2], invalid) << 4) | hex1b(ascii[3], invalid);
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

  return ((~sum) & 0xffff) + 1;
}

static unsigned int
read_serial(int fd, void * data, size_t size)
{
  /* FIX: Fill in the timeout here. */

  size_t received_amount = 0;

  while ( received_amount < size ) {
    int ret = read(fd, data, size - received_amount);
    if ( ret < 0 ) {
      error("Read failed: %s\n", strerror(errno));
      return ret;
    }
    else if ( ret == 0 ) {
      /* Read should always block until it receives at least one character */
      error("Serial end-of-file.\n");
      return -1;
    }
    else {
      received_amount += ret;
      data += ret;
    }
  }
  return received_amount;
}

static int
bms_command(
 int		       fd,
 const unsigned int    address,
 const unsigned int    command,
 const void * restrict info,
 const unsigned int    info_length,
 Seplos_2_0 *	       result)
{
  Seplos_2_0        encoded = {};
  Seplos_2_0_binary s = {};
  Seplos_2_0_binary r = {};

  s.version = 0x20; /* Protocol version 2.0 */
  s.address = address;
  s.device = 0x46;  /* Code for a battery */
  s.function = command;
  s.length = length_checksum(info_length) | (info_length & 0x0fff);

  hex2(s.version, encoded.version);
  hex2(s.address, encoded.address);
  hex2(s.device, encoded.device);
  hex2(s.function, encoded.function);
  hex4(s.length, encoded.length);

  encoded.start = '~';
  assert(info_length < 4096);

  uint8_t * i = encoded.data.info;
  memcpy(i, info, info_length);
  i += info_length;

  uint16_t checksum = overall_checksum(encoded.version, info_length + 12);
  hex4(checksum, i);
  i += 4;

  *i++ = '\r';

  tcflush(fd, TCIOFLUSH); /* Throw away any pending I/O */

  int ret = write(fd, &encoded, info_length + 18);
  if ( ret != info_length + 18 ) {
    error("Write: %s\n", strerror(errno));
    exit(1);
  }
  tcdrain(fd);

  /*
   * Becuase of the tcdrain() above, the BMC should have the command.
   * There should always be at least 18 bytes in a properly-formed packet.
   * Timeout of the read here is an unusual event, and likely means that the BMC got
   * unplugged or went into hibernation.
   */
  ret = read_serial(fd, result, 18);

  if ( ret != 18 ) {
    error("Read %s\n", strerror(errno));
    return -1;
  }

  bool invalid = false;

  if ( result->start != '~' )
    invalid = true;

  r.version = hex2b(result->version, &invalid);
  r.address = hex2b(result->address, &invalid);
  r.device = hex2b(result->device, &invalid);
  r.function = hex2b(result->function, &invalid);
  r.length = hex4b(result->length, &invalid);

  /* Abort if the major protocol version isn't 2. Accept any minor version */
  if ( r.version > 0x2f || r.version < 0x20 ) {
    error("SEPLOS protocol %x not implemented.\n");
    return -1;
  }

  if ( invalid ) {
    error("Non-hexidecimal character where only hexidecimal was expected.\n");
    return -1;
  }

  if ( length_checksum(r.length & 0x0fff) != (r.length & 0xf000) ) {
    error("Length code incorrect.");
    return -1; 
  }
 
  r.length &= 0x0fff;
  
  if ( r.length > 0 ) {
    ret = read_serial(fd, &(result->data.info[5]), r.length);
    if ( ret != r.length ) {
      error("Info read: %s\n", strerror(errno));
      return -1;
    }
  }
  
  for ( unsigned int j = 0; j < r.length + 4; j++ ) {
    uint8_t c = result->data.info[j];
    if ( !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) ) {
      error("Non-hexidecimal character where only hexidecimal was expected.\n");
      return -1;
    }
  }

  checksum = hex4b(&(result->data.info[r.length]), &invalid);
  if ( invalid || checksum != overall_checksum(result->version, r.length + 12) ) {
    error("Checksum mismatch.\n");
    return -1;
  }

  if ( r.function != NORMAL ) {
    error("Return code %x.\n", r.function);
  }
  return r.function;
}

float
seplos_protocol_version(int fd, unsigned int address)
{
  Seplos_2_0	response = {};
  /*
   * For this command: BMS parses the address, but not the pack number.
   */
  uint8_t	pack_info[2] = "00";

  const unsigned int status = bms_command(
   fd,
   address,		/* Address */
   PROTOCOL_VER_GET,	/* command */
   &pack_info,		/* pack number */
   sizeof(pack_info),	/* length of the above */
   &response);

  if ( status != 0 ) {
    error("Bad response %x from SEPLOS BMS.\n", status);
    return -1.0;
  }

  bool invalid = false;
  uint16_t version = hex2b(response.version, &invalid);

  return ((version >> 4) & 0xf) + ((version & 0xf) * 0.1);
}

int
seplos_monitor(int fd, unsigned int address, unsigned int pack, Seplos_monitor * m)
{
  Seplos_2_0	telemetry = {};
  Seplos_2_0	telecommand = {};
  uint8_t	pack_info[2];
  bool		invalid;

  hex2(pack, pack_info);

  int status = bms_command(
   fd,
   address,		/* Address */
   TELEMETRY_GET,	/* command */
   &pack_info,		/* pack number */
   sizeof(pack_info),	/* length of the above */
   &telemetry);

  if ( status != 0 ) {
    error("Bad response %x from SEPLOS BMS.\n", status);
    return -1;
  }

  status = bms_command(
   fd,
   address,		/* Address */
   TELECOMMAND_GET,	/* command */
   &pack_info,		/* pack number */
   sizeof(pack_info),	/* length of the above */
   &telecommand);

  if ( status != 0 ) {
    error("Bad response %x from SEPLOS BMS.\n", status);
    return -1;
  }

  Seplos_2_0_telemetry * t = &(telemetry.data.telemetry);

  m->state_of_health = hex4b(t->state_of_health, &invalid) / 10.0;

  for ( int i = 0; i < 16; i++ ) {
    m->cell_voltage[i] = hex4b(t->cell_voltage[i], &invalid) / 1000.0;
  }

  for ( int i = 0; i < 6; i++ ) {
    m->temperature[i] = hex4b(t->temperature[i], &invalid) / 100.0;
  }

#if 0
  unsigned int	number_of_cells;
  float		charge_discharge_current;
  float		total_battery_voltage;
  float		residual_capacity; /* amp hours */
  float		battery_capacity; /* amp hours */
  float		state_of_charge; /* percentage */
  float		rated_capacity; /* amp hours */
  unsigned int	number_of_cycles;
  float		state_of_health; /* Ratio of current maximum charge to rated capacity */
  float		port_voltage;
  bool		discharge;
  bool		charge;
  bool		floating_charge;
  bool		standby;
  bool		shutdown;
  bool		discharge_switch;
  bool		charge_switch;
  bool		current_switch;
  bool		heating_switch;
  bool		automatic_charging_waiting;
  bool		c_charging_waiting;
  bool		cell_equilibrium[SEPLOS_N_CELLS];
  float		cell_voltage[SEPLOS_N_CELLS];
  float		temperature[SEPLOS_N_TEMPERATURES];
  /*
   * An alarm state is abnormal. All of the status that would be set in normal
   * operations is stored elsewhere in this structure, so if any of the byte
   * or bit alarms are set, the user software should indicate an alarm state,
   * notify the user, etc.
   */
  uint8_t	byte_alarm[SEPLOS_N_BYTE_ALARMS];
  /* Bit alarms are in a bit-field here, rather than bool, to make them quick to scan. */
  uint32_t	bit_alarms[(SEPLOS_N_BIT_ALARMS / 32) + !!(SEPLOS_N_BIT_ALARMS % 32)];
#endif
}

int
seplos_open(const char * serial_device)
{
  struct termios t = {};

  const int fd = open(serial_device, O_RDWR|O_CLOEXEC|O_NOCTTY, 0);
  if ( fd < 0 ) {
    error("%s: %s\n", serial_device, strerror(errno));
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
  Seplos_monitor m;

  if ( fd < 0 )
    return 1;

  seplos_monitor(fd, 0, 0x01, &m);
  return 0;
}
