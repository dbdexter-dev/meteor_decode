Meteor-M series LRPT decoder
=======================================

This is a free, open-source LRPT decoder. It reads samples from a 8-bit
soft-QPSK file and outputs an image displaying the specified APIDs.

Features:
- Support for regular (72k) and interleaved (80k) modes
- Support for differential decoding
- Automatic RGB123/RGB125 composite output based on active APIDs
- APID 70 raw dump
- Native BMP output
- Optional PNG output (requires libpng)
- Split channels output
- Read samples from stdin (pass `-` in place of a filename)
- Ctrl-C at any point to write the image and exit (useful when decoding a stream of symbols)


Build/install instructions
--------------------------

```
mkdir build && cd build
cmake ..
make
sudo make install
```

By default, support for both BMP and PNG output formats will be compiled in,
provided that `libpng` is installed.
The output format will then be chosen based on the extension of the output image.

If you don't need PNG support, you can disable it by running
`cmake -DUSE_PNG=OFF ..` when configuring.


Sample output
-------------
```
 24.54%    vit(avg): 1126 rs(sum): 28       APID 64  seq: 2009311  11:30:23.468
 ^         ^              ^                 ^        ^             ^
 file      average        Reed-Solomon      current  current VCDU  timestamp
 progress  Viterbi path   errors corrected  APID     sequence      reported by the
           length per     (-1 if too many            number        satellite (MSK time)
           byte (lower    errors detected)
           is better,
           ~1100 is usually enough
           to get a decode)
```


Usage
-----

```
meteor_decode [options] input.s
	-7, --70               Dump APID70 data in a separate file
	-a, --apid R,G,B       Specify APIDs to parse (default: autodetect)
	-b, --batch            Batch mode (disable all non-printable characters)
	-d, --diff             Perform differential decoding
	-i, --int              Deinterleave samples (aka 80k mode)
	-o, --output <file>    Output composite image to <file>
	-q, --quiet            Disable decoder status output
	-s, --split            Write each APID in a separate file
	-t, --statfile         Write .stat file

	-h, --help             Print this help screen
	-v, --version          Print version information
```

Typical use cases:
- Meteor-M2, RGB123/125: `meteor_decode <input.s> -o <output.png>`
- Meteor-M2, RGB122: `meteor_decode <input.s> -o <output.png> -a 65,65,64`
