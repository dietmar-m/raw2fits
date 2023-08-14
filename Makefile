
CFLAGS = -Wall -O2
OBJS = main.o raw2fits.o
LIBS = -lraw -lcfitsio

raw2fits: $(OBJS)
	$(CC) $(LDFLAGS) -o raw2fits $(OBJS) $(LIBS)

clean:
	$(RM) raw2fits $(OBJS)
