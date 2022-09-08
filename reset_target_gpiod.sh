#!/bin/sh

# reset_target_gpiod.sh <gpio device file> <btl_act_pin> <nrst_pin> -btl_act
#
# Reset Silicon Labs target with or without forcing bootloader activation pin
# low
# Example forcing btl_act low with gpiochip3, btl_act_pin=1, nrst_pin=0:
#  ./reset_target_gpiod.sh gpiochip3 1 0 -btl_act
# Omit -btl_act argument for simple target reset without GPIO activation
# NOTE: GPIO activation assumes gecko bootloader bootloader activation
# polarity is active low
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

if [ "$#" -lt 3 ]; then
  echo "Usage:"
  echo "\tReset target with bootloader activation: "
  echo "\t\t$0 <gpio device file> <btl_act_pin> <nrst_pin> -btl_act"
  echo "\tReset target without bootloader activation: "
  echo "\t\t$0 <gpio device file> <btl_act_pin> <nrst_pin>"
  exit 1
fi
gpiod_dev=$1
btl_act_pin=$2
nrst_pin=$3

if [ "$4" = "-btl_act" ]; then
  echo "Invoking bootloader with btl_act low"
  #Force btl_act low
  gpioset $gpiod_dev $btl_act_pin=0
else
  echo "Invoking bootloader with btl_act high"
fi

#assert nRESET for 1ms and then deassert
gpioset --mode=time --usec=1000 $gpiod_dev $nrst_pin=0
gpioset $gpiod_dev $nrst_pin=1

if [ "$4" = "-btl_act" ]; then
  #Sleep for a short time then de-assert btl_act
  #wait for 50ms
  sleep 0.05
  #btl_act high
  gpioset $gpiod_dev $btl_act_pin=1
fi
