# LRPT decoder
This is a free, open-source LRPT decoder. It reads samples from a 8-bit soft-QPSK
file and outputs an image displaying the specified APIDs.


## Compiling and installing

Make sure you have `libpng-dev` installed on your system.
Type `make` in the project root to compile, and `make install` to install the 
binary into /usr/bin/.


## Usage info

```
LRPT decoder v0.0b

Usage: ./src/meteor_decode [options] file_in
   -a, --apid R,G,B        Specify APIDs to parse (default: 68,65,64)
   -d, --diff              Differentially decode (e.g. for Meteor-M N2-2)
   -o, --output <file>     Output composite png to <file>
   -t, --statfile          Write the .stat file containing timing data

   -h, --help              Print this help screen
   -v, --version           Print version info
```
