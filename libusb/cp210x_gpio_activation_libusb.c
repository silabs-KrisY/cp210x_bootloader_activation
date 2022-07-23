/************************************
cp210x_gpio_activation_libusb.c

This implementation uses libusb to directly send USB control requests to
CP210x devices to manipulate the GPIOS to drive active low reset and the active
low bootloader activation pins in order to activate bootloader mode on an
EFR32 target. The argument is the CP210x pin shift / position for each of the
GPIOs, as well as the interface for the CP2105.

***************************************/
/**************************************
SPDX-License-Identifier: Zlib

The licensor of this software is Silicon Laboratories Inc.

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*********************************************/

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>

#define REQ_DIR_OUT 0x00
#define REQ_DIR_IN 0x80
#define REQ_TYPE_STD 0x00
#define REQ_TYPE_CLS 0x20
#define REQ_TYPE_VND 0x40
#define REQ_RCPT_DEV 0x00
#define REQ_RCPT_IFC 0x01
#define REQ_RCPT_EPT 0x02
#define REQTYPE_HOST_TO_INTERFACE REQ_DIR_OUT | REQ_TYPE_VND | REQ_RCPT_IFC
#define CP210x_REQ_IFC_ENABLE 0x00
#define bReq_VENDOR_SPECIFIC 0xFF
#define wVal_WRITE_LATCH 0x37E1
#define wVAL_READ_LATCH 0x00C2

#define CP2102N_CP2103_CP2104_PID 0xea60
#define CP2105_PID 0xea70
#define CP2108_PID 0xea71
#define SILABS_VID 0x10c4
#define MAX_GPIO_PIN 15

#define RESET_PIN_MASK (1 << reset_pin_shift)
#define BTLACT_PIN_MASK (1 << btlact_pin_shift)

// Optstring argument for getopt.
#define OPTSTRING      "hbri"

#define RESET_DELAY_US 5000 //5ms
#define BTLACT_DELAY_US 30000 //30ms

static struct option long_options[] = {
     {"help",       no_argument,       0,  'h' },
     {"btlact",     required_argument, 0,  'b' },
     {"reset",      required_argument, 0,  'r' },
     {"interface",  required_argument, 0,  'i' },
     {0,           0,                 0,  0  }};

#define HELP_MESSAGE \
"./exe/cp210x_gpio_activation_libusb --reset <cp210x_gpionum> --btlact <cp210x_gpionum> --interface <cp2105_interfacenum>\n"\
"                                   \n"\
"--reset       This supplies the number of the CP210x GPIO connected to nRESET of the EFR32.\n"           \
"--btlact      This argument supplies the number of the CP210x GPIO connected to the active low bootloader\n"        \
"                activation pin of the EFR32. Note this argument is optional - without it, the\n"         \
"                application will assert reset on the target without activating the bootloader.\n"        \
"--interface   Specifies the interface number for the USB request. This is only valid for CP2105, for\n"  \
"                which the GPIOs are independent for each interface (ECI = interface 0, SCI = interface 1)\n" \
"--help        Print help message\n"

libusb_context* context = NULL;

