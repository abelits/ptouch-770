# Brother PT-H500/P700/E500 printer control utility.

## Building

On Debian/Ubuntu-like systems, compile and install with

```
git clone https://github.com/abelits/ptouch-770
cd ptouch-770/
sudo apt install make gcc libudev-dev
strip ptouch-770-write
make
sudo cp ptouch-770-write /usr/local/bin/
```
