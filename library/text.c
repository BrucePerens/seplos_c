#include "./internal.h"

static void
cell_state_text(FILE * f, const SeplosData const * m, int offset)
{
  fprintf(f, "Cell:         ");
  for ( int i = 0; i < 8; i++ ) {
    fprintf(f, " %2d   ", i + offset);
  }
  fprintf(f, "\nVoltage:      ");
  for ( int i = 0; i < 8; i++ ) {
    const unsigned int index = i + offset;
    fprintf(f, "%.3f ", m->cell_voltage[index]);
  }
  fprintf(f, "\nEquilibrium:  ");
  for ( int i = 0; i < 8; i++ ) {
    const unsigned int index = i + offset;
    fprintf(f, "  %c   ", (m->equilibrium_state & (1 << index)) ? '*' : '-');
  }
  fprintf(f, "\nDisconnected: ");
  for ( int i = 0; i < 8; i++ ) {
    const unsigned int index = i + offset;
    fprintf(f, "  %c   ", (m->equilibrium_state & (1 << index)) ? '*' : '-');
  }
  fprintf(f, "\nTemperature:  ");
  for ( int i = 0; i < 2; i++ ) {
    const unsigned int index = i + (offset / 4);
    fprintf(f, "   %4.0f C, %4.0f F       ", m->temperature[index], _sp_farenheit(m->temperature[index]));
  }
  fprintf(f, "\n");
}

