# Brother P-touch P700 label printer for Linux

## Building

On Debian/Ubuntu-like systems, compile and install with

```
sudo apt install git make gcc libudev-dev python3-pil python-usb libusb-1.0-0 python3-gi gir1.2-gtk-3.0
git clone https://github.com/abelits/ptouch-770
cd ptouch-770/
make
strip ptouch-770-write
sudo cp ptouch-770-write ptouch-770-gui /usr/local/bin/
```
## Usage (Python GTK3 GUI)

![screenshot](https://user-images.githubusercontent.com/2480569/47957306-38ca8c00-dfab-11e8-8d83-3d81aa30278f.png)

## Usage (Python script)

The Python script can be used to print multi-line text like this:

```
./ptouch-770.py --text "test\nfoo"
```

## Usage (raw tool)

Print a test image with

```
sudo ptouch-770-write tux-128px-bw.pbm
```

On the P700, this will result in the printer spitting out approximately 20mm of unprinted label, then cutting the label, then printing the Tux graphic on another approx. 20mm of label and cutting the label again.

To print some text using a 12mm TZe tape:

```
#!/bin/bash

# The following works for 12mm TZe tapes
convert -size x100 -gravity south -pointsize 72 caption:"Test" write.pbm
ptouch-770-write write.pbm 
```
