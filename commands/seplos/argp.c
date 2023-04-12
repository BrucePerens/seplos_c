#include "./seplos_cmd.h"
#include "internal.h"
#include <string.h>

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
  {"format", 'f', "text|HTML|JSON", 0, "Format of the output: text: text file, HTML: web page, JSON: easy format for communication between programs."},
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
  case 'f':
    if ( ( strcmp(arg, "text") == 0 ) || ( strcmp(arg, "TEXT") == 0 ) )
      arguments->format = TEXT;
    else if ( ( strcmp(arg, "html") == 0 ) || ( strcmp(arg, "HTML") == 0 ) )
      arguments->format = HTML;
    else if ( ( strcmp(arg, "json") == 0 ) || ( strcmp(arg, "JSON") == 0 ) )
      arguments->format = JSON;
    else
      argp_failure(state, 1, 0, "Parameter to --format= or -f must be \"text\", \"HTML\", or \"JSON\"");
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
