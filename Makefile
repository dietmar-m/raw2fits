
CFLAGS = -Wall -O2
OBJS = main.o raw2fits.o
LIBS = -lraw -lcfitsio
TARGET =raw2fits
BINDIR = $(HOME)/bin

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o raw2fits $(OBJS) $(LIBS)

clean:
	$(RM) $(TARGET) $(OBJS)

install: $(TARGET)
	install -m 755 $(TARGET) $(BINDIR)
