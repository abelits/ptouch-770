# Brother PT-H500/P700/E500 printer control utility.

## Building

On Debian/Ubuntu-like systems, compile and install with

```
git clone https://github.com/abelits/ptouch-770
cd ptouch-770/
sudo apt install make gcc libudev-dev
make
strip ptouch-770-write
sudo cp ptouch-770-write /usr/local/bin/
```
## Usage

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