int main(int argc, char *argv[])
{
    libusb_device **device_list;
    libusb_device *dev;
    struct libusb_device_descriptor desc;
    libusb_device_handle* dev_handle;
    int retval;
    uint8_t found_flag=0;
    int i=0;
    ssize_t dev_list_count;
    uint16_t wIndex;
    int opt = 0;
    uint8_t btlact_pin_shift=0;
    uint8_t reset_pin_shift=0;
    uint8_t interface_number=0;
    uint8_t btlact_flag = 0; //if btlact isn't specified, reset only

    // Use these for CP2105
    struct cp210x_gpio_write8 {
    	uint8_t	mask;
    	uint8_t	state;
    } gpiobuf8;

     // Use these for CP2108
    struct cp210x_gpio_write16 {
    	uint16_t	mask;
    	uint16_t state;
    } gpiobuf16;

    // Process command line options.
  while ((opt = getopt_long(argc, argv, OPTSTRING, long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
      //TODO
        printf(HELP_MESSAGE);
        exit(0);
      break;

      case 'b':
        //btlact pin shift
        btlact_pin_shift = atoi(optarg);
        if (btlact_pin_shift > MAX_GPIO_PIN) {
          printf("ERROR: btlact pin must be less than %d, selected %d\n",
                  MAX_GPIO_PIN, btlact_pin_shift);
          exit(1);
        }
        btlact_flag = 1;
        break;

      case 'r':
        //btlact pin shift
        reset_pin_shift = atoi(optarg);
        if (reset_pin_shift > MAX_GPIO_PIN) {
          printf("ERROR: reset pin must be less than %d, selected %d\n",
                  MAX_GPIO_PIN, reset_pin_shift);
          exit(1);
        }
        break;

      case 'i':
        //interface number (needed for CP2105, ignored for others)
        interface_number = atoi(optarg);
        break;

      default:
      //TODO
        printf("hit default case\n");
      break;
    }
  }

  if (btlact_flag == 0) {
    // This is just a reset, so we'll allow this case
    printf("Resetting target only (without bootloader activation)\n");
  } else {
    // if btlact was specified, make sure it's not the same pin as reset
    if (reset_pin_shift == btlact_pin_shift) {
      printf("ERROR: reset pin and btlact pin mask cannot be the same value!\n");
      exit(1);
    } else {
      printf("Resetting target with bootloader activation\n");
    }
  }

  retval = libusb_init(&context);
  if (retval< 0) {
    printf("ERROR: libusb init failure %s\n", libusb_error_name(retval));
    libusb_exit(NULL);
    exit(1);
  }

#ifdef DEBUG
    // Enable verbose debug logging
    libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, 4);
#endif
    dev_list_count = libusb_get_device_list(NULL, &device_list);
    if (dev_list_count < 0){
        printf("ERROR: cannot get device list: %s\n", libusb_error_name((int)dev_list_count));
        libusb_exit(NULL);
        exit(1);
    } else {
#ifdef DEBUG
	printf("Searching through the list of %d devices for CP210x...\n", dev_list_count);
#endif
    }
    // Search through the list of USB devices to find the first CP210x
    for (i=0;i<dev_list_count;i++) {
      dev = device_list[i];
      retval = libusb_get_device_descriptor(dev, &desc);
      assert(retval == LIBUSB_SUCCESS);
#ifdef DEBUG
      printf("idvendor=0x%2x, idProduct=0x%2x\n", desc.idVendor, desc.idProduct);
#endif
      if (desc.idVendor == SILABS_VID) {
        switch (desc.idProduct) {
          case CP2105_PID:
            printf("CP2105 detected, using interface %d.\n", interface_number);
            found_flag = 1;
            break;

          case CP2108_PID:
            printf("CP2108 detected.\n");
            found_flag = 1;
            break;

          case CP2102N_CP2103_CP2104_PID:
            printf("CP2102, CP2103, or CP2104 detected.\n");
            found_flag = 1;
            break;

          default:
            break;
        }
      }
      if (found_flag == 1) {
        //exit for loop
        break;
      }
    } //end for

    if (found_flag == 0) {
      // Didn't find any CP210x, exit
      printf("ERROR: No CP210x devices found. Exiting\n");
      libusb_exit(NULL);
      exit(1);
    }

    // Open the device and claim the interface
    retval = libusb_open(dev, &dev_handle);
    if (retval != LIBUSB_SUCCESS) {
      printf("ERROR: libusb open failed: %s\n", libusb_error_name(retval));
      libusb_exit(NULL);
      exit(1);
    }

    // CP210x will be using the kernel driver by default, so we want to detach
    // when accessing directly with libusb and re-attach afterwards
    retval = libusb_set_auto_detach_kernel_driver(dev_handle, 1);
    assert(retval == LIBUSB_SUCCESS);

    retval = libusb_claim_interface(dev_handle, interface_number);
    assert(retval== LIBUSB_SUCCESS);

    // Wiggle the CP210x GPIOs to activate the bootloader
    switch (desc.idProduct) {

      case CP2108_PID:
        interface_number = 0; //ignore interface for CP2108 (use zero)

        // Write the reset pin low, also write the btlact pin low if set
        gpiobuf16.mask = RESET_PIN_MASK | BTLACT_PIN_MASK;
        gpiobuf16.state = 0x0000;
        retval = libusb_control_transfer(dev_handle,
                                        REQTYPE_HOST_TO_INTERFACE,
                                        bReq_VENDOR_SPECIFIC,
                                        wVal_WRITE_LATCH,
                                        interface_number,
                                        (uint8_t *) &gpiobuf16,
                                        sizeof(gpiobuf16),
                                        0);
        assert(retval>=0);
        usleep(RESET_DELAY_US);

        // Write the reset high again, but don't touch the btlact pin
        gpiobuf16.mask = RESET_PIN_MASK;
        gpiobuf16.state = 0xffff; // Write all CP2108 GPIO high
        retval = libusb_control_transfer(dev_handle,
                                        REQTYPE_HOST_TO_INTERFACE,
                                        bReq_VENDOR_SPECIFIC,
                                        wVal_WRITE_LATCH,
                                        interface_number,
                                        (uint8_t *)&gpiobuf16,
                                        sizeof(gpiobuf16),
                                        0);
        assert(retval>=0);

        if (btlact_flag == 1) {
          usleep(BTLACT_DELAY_US);
          // Write btlact high if its being used
          gpiobuf16.mask = BTLACT_PIN_MASK;
          gpiobuf16.state = 0xffff; // Write all CP2108 GPIO high
          retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_INTERFACE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          interface_number,
                                          (uint8_t *)&gpiobuf16,
                                          sizeof(gpiobuf16),
                                          0);
          assert(retval>=0);
        }
        break;

      case CP2105_PID:
        // NOTE that the interface number is important for the CP2105

        // Write the reset pin low, also write the btlact pin low if set
        // Using typecast to insure value fits in the byte
        gpiobuf8.mask = (uint8_t) RESET_PIN_MASK | BTLACT_PIN_MASK;
        gpiobuf8.state = 0x00;

        retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_INTERFACE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          interface_number,
                                          (uint8_t *) &gpiobuf8,
                                          sizeof(gpiobuf8), 0);
        assert(retval>=0);
        usleep(RESET_DELAY_US);

        // Write the reset high again, but don't touch the btlact pin
        // Using typecast to insure value fits in the byte
        gpiobuf8.mask = (uint8_t) RESET_PIN_MASK;
        gpiobuf8.state = 0xff;
        retval = libusb_control_transfer(dev_handle,
                                        REQTYPE_HOST_TO_INTERFACE,
                                        bReq_VENDOR_SPECIFIC,
                                        wVal_WRITE_LATCH,
                                        interface_number,
                                        (uint8_t *)&gpiobuf8,
                                        sizeof(gpiobuf8),
                                        0);
        assert(retval>=0);

        if (btlact_flag == 1) {
          usleep(BTLACT_DELAY_US);
          // Write btlact high if its being used
          // Using typecast to insure value fits in the byte
          gpiobuf8.mask = (uint8_t) BTLACT_PIN_MASK;
          gpiobuf8.state = 0xff;
          retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_INTERFACE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          interface_number,
                                          (uint8_t *)&gpiobuf8,
                                          sizeof(gpiobuf8),
                                          0);
          assert(retval>=0);
        }
        break;

      case CP2102N_CP2103_CP2104_PID:
        // Write the reset pin low, also write the btlact pin low if set
        interface_number = 0; //ignore interface for CP2102N_CP2103_CP2104 (use zero)
        // Using typecast to insure mask value fits in the byte
        wIndex = (0x00 << 8) | (uint8_t) (BTLACT_PIN_MASK | RESET_PIN_MASK);
        retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_INTERFACE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          wIndex,
                                          NULL,
                                          0,
                                          0);
        assert(retval>=0);
        usleep(RESET_DELAY_US);

        // Write the reset high again, but don't touch the btlact pin
        // Using typecast to insure value fits in the byte
        wIndex = (0xff << 8) | (uint8_t) RESET_PIN_MASK;
        retval = libusb_control_transfer(dev_handle,
                                        REQTYPE_HOST_TO_INTERFACE,
                                        bReq_VENDOR_SPECIFIC,
                                        wVal_WRITE_LATCH,
                                        wIndex,
                                        NULL,
                                        0,
                                        0);
        assert(retval>=0);
        if (btlact_flag == 1) {
          usleep(BTLACT_DELAY_US);
          // Write btlact high if its being used
          // Using typecast to insure value fits in the byte
          wIndex = (0xff << 8) | (uint8_t) BTLACT_PIN_MASK;
          retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_INTERFACE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          wIndex,
                                          NULL,
                                          0,
                                          0);
          assert(retval>=0);
        }
        break;

      default:
        printf("ERROR: desc.idProduct=0x%2x not handled, exiting\n", desc.idProduct);
        exit(1);
        break;

    }

    printf("Success!\n");
    retval = libusb_release_interface(dev_handle, interface_number);
    assert(retval== LIBUSB_SUCCESS);
    libusb_close(dev_handle);
    libusb_free_device_list(device_list, 1);
    libusb_exit(NULL);
    return 0;
}
