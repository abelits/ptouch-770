#!/usr/bin/env python2.7

# Copyright (c) 2017-2020 Vitaly Puzrin
# Portions Copyright (c) 2020 Simon Peter
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


from labelmaker_encode import encode_raster_transfer, read_png, unsigned_char

import binascii
import packbits
import serial
import sys
import time

STATUS_OFFSET_BATTERY = 6
STATUS_OFFSET_EXTENDED_ERROR = 7
STATUS_OFFSET_ERROR_INFO_1 = 8
STATUS_OFFSET_ERROR_INFO_2 = 9
STATUS_OFFSET_STATUS_TYPE = 18
STATUS_OFFSET_PHASE_TYPE = 19
STATUS_OFFSET_NOTIFICATION = 22

STATUS_TYPE = [
    "Reply to status request",
    "Printing completed",
    "Error occured",
    "IF mode finished",
    "Power off",
    "Notification",
    "Phase change",
]

STATUS_BATTERY = [
    "Full",
    "Half",
    "Low",
    "Change batteries",
    "AC adapter in use"
]

def print_status(raw):
    if len(raw) != 32:
        print "Error: status must be 32 bytes. Got " + str(len(raw))
        return

    if raw[STATUS_OFFSET_STATUS_TYPE] < len(STATUS_TYPE):
        print "Status: " + STATUS_TYPE[raw[STATUS_OFFSET_STATUS_TYPE]]
    else:
        print "Status: 0x" + binascii.hexlify(raw[STATUS_OFFSET_STATUS_TYPE])

    if raw[STATUS_OFFSET_BATTERY] < len(STATUS_BATTERY):
        print "Battery: " + STATUS_BATTERY[raw[STATUS_OFFSET_BATTERY]]
    else:
        print "Battery: 0x" + binascii.hexlify(raw[STATUS_OFFSET_BATTERY])

    print "Error info 1: 0x" + binascii.hexlify(raw[STATUS_OFFSET_ERROR_INFO_1])
    print "Error info 2: 0x" + binascii.hexlify(raw[STATUS_OFFSET_ERROR_INFO_2])
    print "Extended error: 0x" + binascii.hexlify(raw[STATUS_OFFSET_EXTENDED_ERROR])
    print


# Check for input image
if len(sys.argv) < 3:
    print "Usage: %s <path-to-image> <path to printer /dev/...>" % sys.argv[0]
    sys.exit(1)
    read_size = ""

# Get serial device
if "ulpt" in sys.argv[2]:
    # Tested with Brother P-Touch P-700 over USB on FreeBSD
    ser = open(sys.argv[2], "w+")
    read_size = None
else:
    # Brother P-Touch Cube over Bluetooth
    ser = serial.Serial(
        sys.argv[2],
        baudrate=9600,
        stopbits=serial.STOPBITS_ONE,
        parity=serial.PARITY_NONE,
        bytesize=8,
        dsrdtr=True
    )
    read_size = "size=32"

print(ser.name)

# Read input image into memory
data = read_png(sys.argv[1])

# Enter raster graphics (PTCBP) mode
ser.write(b"\x1b\x69\x61\x01")

# Initialize
ser.write(b"\x1b\x40")

# Dump status
ser.write(b"\x1b\x69\x53")
if "ulpt" in ser.name:
    print_status(ser.read() )
else:
    print_status(ser.read(read_size))

# Flush print buffer
for i in range(64):
    ser.write(b"\x00")

# Initialize
ser.write(b"\x1b\x40")

# Enter raster graphics (PTCBP) mode
ser.write(b"\x1b\x69\x61\x01")

# Found docs on http://www.undocprint.org/formats/page_description_languages/brother_p-touch
ser.write(b"\x1B\x69\x7A") # Set media & quality
ser.write(b"\xC4\x01") # print quality, continuous roll
ser.write(b"\x0C") # Tape width in mm
ser.write(b"\x00") # Label height in mm (0 for continuous roll)

# Number of raster lines in image data
raster_lines = len(data) / 16
print raster_lines, raster_lines % 256, int(raster_lines / 256)
ser.write( unsigned_char.pack( raster_lines % 256 ) )
ser.write( unsigned_char.pack( int(raster_lines / 256) ) )

# Unused data bytes in the "set media and quality" command
ser.write(b"\x00\x00\x00\x00")

# Set print chaining off (0x8) or on (0x0)
ser.write(b"\x1B\x69\x4B\x08")

# Set no mirror, no auto tape cut
ser.write(b"\x1B\x69\x4D\x00")

# Set margin amount (feed amount)
ser.write(b"\x1B\x69\x64\x00\x00")

# Set compression mode: TIFF
ser.write(b"\x4D\x02")

# Send image data
print("Sending image data")
ser.write( encode_raster_transfer(data) )
print "Done"

# Print and feed
ser.write(b"\x1A")

# Dump status that the printer returns
if "ulpt" in ser.name:
    print_status(ser.read() )
else:
    print_status(ser.read(read_size))

# Initialize
ser.write(b"\x1b\x40")

ser.close()
