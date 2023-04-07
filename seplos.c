#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct _Seplos_2_0 {
  char  start_code;      /* Always "~" */
  char  version_code[2]; /* Always "2", "0" for protocol version 2.0 */
  char  address_code[2]; /* ASCII value from "0" to "15" */
  char  device_code[2];  /* Always "4", "6" for a battery */
  char  function_code[2];/* Command or reply ID */
  char  length_code[4];  /* Length (12 bits) and length checksum (4 bits). */
  union _seplos_data {
    char info[4095 + 4 + 1];/* "info" field, checksum, 0xD to end the packet */
    char command_group[2];/* BINARY bank dip-switch address: 0 to 0xF */
  } data;
  char  guard;            /* The pointer into "data" must never reach this address */
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
  Seplos_2_0	s = {};
  return 0;
}
