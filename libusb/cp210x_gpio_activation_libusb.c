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

libusb_context* context = NULL;

int main(void)
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
        printf("libusb_init failure %s\n", libusb_error_name((int)dev_list_count));
        libusb_exit(NULL);
        exit(1);
    }
    // Search through the list of USB devices to find the first CP210x
    while ((dev = device_list[i++]) != NULL) {
        dev_handle = NULL;
        retval = libusb_get_device_descriptor(dev, &desc);
        if (retval< 0) {
            fprintf(stderr, "failed to get device descriptor");
            continue;
        } else {
          printf("idvendor=0x%2x, idProduct=0x%2x\r\n", desc.idVendor, desc.idProduct);
          if ((desc.idVendor == SILABS_VID) && ((desc.idProduct == CP2102N_CP2103_CP2104_PID) || \
           (desc.idProduct == CP2105_PID) || (desc.idProduct == CP2108_PID))) {
             printf("CP210x detected!\r\n");
             found_flag = 1;
             break;
          }
        }
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
      printf("llibusb_open fail, %s!\n", libusb_error_name(retval));
      exit(1);
    }

    // CP210x will be using the kernel driver by default, so we want to detach
    // when accessing directly with libusb and re-attach afterwards
    retval = libusb_set_auto_detach_kernel_driver(dev_handle, 1);
    assert(retval == LIBUSB_SUCCESS);

    retval = libusb_get_active_config_descriptor(dev, &cfg_desc);
    assert(retval == LIBUSB_SUCCESS);

    // Choose the first interface
    uint8_t interface_number =
        cfg_desc->interface[0].altsetting[0].bInterfaceNumber;

    retval = libusb_claim_interface(dev_handle, interface_number);
    assert(retval== LIBUSB_SUCCESS);

    // Now we will do the GPIO manipulation. Just write low for 5 sec than high for now
    // for proof of concept purposes
    switch (desc.idProduct) {

      case CP2108_PID:
        // write all CP2108 GPIO low
        gpiobuf16.mask = 0xffff;
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
        sleep(5);

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
        break;

      case CP2105_PID:
        // write all CP2105 GPIO low (but only on the specified interface)
        gpiobuf8.mask = 0xff;
        gpiobuf8.state = 0x00;

        retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_INTERFACE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          interface_number,
                                          (uint8_t *) &gpiobuf8,
                                          sizeof(gpiobuf8), 0);
        assert(retval>=0);
        sleep(5);

        // Write all CP2105 GPIO high (but only on the specified interface)
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
        break;

      case CP2102N_CP2103_CP2104_PID:
        // write all CP210x GPIO low
        wIndex = 0x00FF;
        retval = libusb_control_transfer(dev_handle,
                                          REQTYPE_HOST_TO_DEVICE,
                                          bReq_VENDOR_SPECIFIC,
                                          wVal_WRITE_LATCH,
                                          wIndex,
                                          NULL,
                                          0,
                                          0);
        assert(retval>=0);
        sleep(5);

        wIndex = 0xffff; // Write all CP210x GPIO high
        retval = libusb_control_transfer(dev_handle,
                                        REQTYPE_HOST_TO_DEVICE,
                                        bReq_VENDOR_SPECIFIC,
                                        wVal_WRITE_LATCH,
                                        wIndex,
                                        NULL,
                                        0,
                                        0);
        assert(retval>=0);
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
