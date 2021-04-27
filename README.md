# CP210x Gecko Bootloader Activation

This repo provides information on using the GPIO of a CP210x usb-to-serial converter to activate the gecko bootloader on a Silicon Labs EFR32. Bootloader activation is useful as a way to always be able to upgrade the firmware on a device, regardless of the state of the application firmware. This avoids the possibility of "bricking" the device. This is normally done via direct GPIO access from a host processor to the device, but when the device is being accessed serially behind a CP210x usb-to-serial converter, the direct GPIO access to the host is usually not present. But the CP210x have GPIOs that are accessible via USB, and these can be used to drive the pins on the target device to activate the bootloader.

## SW Prerequisites

1. Python 3.x.
2. xmodem python package
3. pyserial python package
4. pyusb python package
5. A GBL upgrade file for your target device

## HW Prerequisites
1. CP2105
2. EFR32/EFM32 with xmodem gecko bootloader flashed
3. Connect GPIO.0 of one of the CP2105 ports to nRESET. Note that it's difficult to connect to the target nRESET pin when using a radio board connected to the WSTK. For this reason, I recommend using the BRD8016A that comes with the [EXP4320A WGM110 Wi-Fi Expansion Kit](https://www.silabs.com/documents/public/user-guides/ug291-exp4320a-user-guide.pdf). You can install a radio board on this board and access pins via the 40 pin header (not installed by default). See below for an example pinout using BRD4158A as the target.
4. Connect GPIO.1 to a pin configured in the gecko bootloader as an active low bootloader activation pin (which I am calling 'nBOOT'). Note that it's
5. Connect the RX pin of the CP2105 port to the TX pin of the target device.
6. Connect the TX pin of the CP2105 port to the RX pin of the target device.

Here's an example schematic showing the recommended connections.

![schematic showing CP2105 with recommended bootloader activation connections](images/cp210x_bootloader_activation_hw_block_diagram.png)

Note that the resistors are recommended for a robust production design but not needed to run it as a demo. Also note that HW flow control is not really needed for reliable xmodem transfer, since xmodem implements its own flow control protocol. However, HW flow control is always recommended when using UARTs for Silicon Labs NCP (Network Co-processor) interfaces.

Here's my pinout using a BRD4158A installed on a BRD8016A:

| EFR32xG13 Pin | Function                          | BRD8016A 40-pin Header Pin  |
| ------------- | --------------------------------- | --------------------------  |
| PF6           | Bootloader Entry (active low)     | 7                           |
| nRESET        | Active low reset                  | 16                          |
| PA3           | Target RX                         | 11                          |
| PA2           | Target TX                         | 36                          |

## Installing and Running on Raspberry Pi

1. Clone the repo.

```
$ git clone https://github.com/silabs-KrisY/cp210x_bootloader_activation.git
```

2. Use pip to install the required python libraries.

```
$ pip3 install pyserial --user
$ pip3 install pyusb --user
$ pip3 install xmodem --user
```

3. Note that by default, the "pi" user doesn't have direct access to USB devices. We can provide the "pi" user with access to CP210x devices
by adding the following line to the beginning of /etc/udev/rules.d/99-com.rules:
```
SUBSYSTEM=="usb",ATTRS{idVendor}=="10c4",MODE="0666"
```
  Once you add this, reload the rules so they take effect:
```
$ sudo udevadm control --reload-rules
$ sudo udevadm trigger
```

4. To update the device on the SCI port (COM port /dev/ttyUSB0, USB interface \#0):
```
$ python3 cp210x_xmodem_activation.py flash -p /dev/ttyUSB0 -i 0 -f soc_empty.gbl
Restarting NCP into Bootloader mode...
BL version:Gecko Bootloader v1.12.00

Successfully restarted into bootloader mode! Starting upload of NCP image...
Finished!
Rebooting NCP...
```

5. To update the device on the ECI port (COM port /dev/ttyUSB1, USB interface \#1):
```
$ python3 cp210x_xmodem_activation.py flash -p /dev/ttyUSB1 -i 1 -f soc_empty.gbl
Restarting NCP into Bootloader mode...
BL version:Gecko Bootloader v1.12.00

Successfully restarted into bootloader mode! Starting upload of NCP image...
Finished!
Rebooting NCP...
```

## References
[AN571 - CP210x Virtual COM Port Interface](https://www.silabs.com/documents/public/application-notes/AN571.pdf)


## Reporting Bugs/Issues and Posting Questions and Comments

To report bugs, please create a new "Issue" in the "Issues" section of this repo. Please be as specific as possible (line numbers, reproducing hardware, etc.). If you are proposing a fix, also include information on the proposed fix. Since these examples are provided as-is, there is no guarantee that these examples will be updated to fix these issues.

Questions and comments related to these examples should be made by creating a new "Issue" in the "Issues" section of this repo.

## Disclaimer

This example is considered to be EXPERIMENTAL QUALITY which implies that the code provided in the repo has not been formally tested and is provided as-is.  It is not suitable for production environments.  In addition, this code will not be maintained and there may be no bug maintenance planned for these resources. I may update projects from time to time.
