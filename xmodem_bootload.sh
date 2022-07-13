#!/bin/sh

# xmodem_bootload.sh
# Send a CR and then "1" to the serial port
# and then send a file for xmodem bootloading
# Usage: ./xmodem_bootload <serial port> <gbl file>
# NOTE: Assumes 115200 baud rate, 8N1, no flow control
# This software is EXAMPLE SOFTWARE and is being provided on an AS-IS basis.

# # # # # # # # # # # # # # # # # # # # #
# SPDX-License-Identifier: Zlib
#
# The licensor of this software is Silicon Laboratories Inc.
#
# This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
#
# The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
# Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
# This notice may not be removed or altered from any source distribution.
# # # # # # # # # # # # # # # # # # # # #
if [ "$#" -ne 2 ]; then
 echo "Usage: $0 <serial port> <gbl file>"
 exit 1
fi
# Configure the serial device for 115200, 8N1, no flow control
stty -F $1 115200 cs8 -cstopb -parenb -crtscts -ixoff

# send "\n" to kick the bootloader prompt, send "1" to start xmodem receive on
# the bootloaders
echo -e "\n" > $1
echo -e "1" > $1

# Perform the bootloader transfer using the linux lrzsz utility
sx $2 < $1 > $1
