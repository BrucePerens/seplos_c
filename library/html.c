#include "./internal.h"

static void
cell_state_html(FILE * f, const SeplosData const * m, int offset, int length)
{
  fprintf(f, "<tr><th style=\"text-align: right;\">Cell</th>");
  for ( int i = 0; i < length; i++ ) {
    fprintf(f, "<th>%d</th>", i + offset);
  }
  fprintf(f, "</tr>\n<tr><th style=\"text-align: right;\">Voltage</th>");
  for ( int i = 0; i < length; i++ ) {
    const unsigned int index = i + offset;
    fprintf(f, "<td>%.3f</td>", m->cell_voltage[index]);
  }
  fprintf(f, "</tr>\n<tr><th style=\"text-align: right;\">Equilibrium</th>");
  for ( int i = 0; i < length; i++ ) {
    const unsigned int index = i + offset;
    fprintf(f, "<td style=\"text-align: center;\">%s</td>", (m->equilibrium_state & (1 << index)) ? "&#x2713;" : "&#x00b7;");
  }
  fprintf(f, "</tr>\n<tr><th style=\"text-align: right;\">Disconnected</th>");
  for ( int i = 0; i < length; i++ ) {
    const unsigned int index = i + offset;
    fprintf(f, "<td style=\"text-align: center;\">%s</td>", (m->equilibrium_state & (1 << index)) ? "&#2713;" : "&#x00b7;");
  }
  fprintf(f, "</tr>\n<tr><th style=\"text-align: right;\">Temperature</th>");
  for ( int i = 0; i < length / 4; i++ ) {
    const unsigned int index = i + (offset / 4);
    fprintf(f, "<td colspan=\"4\" style=\"text-align: center;\">%.0f C, %.0f F</td>", m->temperature[index], _sp_farenheit(m->temperature[index]));
  }
  fprintf(f, "</tr>\n");
}

	void
seplos_html(FILE * f, const SeplosData const * m, bool longer)
{
  fprintf(f, "<h2>Controller %x, battery pack %x:</h2>\n", m->controller_address, m->battery_pack_number);
  fprintf(f, "<p>\n");
  if ( m->has_alarm ) {
    fprintf(f, "<p>\n");
    fprintf(f, "<strong>&#x26a0;&nbsp;The battery indicates an alarm state. &#x26a0;</strong><br/>\n");
    fprintf(f, "Resolve this issue ASAP, or the battery may be damaged.<br/>\n");
    if ( m->depleted )
      fprintf(f, "<strong>The battery is depleted of charge.</strong><br/>\n");
    if ( m->overcharge )
      fprintf(f, "<strong>The battery is overcharged.</strong><br/>\n");
    if ( m->hot )
      fprintf(f, "<strong>The battery is too hot.</strong><br/>\n");
    if ( m->cold )
      fprintf(f, "<strong>the battery is too cold.</strong><br/>\n");
    if ( m->other_or_undocumented_alarm_state )
      fprintf(f, "<strong>The battery indicates an &#x201c;other&#x201d; or undocumented alarm state.</strong><br/>\n");

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
          s = "controller reports &#x201c;other&#x201d; voltage alarm state.\n";
          break;
        }

        fprintf(f, "<strong>Total battery voltage: %s</strong><br/>\n", s);
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

        fprintf(f, "<strong>%s</strong><br/>\n", s);
      }
    }

    if ( m->has_cell_alarm ) {
      fprintf(f, "<strong>The battery indicates an issue with one or more of the cells:</strong><br/>\n");
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
           
          fprintf(f, "<strong>Cell %d: %s</strong><br/>\n", i, s);
        }
      }
      fprintf(f, "\n");
    }

    if ( m->temperature_alarm ) {
      fprintf(f, "<strong>The battery temperature is out of bounds:</strong><br/>\n");

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
            s = "controller reports &#201c;other&#201d; temperature state.\n";
          }
          fprintf(f, "<strong>Cell %d: %s</strong><br/>\n", s);
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
              fprintf(f, "<strong>Alarm: %s.</strong><br/>\n", seplos_bit_alarm_names[(i * 32) + j]);
            }
          }
        }
      }
    }
  }
  else {
    fprintf(f, "&#x263a;&nbsp;No Alarms.\n");
  }
  fprintf(f, "</p>\n");

  fprintf(f, "<table>\n");
  fprintf(f, "<tr><th style=\"text-align: right;\">Voltage</th><td>%.2f V</td></tr>\n", m->total_battery_voltage);
  fprintf(f, "<tr><th style=\"text-align: right;\">Current</th><td>%.2f A</td></tr>\n", m->charge_discharge_current);
  fprintf(f, "<tr><th style=\"text-align: right;\">State of Charge</th><td>%.0f%</td></tr>\n", m->state_of_charge);
  fprintf(f, "<tr><th style=\"text-align: right;\">Temperatures</th><td>%.0f - %.0f C, %.0f - %.0f F (internal heating: %s)</td></tr>\n", m->lowest_temperature, m->highest_temperature, _sp_farenheit(m->lowest_temperature), _sp_farenheit(m->highest_temperature), m->heating_switch ? "ON" : "off");
  fprintf(f, "<tr><th style=\"text-align: right;\">Cell Voltages</th><td>%.3f - %.3f V (unbalance %.03f V)</td></tr>\n", m->lowest_cell_voltage, m->highest_cell_voltage, m->highest_cell_voltage - m->lowest_cell_voltage);
  fprintf(f, "<tr><th style=\"text-align: right;\">Port Voltage</th><td>%.3f V</td></tr>\n", m->port_voltage);
  fprintf(f, "<tr><th style=\"text-align: right;\">Battery Capacity</th><td>%.2f AH</td></tr>\n", m->battery_capacity);
  fprintf(f, "<tr><th style=\"text-align: right;\">Rated Capacity</th><td>%.2f AH</td></tr>\n", m->rated_capacity);
  fprintf(f, "<tr><th style=\"text-align: right;\">State of Health</th><td>%.0f%</td></tr>\n", m->state_of_health);
  fprintf(f, "<tr><th style=\"text-align: right;\">Lifetime Cycles</th><td>%.0f</td></tr>\n", m->number_of_cycles);
  fprintf(f, "</table>\n");

  if ( longer ) {
    fprintf(f, "\n<h3>Battery Cell State</h3>\n");
    fprintf(f, "<table>\n");
    cell_state_html(f, m, 0, 16);
    fprintf(f, "</table><br/><br/>\n");
  
    fprintf(f, "<table>\n");
    fprintf(f, "<tr><th style=\"text-align: right;\">Ambient Temperature</th><td>%.0f C, %.0f F</td></tr>\n", m->temperature[4], _sp_farenheit(m->temperature[4]));
    fprintf(f, "<tr><th style=\"text-align: right;\">Power Electronics Temperature</th><td>%.0f C, %.0f F</td></tr>\n", m->temperature[5], _sp_farenheit(m->temperature[5]));
    fprintf(f, "</table>\n");
  }
}
