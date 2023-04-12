CFLAGS= -g
OBJECTS= bms.o data.o data_conversion.o error.o html.o json.o names.o posix.o posix_open.o \
 posix_read.o \
 protocol_version.o text.o

libseplos.a: $(OBJECTS)
	- rm -f $@
	ar crs $@ $(OBJECTS)
