PROJECTNAME ?= cp210x_gpio_activation_libusb
CC = gcc
LD = ld
C_SRC = cp210x_gpio_activation_libusb.c
CFLAGS=-g -Wall -Wextra $(shell pkg-config --libs --cflags libusb-1.0)
#LDFLAGS=$(shell pkg-config --libs libusb-1.0)

cp210x_gpio_activation_libusb: $(C_SRC)
	$(CC) $(DEBUG) -o $@ $^ $(CFLAGS)

debug: DEBUG= -DDEBUG

debug: cp210x_gpio_activation_libusb

clean: 
	rm -f cp210x_gpio_activation_libusb.o
	rm -f cp210x_gpio_activation_libusb

