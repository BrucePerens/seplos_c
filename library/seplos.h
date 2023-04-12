#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define SEPLOS_N_CELLS 16
#define SEPLOS_N_TEMPERATURES 6
#define SEPLOS_N_BIT_ALARMS 64

typedef int	seplos_device; /* File descriptor on POSIX */

/*
 * This is the structure that all other software will use to montior the battery.
 * All of the communications, validation, and data conversion to the native data
 * format are done for the user.
 *
 * Alarms are an array, their names are in seplos_byte_alarm_names[] and
 * seplos_bit_alarm_names[]. Byte alarms are normal if 0, 1 means the lower
 * limit was reached, 2 means the upper limit was reached, 0xf0 means
 * "other alarms". Bit alarms are in alarm state if they are set.
 * 
 * The SEPLOS documentation on numeric conversion is confusing.
 * Nothing is in base 10, it's all hexidecimal like the rest of the data.
 * It's all unsigned fixed-point with one exception, the charge-discharge
 * current is a twos-complement signed number, fixed-point.
 *
 * The point is in various places for different types of value.
 * The number read is divided by:
 * * 1000.0 for cell voltages
 * * 100.0 for temperatures, and the result is in Kelvin degrees.
 * * 10.0 for the state-of-health.
 *
 * The one lonely integer value, the cycle count, is hexidecimal.
 * I haven't read the time from the battery yet. And things like history and
 * getting/setting configuration values are not documented.
 */
typedef struct _SeplosData {
  uint8_t	controller_address;
  uint8_t	battery_pack_number;

  /*
   * has_alarm will be true if cell, temperature, voltage, current, bit alarms,
   * depleted, overcharge, cold, or hot are true.
   */
  bool		has_alarm;
  /*
   * This is true if any of the byte alarms are 0xf0, or any value other than
   * 1 and 2.
   */
  bool		other_or_undocumented_alarm_state;

  bool		has_cell_alarm;
  /*
   * cold or hot will be true if any of temperature alarms indicate cold or hot.
   */
  bool		has_temperature_alarm;
  /*
   * This will be true if charge_discharge_current or total_battery_voltage are
   * in any alarm state. It doesn't indicate any of the cell voltage alarms.
   */
  bool		has_voltage_or_current_alarm;
  /*
   * This will be true if any of the 64 bit alarms are true.
   */
  bool		has_bit_alarm;
  /*
   * This will be true if any of cell_alarm[*] or total_battery_voltage indicates
   * a low-voltage limit hit. 
   */
  bool		depleted; /* A cell or the battery voltage is too low. */
  /*
   * This will be true if any of cell_alarm[*] or total_battery_voltage indicates
   * a high-voltage limit hit. 
   */
  bool		overcharge; /* A cell or the battery voltage is too high. */

  /*
   * This will be true if any of the temperature alarms indicate the low temperature
   * limit hit. 
   */
  bool		cold; /* One of the temperature sensors is too cold. */
  /*
   * This will be true if any of the temperature alarms indicate the high temperature
   * limit hit. 
   */
  bool		hot; /* One of the temperature sensors is too hot. */

  /*
   * The lowest temperature reported by the 6 temperature sensors.
   */
  float		lowest_temperature;

  /*
   * The highest temperature reported by the 6 temperature sensors.
   */
  float		highest_temperature;

  /*
   * The lowest cell voltage reported.
   */
  float		lowest_cell_voltage;

  /*
   * The highest cell voltage reported.
   */
  float		highest_cell_voltage;

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
  bool		current_limit_switch;
  bool		heating_switch;
  float		cell_voltage[SEPLOS_N_CELLS];
  float		temperature[SEPLOS_N_TEMPERATURES];
  uint16_t	equilibrium_state;
  uint16_t	disconnection_state;
  /*
   * An alarm state is abnormal. All of the status that would be set in normal
   * operations is stored elsewhere in this structure, so if any of the byte
   * or bit alarms are set, the user software should indicate an alarm state,
   * notify the user, etc.
   */
  uint8_t	cell_alarm[SEPLOS_N_CELLS];
  uint8_t	temperature_alarm[SEPLOS_N_TEMPERATURES];
  uint8_t	charge_discharge_current_alarm;
  uint8_t	total_battery_voltage_alarm;
  /* Bit alarms are in a bit-field here, rather than bool, to make them quick to scan. */
  uint32_t	bit_alarm[(SEPLOS_N_BIT_ALARMS / 32) + !!(SEPLOS_N_BIT_ALARMS % 32)];
} SeplosData;

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

enum _seplos_byte_alarm_value {
  NORMAL = 0x00,
  LOW_LIMIT_HIT = 1,
  HIGH_LIMIT_HIT = 2,
  OTHER_ALARM = 0xf0
};
enum _seplos_response {
  /* NORMAL = 0x00,                 Normal response. Same as above, don't redeclare */
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

extern const char const * seplos_bit_alarm_names[SEPLOS_N_BIT_ALARMS];
extern const char const * seplos_temperature_names[SEPLOS_N_TEMPERATURES];

extern int		seplos_data(seplos_device fd, unsigned int address, unsigned int pack, SeplosData * m);
extern seplos_device	seplos_open(const char * serial_device);
extern float		seplos_protocol_version(seplos_device fd, unsigned int address);
extern void		seplos_html(FILE * f, const SeplosData const * m, bool longer);
extern void		seplos_text(FILE * f, const SeplosData const * m, bool longer);
