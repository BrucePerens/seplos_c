typedef struct _Seplos_2_0_Telemetry {
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
} Seplos_2_0_Telemetry;

typedef struct _Seplos_2_0_Telecommand {
  uint8_t	data_flag[2];
  uint8_t	command_group[2];
  uint8_t	number_of_cells[2];
  uint8_t	cell_alarm[16][2];
  uint8_t	number_of_temperatures[2];
  uint8_t	temperature_alarm[6][2];
  uint8_t	charge_discharge_current_alarm[2];
  uint8_t	total_battery_voltage_alarm[2];
  uint8_t	number_of_custom_alarms[2];
  uint8_t	alarm_1_through_6[6][2];
  uint8_t	on_off_state[2];
  uint8_t	equilibrium_state[2][2];
  uint8_t	system_state[2];
  uint8_t	disconnection_state[2][2];
  uint8_t	alarm_7_and_8[2][2];
  uint8_t	reserved[6][2];
} Seplos_2_0_Telecommand;

typedef struct _Seplos_2_0 {
  char  start;      /* Always '~' */
  char  version[2]; /* Always '2', '0' for protocol version 2.0 */
  char  address[2]; /* ASCII value from '0' to '15' */
  char  device[2];  /* Always '4', '6' for a battery */
  char  function[2];/* Command or reply ID */
  char  length[4];  /* Length (12 bits) and length checksum (4 bits). */
  union {
    char  info[4095 + 4 + 1];/* "info" field, checksum, 0xD to end the packet */
    Seplos_2_0_Telemetry telemetry;
    Seplos_2_0_Telecommand telecommand;
  };
} Seplos_2_0;

extern int
_sp_bms_command(
 seplos_device	       fd,
 const unsigned int    address,
 const unsigned int    command,
 const void * restrict info,
 const unsigned int    info_length,
 Seplos_2_0 *	       result);
