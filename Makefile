TARGET = fusehfs
LIBS = -lm -L hfsutils-3.2.6/libhfs/ -lhfs -lfuse
CC = gcc
CFLAGS = -g -Wall -D_FILE_OFFSET_BITS=64 -I hfsutils-3.2.6
LIBHFS = hfsutils-3.2.6/libhfs/libhfs.a

.PHONY: default all clean

default: $(LIBHFS) $(TARGET) 
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

$(LIBHFS) ::
	cd hfsutils-3.2.6 && $(MAKE)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
