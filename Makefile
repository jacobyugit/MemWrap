# the compiler: gcc for C program
CC		= gcc
RM		= /bin/rm

# compiler flags:
CFLAGS	= -shared -fPIC

SRCS	= myMemWrap.c

# The traget shared library
TARGET	= libmemwrap.so

all			:	$(TARGET)

$(TARGET)	:	$(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) -ldl

# Clean up
clean:
	$(RM) $(TARGET)
