'''
/***************************************************************************//**
* @file cp210x_xmodem_activation.py
* @brief This python script is intended to upgrade the firmware on node connected
        to a CP2105 USB-to-serial device using the CP2105 GPIOs to force the node
        to activate the Gecko Bootloader.
*******************************************************************************
* # License
* <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* SPDX-License-Identifier: Zlib
*
* The licensor of this software is Silicon Laboratories Inc.
*
* This software is provided \'as-is\', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would be
*    appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*
*******************************************************************************
* # Experimental Quality
* This code has not been formally tested and is provided as-is. It is not
* suitable for production environments. In addition, this code will not be
* maintained and there may be no bug maintenance planned for these resources.
* Silicon Labs may update projects from time to time.
******************************************************************************/

'''

import time
import usb.core
import usb.util
import serial
from xmodem import XMODEM
import sys
import argparse
import os
import serial.tools.list_ports as prtlst

BOOTLOADER_INIT_TIMEOUT = 10

def is_valid_file(parser, arg):
    if not os.path.isfile(arg):
        parser.error("The file %s does not exist!" % arg)
    else:
        return arg

def activate_target_bootloader_cp210x(bInterface):
    '''
    This implementation should work on CP2105, CP2102N, CP2103, CP2104, and CP2108.
    Assume GPIO0 is nRESET and GPIO1 is the active low bootloader activation
        pin on the target. We just need to assert nRESET, assert bootloader activation,
        then de-assert nRESET and then de-assert bootloader activation. The target
        should then be in the right state to receive a new image.

        bInterface parameter is only used for the CP2105
    '''
    REQ_DIR_OUT = 0x00
    REQ_DIR_IN = 0x80
    REQ_TYPE_STD = 0x00
    REQ_TYPE_CLS = 0x20
    REQ_TYPE_VND = 0x40
    REQ_RCPT_DEV = 0x00
    REQ_RCPT_IFC = 0x01
    REQ_RCPT_EPT = 0x02
    CP210x_REQ_IFC_ENABLE = 0x00
    bReq_VENDOR_SPECIFIC = 0xFF
    wVal_WRITE_LATCH = 0x37E1

    GPIO0_MASK = 0x01
    GPIO1_MASK = 0x02
    nRESET_MASK = GPIO0_MASK
    nBOOT_MASK = GPIO1_MASK

    CP2102N_CP2103_CP2104_PID = 0xea60
    CP2105_PID = 0xea70
    CP2108_PID = 0xea71

    PIDs = [
            CP2102N_CP2103_CP2104_PID,
            CP2105_PID,
            CP2108_PID
            ]
    PIDs_STRINGS = [
            "CP2102N_CP2103_CP2104",
            "CP2105",
            "CP2108"
    ]

    VID = 0x10c4 # SiliconLabs

    found_flag = 0
    cp2105_flag = 0
    # Find the first device with the Silabs VID
    for dev in usb.core.find(find_all=True, idVendor=VID):
        if dev.idProduct in PIDs:
            print("Found device! PID={} ({})".format(hex(dev.idProduct),
                PIDs_STRINGS[PIDs.index(dev.idProduct)]))
            found_flag = 1
            if dev.idProduct == CP2105_PID:
                cp2105_flag = True
                if bInterface == None:
                    print("Error: Need to specify interface for CP2105!")
                    return 1
            break

    if found_flag == 0:
        print("Error: Did not find a device!")
        return 1

    reqType = REQ_DIR_OUT | REQ_TYPE_VND | REQ_RCPT_IFC

    wLength = 1

    # Go ahead and drive both GPIOs on the selected interface low
    # First (low) byte is mask, second (high) byte is desired latch state
    latchVal = [(nRESET_MASK | nBOOT_MASK), ~(nRESET_MASK | nBOOT_MASK) & 0xff]
    #print("Resetting! latchVal={}, {}".format(hex(latchVal[0]), hex(latchVal[1])))
    if cp2105_flag == True:
        wIndex = bInterface
        data = latchVal
    else:
        wIndex = latchVal[0] | (latchVal[1] << 8) # Send as int
        data = None
    dev.ctrl_transfer(reqType, bReq_VENDOR_SPECIFIC, wVal_WRITE_LATCH, wIndex, data)
    time.sleep(0.03) # wait for 30 ms

    # Drive nRESET high but leave gpio activation low
    latchVal = [nRESET_MASK , nRESET_MASK]
    #print("Coming out of reset. latchVal={}, {}".format(hex(latchVal[0]), hex(latchVal[1])))
    if cp2105_flag == True:
        wIndex = bInterface
        data = latchVal
    else:
        wIndex = latchVal[0] | (latchVal[1] << 8) # Send as int
        data = None
    dev.ctrl_transfer(reqType, bReq_VENDOR_SPECIFIC, wVal_WRITE_LATCH, wIndex, data)
    time.sleep(0.1) # wait for 100 ms

    # Now return gpio activation high
    latchVal = [nBOOT_MASK ,(nBOOT_MASK)]
    #print("Releasing nBOOT, latchVal={}, {}".format(hex(latchVal[0]), hex(latchVal[1])))
    if cp2105_flag == True:
        wIndex = bInterface
        data = latchVal
    else:
        wIndex = latchVal[0] | (latchVal[1] << 8) # Send as int
        data = None
    dev.ctrl_transfer(reqType, bReq_VENDOR_SPECIFIC, wVal_WRITE_LATCH, wIndex, data)

    return 0


