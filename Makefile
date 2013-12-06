# compiler:
CC = gcc
## args: especially libs
CFLAGS = -O3 -Wunused
LIBS = -lwiringPi
OUTPUT = NFCLight

all: NFCLight

NFCLight:  main.o P9813GPIO.o PN532SPI.o
	 	$(CC) $(CFLAGS) $(LIBS) -o $@ $^


main.o: main.c PN532SPI.h


PN532SPI.o: PN532SPI.c PN532SPI.h


P9813GPIO.0: P9813GPIO.c P9813GPIO.h


clean:
	rm -rf *.o NFCLight
	