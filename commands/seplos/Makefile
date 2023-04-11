CFLAGS= -g -I../../library
OBJS= argp.o main.o

LIBS=../../library/libseplos.a

seplos:	$(OBJS) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)