def flash(port, interface, file):
    # STATIC FUNCTIONS
    def getc(size, timeout=1):
        return ser.read(size)

    def putc(data, timeout=1):
        ser.write(data)
        time.sleep(0.001)

    print('Restarting NCP into Bootloader mode...')

    # Reboot target, asserting bootloader activation
    if activate_target_bootloader_cp210x(interface) == 1:
        # Error occurred
        exit(1)

    # Default port settings
    BAUD = 115200;
    XON_XOFF = False;
    RTS_CTS = False; #HW flow control not really needed with xmodem

    # Init serial port
    ser = serial.Serial(
        port=port,
        baudrate=BAUD,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        xonxoff=XON_XOFF,
        rtscts=RTS_CTS
    )

    # Kick the prompt
    ser.write(b'\n')
    # Gecko  BL verion at 2nd line
    ser.readline() # read blank line
    verBL = ser.readline() # read Gecko BTL version
    print('BL version:{}'.format(verBL.decode())) # show Bootloader version
    ser.readline() # 1. upload gbl
    ser.readline() # 2. run
    ser.readline() # 3. gbl info
    # Enter '1' to initialize X-MODEM mode
    ser.write(b'1')
    time.sleep(1)
    # Read responses
    ser.readline() # BL > 1
    ser.readline() # begin upload
    # Wait for char 'C'
    success = False
    start_time = time.time()
    while time.time()-start_time < BOOTLOADER_INIT_TIMEOUT:
        if ser.read() == b'C':
            success = True
            break
    if not success:
        print('Failed to restart into bootloader mode. Please see users guide.')
        sys.exit(1)

    print('Successfully restarted into bootloader mode! Starting upload of NCP image... ')

    # Start XMODEM transaction
    modem = XMODEM(getc, putc)
    stream = open(file,'rb')
    sentcheck = modem.send(stream,retry=8)

    if sentcheck:
        print('Finished!')
    else:
        print('NCP upload failed. Please reload a correct NCP image to recover.')
    print('Rebooting NCP...')

    # Send Reboot into App-Code command
    ser.write(b'2')
    ser.close()

def scan():
    '''
    Scan for Silabs comports
    '''

    for c in prtlst.comports():
        if c.vid == 0x10c4:
            print("{}".format(c.device))
            print("   Desc: {}".format(c.description))
            print("   HWID: {}".format(c.hwid))

parser = argparse.ArgumentParser(description='')
subparsers = parser.add_subparsers(help='flash, scan')
parser_flash = subparsers.add_parser('flash', help='Flashes a NCP with a new application packaged in an GBL file.')
parser_flash.add_argument('-p','--port', type=str, required=True,
                    help='Serial port for device')
parser_flash.add_argument('-i','--interface', type=int, required=False,
                    help='USB interface for CP2105 (0 = ECI, 1 = SCI)')
parser_flash.add_argument('-f', '--file', type=lambda x: is_valid_file(parser, x), required=True,
                    help='GBL file to upload to NCP')
parser_flash.set_defaults(which='flash')

parser_scan = subparsers.add_parser('scan', help='Scan OS for attached CP2105 devices')
parser_scan.set_defaults(which='scan')

args = parser.parse_args()
if args.which == 'scan':
    scan()
else:
    flash(args.port,args.interface,args.file)
