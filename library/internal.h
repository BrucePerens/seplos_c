#include "./seplos.h"

typedef struct _Seplos_2_O_Binary {
  uint8_t	version;
  uint8_t	address;
  uint8_t	device;
  uint8_t	function;
  uint16_t	length;
} Seplos_2_0_Binary;

extern void		_sp_discard_serial_input(seplos_device fd);
extern void		_sp_error(const char * restrict pattern, ...);
extern float		_sp_farenheit(float c);
extern void		_sp_hex1(uint8_t value, char ascii[1]);
extern void		_sp_hex2(uint8_t value, char ascii[2]);
extern void		_sp_hex4(uint16_t value, char ascii[4]);
extern uint8_t		_sp_hex1b(uint8_t c, bool * invalid);
extern uint8_t		_sp_hex2b(const char ascii[2], bool * invalid);
extern uint16_t		_sp_hex4b(const char ascii[4], bool * invalid);
extern unsigned int	_sp_length_checksum(unsigned int length);
extern unsigned int	_sp_overall_checksum(const char * restrict data, unsigned int length);
extern int		_sp_read_serial(seplos_device fd, void * data, size_t size);
extern void		_sp_wait_until_serial_data_is_transmitted(seplos_device fd);
extern int		_sp_write_serial(seplos_device fd, void * data, size_t size);
