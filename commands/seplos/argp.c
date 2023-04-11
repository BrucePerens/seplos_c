#include "./seplos_cmd.h"

static error_t parse_opt(int key, char *arg, struct argp_state *state);

const char * argp_program_version = "seplos 0.1";
const char * argp_program_bug_address = "Bruce Perens K6BP <bruce@perens.com>";

static const char args_doc[] = "";
static const char doc[] = \
  "Monitor the battery-management system." \
  "";

static const struct argp_option options[] = {
  {"device", 'd', "/dev/tty...", 0, "The serial device used to communicate with the battery."},
  {"longer", 'l', 0, 0, "More information: individual cell states, etc."},
  {}
};

const struct argp argp = {
  options, parse_opt, args_doc, doc
};

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
  struct arguments * arguments = state->input;

  switch ( key ) {
  case 'd':
    arguments->device = arg;
    break;
  case 'l':
    arguments->longer = true;
    break;
  case ARGP_KEY_ARG:
  case ARGP_KEY_END:
  case ARGP_KEY_FINI:
  case ARGP_KEY_INIT:
  case ARGP_KEY_NO_ARGS:
  case ARGP_KEY_SUCCESS:
    break;
  case ARGP_KEY_ERROR:
    _sp_error("parse_opt() got ARGP_KEY_ERROR for argument %s\n", arg);
    break;
  default:
    _sp_error("parse_opt(key=%x) was not understood.\n", key);
    break;
  }
  return 0;
}
