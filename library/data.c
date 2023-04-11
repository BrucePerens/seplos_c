#include "./internal.h"
#include "./communication.h"

int
seplos_data(seplos_device fd, unsigned int address, unsigned int pack, SeplosData * m)
{
  Seplos_2_0	telemetry = {};
  Seplos_2_0	telecommand = {};
  uint8_t	pack_info[2];
  bool		invalid;

  _sp_hex2(pack, pack_info);

  int status = _sp_bms_command(
   fd,
   address,		/* Address */
   TELEMETRY_GET,	/* command */
   &pack_info,		/* pack number */
   sizeof(pack_info),	/* length of the above */
   &telemetry);

  if ( status != NORMAL ) {
    _sp_error("Bad response %x from SEPLOS BMS.\n", status);
    return -1;
  }

  status = _sp_bms_command(
   fd,
   address,		/* Address */
   TELECOMMAND_GET,	/* command */
   &pack_info,		/* pack number */
   sizeof(pack_info),	/* length of the above */
   &telecommand);

  if ( status != 0 ) {
    _sp_error("Bad response %x from SEPLOS BMS.\n", status);
    return -1;
  }

  const Seplos_2_0_Telemetry const * t = &(telemetry.telemetry);
  const Seplos_2_0_Telecommand const * c = &(telecommand.telecommand);

  m->controller_address = address;
  m->battery_pack_number = pack;

  m->number_of_cells = _sp_hex2b(t->number_of_cells, &invalid);

  m->lowest_cell_voltage = 1000.0;
  m->highest_cell_voltage = -1000.0;
  for ( int i = 0; i < 16; i++ ) {
    const float value = _sp_hex4b(t->cell_voltage[i], &invalid) / 1000.0;
    m->cell_voltage[i] = value;
    if ( value > m->highest_cell_voltage )
      m->highest_cell_voltage = value;
    if ( value < m->lowest_cell_voltage )
      m->lowest_cell_voltage = value;
  }

  m->lowest_temperature = 1000.0;
  m->highest_temperature = -1000.0;
  for ( int i = 0; i < 6; i++ ) {
    const float value = (_sp_hex4b(t->temperature[i], &invalid) - 2731) / 10.0;
    m->temperature[i] = value;
    if ( value > m->highest_temperature )
      m->highest_temperature = value;
    if ( value < m->lowest_temperature )
      m->lowest_temperature = value;
  }

  /* Charge-discharge current is a twos-complement number. */
  int current = _sp_hex4b(t->charge_discharge_current, &invalid);
  if ( current & 0x8000 )
    m->charge_discharge_current = (~current) / -100.0;
  else
    m->charge_discharge_current = current / 100.0;

  m->total_battery_voltage = _sp_hex4b(t->total_battery_voltage, &invalid) / 100.0;
  m->residual_capacity = _sp_hex4b(t->residual_capacity, &invalid) / 100.0;
  m->battery_capacity = _sp_hex4b(t->battery_capacity, &invalid) / 100.0;
  m->state_of_charge = _sp_hex4b(t->state_of_charge, &invalid) / 10.0;
  m->rated_capacity = _sp_hex4b(t->rated_capacity, &invalid) / 100.0;
  m->number_of_cycles = _sp_hex4b(t->number_of_cycles, &invalid);
  m->state_of_health = _sp_hex4b(t->state_of_health, &invalid) / 10.0;
  m->port_voltage = _sp_hex4b(t->port_voltage, &invalid) / 100.0;

  for (int i = 0; i < SEPLOS_N_CELLS; i++ ) {
    m->cell_alarm[i] = _sp_hex2b(c->cell_alarm[i], &invalid);
  }
  for (int i = 0; i < SEPLOS_N_TEMPERATURES; i++ ) {
    m->temperature_alarm[i] = _sp_hex2b(c->temperature_alarm[i], &invalid);
  }
  m->charge_discharge_current_alarm = _sp_hex2b(c->charge_discharge_current_alarm, &invalid);
  m->total_battery_voltage_alarm = _sp_hex2b(c->total_battery_voltage_alarm, &invalid);

  m->bit_alarm[0] = _sp_hex2b(c->alarm_1_through_6[0], &invalid) \
   | (_sp_hex2b(c->alarm_1_through_6[1], &invalid) << 8) \
   | (_sp_hex2b(c->alarm_1_through_6[2], &invalid) << 16) \
   | (_sp_hex2b(c->alarm_1_through_6[3], &invalid) << 24);

  m->bit_alarm[1] = _sp_hex2b(c->alarm_1_through_6[4], &invalid) \
   | (_sp_hex2b(c->alarm_1_through_6[5], &invalid) << 8) \
   | (_sp_hex2b(c->alarm_7_and_8[0], &invalid) << 16) \
   | (_sp_hex2b(c->alarm_7_and_8[1], &invalid) << 24);

  m->equilibrium_state = _sp_hex2b(c->equilibrium_state[0], &invalid) \
   | (_sp_hex2b(c->equilibrium_state[1], &invalid) << 8);

  m->disconnection_state = _sp_hex2b(c->disconnection_state[0], &invalid) \
   | (_sp_hex2b(c->disconnection_state[1], &invalid) << 8);

  uint8_t state = _sp_hex2b(c->on_off_state, &invalid);
  m->discharge_switch = !!(state & 0x01);
  m->charge_switch = !!(state & 0x02);
  m->current_limit_switch = !!(state & 0x04);
  m->heating_switch = !!(state & 0x08);

  state = _sp_hex2b(c->system_state, &invalid);
  m->discharge = (state & 0x01);
  m->charge = (state & 0x02);
  m->floating_charge = (state & 0x04);
  m->standby = (state & 0x10);
  m->shutdown = (state & 0x20);

  if ( m->total_battery_voltage_alarm != NORMAL ) {
    m->has_alarm = m->has_voltage_or_current_alarm = true;
    switch ( m->total_battery_voltage_alarm ) {
    case LOW_LIMIT_HIT:
      m->depleted = true;
      break;
    case HIGH_LIMIT_HIT:
      m->overcharge = true;
      break;
    case OTHER_ALARM:
    default:
      m->other_or_undocumented_alarm_state = true;
    }
  }

  if ( m->charge_discharge_current_alarm != NORMAL ) {
    m->has_alarm = m->has_voltage_or_current_alarm = true;
    switch ( m->charge_discharge_current_alarm ) {
    case LOW_LIMIT_HIT:
    case HIGH_LIMIT_HIT:
      break;
    case OTHER_ALARM:
    default:
      m->other_or_undocumented_alarm_state = true;
    }
  }

  for ( unsigned int i = 0; i < SEPLOS_N_CELLS; i++ ) {
    const uint8_t value = m->cell_alarm[i];
    if ( value != 0 ) {
      m->has_alarm = m->has_cell_alarm = true;
      switch ( value ) {
      case LOW_LIMIT_HIT:
        m->depleted = true;
        break;
      case HIGH_LIMIT_HIT:
        m->overcharge = true;
        break;
      case OTHER_ALARM:
      default:
        m->other_or_undocumented_alarm_state = true;
      }
      break;
    }
  }

  for ( unsigned int i = 0; i < SEPLOS_N_TEMPERATURES; i++ ) {
    const uint8_t value = m->temperature_alarm[i];
    if ( value != NORMAL ) {
      m->has_alarm = m->has_temperature_alarm = true;
      switch ( value ) {
      case LOW_LIMIT_HIT:
        m->cold = true;
        break;
      case HIGH_LIMIT_HIT:
        m->hot = true;
        break;
      case OTHER_ALARM:
      default:
        m->other_or_undocumented_alarm_state = true;
        break;
      }
    }
  }

  for ( int i = 0; i < (sizeof(m->bit_alarm) / sizeof(*(m->bit_alarm))); i++ ) {
    if ( m->bit_alarm[i] ) {
      m->has_alarm = m->has_bit_alarm = true;
      break;
    }
  }
}
