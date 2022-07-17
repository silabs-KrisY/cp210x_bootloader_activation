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
#define REQTYPE_HOST_TO_DEVICE REQ_DIR_OUT | REQ_TYPE_VND | REQ_RCPT_DEV
#define CP210x_REQ_IFC_ENABLE 0x00
#define bReq_VENDOR_SPECIFIC 0xFF
#define wVal_WRITE_LATCH 0x37E1
#define wVAL_READ_LATCH 0x00C2

#define GPIO0_MASK 0x01
#define GPIO1_MASK 0x02
#define nRESET_MASK GPIO0_MASK
#define nBOOT_MASK GPIO1_MASK

#define CP2102N_CP2103_CP2104_PID 0xea60
#define CP2105_PID 0xea70
#define CP2108_PID 0xea71
#define SILABS_VID 0x10c4

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

libusb_context* context = NULL;

int main(int argc, char *argv[])
{
    libusb_device **device_list;
    libusb_device *dev;
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *cfg_desc = NULL;
    libusb_device_handle* dev_handle;
    int retval;
    uint8_t found_flag=0;
    int i=0;
    ssize_t dev_list_count;
    uint16_t wIndex;
    int opt = 0;
    uint16_t btlact_pin_mask=0; //active low btl act pin mask
    uint16_t reset_pin_mask=0; //active low reset pin mask
    uint8_t interface_number=0;

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
        printf("add help message here!\n");
      break;

      case 'b':
        //btlact pin mask
        btlact_pin_mask = atoi(optarg);
        break;

      case 'r':
        //btlact pin mask
        reset_pin_mask = atoi(optarg);
        break;

      case 'i':
        //interface number (needed for CP2105, ignored for others)
        interface_number = atoi(optarg);
        break;

      default:
        printf("hit default case\n");
      break;
    }
  }

  // Validate inputs
  if (reset_pin_mask == 0) {
    printf("Error - reset pin mask must be non-zero!\n");
    exit(1);
  } else if ((reset_pin_mask & (reset_pin_mask - 1)) != 0) {
    // check to make sure only one bit is set
    printf("ERROR: reset pin mask must only have one bit set, value = 0x%2x\n",
          reset_pin_mask);
    exit(1);
  }

  // Validate inputs
  if (reset_pin_mask == 0) {
    // This is just a reset, so we'll allow this case
    printf("Toggling reset without btl activation\n");
  } else if ((btlact_pin_mask & (btlact_pin_mask - 1)) != 0) {
    // check to make sure only one bit is set
    printf("ERROR: btlact pin mask must only have one bit set, value = 0x%2x\n",
          btlact_pin_mask);
    exit(1);
  }

  if (reset_pin_mask == btlact_pin_mask) {
    printf("ERROR: reset pin mask and btlact pin mask cannot be the same value!\n");
    exit(1);
  }
  retval = libusb_init(&context);
  if (retval< 0) {
    printf("libusb_init failure %s\n", libusb_error_name(retval));
    libusb_exit(NULL);
    exit(1);
  }

#ifdef DEBUG
    // Enable verbose debug logging
    libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, 4);
#endif
    dev_list_count = libusb_get_device_list(NULL, &device_list);
    if (dev_list_count < 0){
        printf("ERROR: libusb_init failure %s\n", libusb_error_name((int)dev_list_count));
        libusb_exit(NULL);
        exit(1);
    }
    // Search through the list of USB devices to find the first CP210x
    while (((dev = device_list[i++]) != NULL) && found_flag == 0) {
        dev_handle = NULL;
        retval = libusb_get_device_descriptor(dev, &desc);
        if (retval< 0) {
            printf("failed to get device descriptor\n");
            continue;
        } else {
#ifdef DEBUG
          printf("idvendor=0x%2x, idProduct=0x%2x\r\n", desc.idVendor, desc.idProduct);
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
        } //end if
      } //end while

    if (found_flag == 0) {
      // Didn't find any CP210x, exit
      printf("No CP210x devices found. Exiting\n");
      libusb_exit(NULL);
      exit(1);
    }

    // Open the device and claim the interface
    retval = libusb_open(dev, &dev_handle);
    if (retval== LIBUSB_SUCCESS) {
      printf("libusb_open success!\n");
    } else {
      printf("ERROR: llibusb_open fail, %s!\n", libusb_error_name(retval));
      exit(1);
    }

    // CP210x will be using the kernel driver by default, so we want to detach
    // when accessing directly with libusb and re-attach afterwards
    retval = libusb_set_auto_detach_kernel_driver(dev_handle, 1);
    assert(retval == LIBUSB_SUCCESS);

    retval = libusb_get_active_config_descriptor(dev, &cfg_desc);
    assert(retval == LIBUSB_SUCCESS);

    retval = libusb_claim_interface(dev_handle, interface_number);
    assert(retval== LIBUSB_SUCCESS);

    // Now we will do the GPIO manipulation. Just write low for 5 sec than high for now
    // for proof of concept purposes
    switch (desc.idProduct) {

      case CP2108_PID:
        // Write the reset pin low, also write the btlact pin low if set
        gpiobuf16.mask = reset_pin_mask | btlact_pin_mask;
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
        gpiobuf16.mask = reset_pin_mask;
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

        if (btlact_pin_mask != 0) {
          usleep(BTLACT_DELAY_US);
          // Write btlact high if its being used
          gpiobuf16.mask = btlact_pin_mask;
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
        // do a bit of bound checking here
        if (reset_pin_mask > 0xff) {
          printf("ERROR: reset pin mask 0x%2x out of range of 8-bit value for CP2105\n",
                  reset_pin_mask);
          exit(1);
        }
        if (btlact_pin_mask > 0xff) {
          printf("ERROR: btlact pin mask 0x%2x out of range of 8-bit value for CP2105\n",
                  btlact_pin_mask);
          exit(1);
        }
        // Write the reset pin low, also write the btlact pin low if set
        gpiobuf8.mask = reset_pin_mask | btlact_pin_mask;
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
        gpiobuf8.mask = reset_pin_mask;
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

        if (btlact_pin_mask != 0) {
          usleep(BTLACT_DELAY_US);
          // Write btlact high if its being used
          gpiobuf8.mask = btlact_pin_mask;
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
        wIndex = (0x00 << 8) | btlact_pin_mask | reset_pin_mask;
        retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_DEVICE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          wIndex,
                                          NULL,
                                          0,
                                          0);
        assert(retval>=0);
        usleep(RESET_DELAY_US);

        // Write the reset high again, but don't touch the btlact pin
        wIndex = (0xff << 8) | reset_pin_mask;
        retval = libusb_control_transfer(dev_handle,
                                        REQTYPE_HOST_TO_DEVICE,
                                        bReq_VENDOR_SPECIFIC,
                                        wVal_WRITE_LATCH,
                                        wIndex,
                                        NULL,
                                        0,
                                        0);
        assert(retval>=0);
        if (btlact_pin_mask != 0) {
          usleep(BTLACT_DELAY_US);
          // Write btlact high if its being used
          wIndex = (0xff << 8) | btlact_pin_mask;
          retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_DEVICE,
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
        printf("Error! desc.idProduct=0x%2x not handled, exiting\n", desc.idProduct);
        exit(1);
        break;

    }

    retval = libusb_release_interface(dev_handle, interface_number);
    assert(retval== LIBUSB_SUCCESS);
    libusb_close(dev_handle);
    libusb_free_device_list(device_list, 1);
    libusb_exit(NULL);
    return 0;
}
