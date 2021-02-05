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


import packbits
import png
import struct

# "Raster graphics transfer" serial command
TRANSFER_COMMAND = b"\x47"

unsigned_char = struct.Struct('B');
def as_unsigned_char(byte):
    """ Interpret a byte as an unsigned int """
    return unsigned_char.unpack(byte)[0]

def encode_raster_transfer(data):
    """ Encode 1 bit per pixel image data for transfer over serial to the printer """
    buf = bytearray()

    # Send in chunks of 1 line (128px @ 1bpp = 16 bytes)
    # This mirrors the official app from Brother. Other values haven't been tested.
    chunk_size = 16

    for i in xrange(0, len(data), chunk_size):
        chunk = data[i : i + chunk_size]

        # Encode as tiff
        packed_chunk = packbits.encode(chunk)

        # Write header
        buf.append(TRANSFER_COMMAND)

        # Write number of bytes to transfer (n1 + n2*256)
        length = len(packed_chunk)
        buf.append(unsigned_char.pack( int(length % 256) ))
        buf.append(unsigned_char.pack( int(length / 256) ))

        # Write data
        buf.extend(packed_chunk)

    return buf

def decode_raster_transfer(data):
    """ Read data encoded as T encoded as TIFF with transfer headers """

    buf = bytearray()
    i = 0

    while i < len(data):
        if data[i] == TRANSFER_COMMAND:
            # Decode number of bytes to transfer
            n1 = as_unsigned_char(data[i+1])
            n2 = as_unsigned_char(data[i+2])
            num_bytes = n1 + n2*256

            # Copy contents of transfer to output buffer
            transferedData = data[i + 3 : i + 3 + num_bytes]
            buf.extend(transferedData)

            # Confirm
            if len(transferedData) != num_bytes:
                raise Exception("Failed to read %d bytes at index %s: end of input data reached." % (num_bytes, i))

            # Shift to the next position after these command and data bytes
            i = i + 3 + num_bytes

        else:
            raise Exception("Unexpected byte %s" % data[i])

    return buf

def read_png(path):
    """ Read a (monochrome) PNG image and convert to 1bpp raw data

    This should work with any 8 bit PNG. To ensure compatibility, the image can
    be processed with Imagemagick first using the -monochrome flag.
    """

    buf = bytearray()

    # State for bit packing
    bit_cursor = 8
    byte = 0

    # Read the PNG image
    reader = png.Reader(filename=path)
    width, height, rows, metadata = reader.asRGB()

    # Loop over image and pack into 1bpp buffer
    for row in rows:
        for pixel in xrange(0, len(row), 3):
            bit_cursor -= 1

            if row[pixel] == 0:
                byte |= (1 << bit_cursor)

            if bit_cursor == 0:
                buf.append(unsigned_char.pack(byte))
                byte = 0
                bit_cursor = 8

    return buf