void
seplos_text(FILE * f, const SeplosData const * m, bool longer)
{
  fprintf(f, "Controller %x, battery pack %x:\n", m->controller_address, m->battery_pack_number);
  if ( m->has_alarm ) {
    fprintf(f, "!!! ALARM !!! - The battery indicates an alarm state.\n");
    fprintf(f, "Resolve this issue ASAP, or the battery may be damaged.\n");
    if ( m->depleted )
      fprintf(f, "!!! THE BATTERY IS DEPLETED OF CHARGE !!!\n");
    if ( m->overcharge )
      fprintf(f, "!!! THE BATTERY IS OVERCHARGED !!!\n");
    if ( m->hot )
      fprintf(f, "!!! THE BATTERY IS TOO HOT !!!\n");
    if ( m->cold )
      fprintf(f, "!!! THE BATTERY IS TOO COLD !!!\n");
    if ( m->other_or_undocumented_alarm_state )
      fprintf(f, "!!! The battery indicates an \"other\" or undocumented alarm state. !!!\n");

    if ( m->has_voltage_or_current_alarm ) {
      if ( m->total_battery_voltage_alarm ) {
        const char * s = "undefined voltage alarm state.";
  
        switch ( m->total_battery_voltage_alarm ) {
        case LOW_LIMIT_HIT:
          s = "exhausted: voltage was depleted below the lower limit.";
          break;
        case HIGH_LIMIT_HIT:
          s = "overcharged: voltage has exceeded the upper limit.";
          break;
        case OTHER_ALARM:
          s = "controller reports \"other\" voltage alarm state.\n";
          break;
        }

        fprintf(f, "\nTotal battery voltage: %s\n", s);
      }

      if ( m->charge_discharge_current_alarm ) {
        const char * s = "Undefined charge or discharge current alarm state.";
  
        switch ( m->charge_discharge_current_alarm ) {
        case LOW_LIMIT_HIT:
          s = "Discharge current exceeded the battery's limit.";
          break;
        case HIGH_LIMIT_HIT:
          s = "Charge current exceeded the battery's limit.";
          break;
        case OTHER_ALARM:
          s = "Controller reports \"other\" charge or discharge alarm state.\n";
          break;
        }

        fprintf(f, "%s\n", s);
      }
    }

    if ( m->has_cell_alarm ) {
      fprintf(f, "\nThe battery indicates an issue with one or more of the cells:\n");
      for ( unsigned int i = 0; i < SEPLOS_N_CELLS; i++ ) {
        uint8_t value = m->cell_alarm[i];
    
        if ( value != NORMAL ) {
          const char * s = "undefined cell alarm state.";
    
          switch ( value ) {
          case LOW_LIMIT_HIT:
            s = "exhausted: voltage was depleted below the lower limit.";
            break;
          case HIGH_LIMIT_HIT:
            s = "overcharged: voltage has exceeded the upper limit.";
            break;
          case OTHER_ALARM:
            s = "controller reports \"other\" cell alarm state.\n";
            break;
          }
           
          fprintf(f, "Cell %d: %s\n", i, s);
        }
      }
      fprintf(f, "\n");
    }

    if ( m->temperature_alarm ) {
      fprintf(f, "\nThe battery temperature is out of bounds:\n");

      for ( unsigned int i = 0; i < SEPLOS_N_TEMPERATURES; i++ ) {
        uint8_t value = m->temperature_alarm[i];
  
        if ( value != NORMAL ) {
          fprintf(f, "%s: \n", seplos_temperature_names[i]);
          const char * s = "undefined temperature state.";
   
          switch ( value ) {
          case LOW_LIMIT_HIT:
            s = "too cold: below the lower limit.";
            break;
          case HIGH_LIMIT_HIT:
            s = "too hot: above the upper limit.";
            break;
          case OTHER_ALARM:
            s = "controller reports \"other\" temperature state.\n";
          }
          fprintf(f, "Cell %d: %s\n", s);
        }
      }
    }
    if ( m->has_bit_alarm ) {
      for ( int i = 0; i < (sizeof(m->bit_alarm) / sizeof(*m->bit_alarm)); i++ ) {
        uint32_t value = m->bit_alarm[i];
        if ( value != 0 ) {
          for ( int j = 0; j < 32; j++ ) {
            const uint32_t mask = 1 << j;
            if ( (value & mask) != 0 ) {
              fprintf(f, "Alarm: %s.\n", seplos_bit_alarm_names[(i * 32) + j]);
            }
          }
        }
      }
    }
  }
  else {
    fprintf(f, "No Alarms.\n");
  }

  fprintf(f, "\nVoltage:          %.2f V\n", m->total_battery_voltage);
  fprintf(f, "Current:          %.2f A\n", m->charge_discharge_current);
  fprintf(f, "State of charge:  %.0f\%\n", m->state_of_charge);

  fprintf(f, "Temperatures:     %.0f - %.0f C, %.0f - %.0f F",
   m->lowest_temperature,
   m->highest_temperature,
   _sp_farenheit(m->lowest_temperature),
   _sp_farenheit(m->highest_temperature));

  fprintf(f, " (internal heating: %s)\n", m->heating_switch ? "ON" : "off");

  fprintf(f, "Cell voltages:    %.3f - %.3f V (unbalance: %.03f V)\n", m->lowest_cell_voltage, m->highest_cell_voltage, m->highest_cell_voltage - m->lowest_cell_voltage);
  fprintf(f, "Port voltage:     %.2f V\n", m->port_voltage);
  fprintf(f, "Battery capacity: %.2f AH\n", m->battery_capacity);
  fprintf(f, "Rated capacity:   %.2f AH\n", m->rated_capacity);
  fprintf(f, "State of health:  %.0f\%\n", m->state_of_health);
  fprintf(f, "Cycles:           %d\n", m->number_of_cycles);

  if ( longer ) {
    fprintf(f, "\nBattery Cell State:\n\n");
    cell_state_text(f, m, 0);
    fprintf(f, "\n");
    cell_state_text(f, m, 8);
    fprintf(f, "\n");
  
    fprintf(f, "Ambient temperature:           %.0f C, %.0f F\n", m->temperature[4], _sp_farenheit(m->temperature[4]));
    fprintf(f, "Power electronics temperature: %.0f C, %.0f F\n", m->temperature[5], _sp_farenheit(m->temperature[5]));
  }
}
