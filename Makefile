CC = gcc
RM = rm -f
CFLAGS = -Wall -O2
LIBS = -ludev
LDFLAGS = 

all: ptouch-770-write

clean:
	$(RM) ptouch-770-write *.o

ptouch-770-write: ptouch-770-write.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $^

#gcc -Wall -o ptouch-770-write -O2 ptouch-770-write.c -ludev
