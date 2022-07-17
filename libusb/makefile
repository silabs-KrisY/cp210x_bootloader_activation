TARGET ?= cp210x_gpio_activation_libusb
CC = gcc
LD = ld
C_SRC = cp210x_gpio_activation_libusb.c
CFLAGS=-g -Wall -Wextra $(shell pkg-config --libs --cflags libusb-1.0)
EXEDIR = exe

$(EXEDIR)/$(TARGET): $(C_SRC)
	mkdir -p $(EXEDIR)
	$(CC) -o $@ $^ $(CFLAGS)

debug: DEBUG= -DDEBUG

debug: $(EXEDIR)/$(TARGET)

clean:
	rm -f $(EXEDIR)/$(TARGET)
