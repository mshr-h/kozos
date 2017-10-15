PREFIX  = $(HOME)/local
ARCH    = h8300-elf
BINDIR  = $(PREFIX)/bin
ADDNAME = $(ARCH)-

AR      = $(BINDIR)/$(ADDNAME)ar
AS      = $(BINDIR)/$(ADDNAME)as
CC      = $(BINDIR)/$(ADDNAME)gcc
LD      = $(BINDIR)/$(ADDNAME)ld
NM      = $(BINDIR)/$(ADDNAME)nm
OBJCOPY = $(BINDIR)/$(ADDNAME)objcopy
OBJDUMP = $(BINDIR)/$(ADDNAME)objdump
RANLIB  = $(BINDIR)/$(ADDNAME)ranlib
STRIP   = $(BINDIR)/$(ADDNAME)strip

OBJS  = vector.o startup.o main.o
OBJS += lib.o serial.o xmodem.o

TARGET = kzload

CFLAGS = -Wall -mh -nostdinc -nostdlib -fno-builtin
CFLAGS += -I.
# CFLAGS += -g
CFLAGS += -Os
CFLAGS += -DKZLOAD

LFLAGS = -static -T ld.scr -L.

.SUFFIXES: .c .o
.SUFFIXES: .s .o

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(CFLAGS) $(LFLAGS)
	cp $(TARGET) $(TARGET).elf
	$(STRIP) $(TARGET)

.c.o : $<
	$(CC) -c $(CFLAGS) $<

.s.o : $<
	$(CC) -c $(CFLAGS) $<

$(TARGET).mot : $(TARGET)
	$(OBJCOPY) -O srec $(TARGET) $(TARGET).mot

image : $(TARGET).mot

write: $(TARGET).mot
	h8write -3069 -f20 $(TARGET).mot com1

clean :
	rm -f $(OBJS) $(TARGET) $(TARGET).elf $(TARGET).mot
