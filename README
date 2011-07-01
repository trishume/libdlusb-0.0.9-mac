LibDLUSB-Mac
============

About the Port
--------------

This is a port of libdlusb from libusb to [signal 11's hidapi](http://www.signal11.us/oss/hidapi/)

All the changes are made in the makefiles,usb_libusb.h and usb_libusb.c

The old files are kept in the old folder

DISCLAIMER
-------------

This software can cause severe damage to your Timex Data Link USB watch.  Use
at your own risk.

Be prepared to do hard-reset!


INTRODUCTION
-------------

libdlusb is a library used to communicate with the Timex Data Link USB watch on
various UN*X operating systems.  The library can be built with two USB
libraries: libusb (OS-independent USB library) and usbhid (USB HID library for
*BSD).

The communication protocol was implemented based on "Data Link USB
Communication Protocol and Database Design Guide," which is distributed by
Timex Corporation.

Homepage: http://geni.ath.cx/libdlusb.html


COMPILATION
-------------

1. Create Makefile.  The configure script generates Makefile for libusb support
   by default:

   ./configure --prefix=$HOME

2. Build the library and test programs:

   make

3. Install programs under $HOME/bin (${prefix}/bin):

   make install

4. Edit $HOME/.dlusbrc

Now you have libdlusb.a and several test programs.  If you want a shared
library, add --enable-shared option to configure (./configure --enable-shared)
and run make.


BUG REPORT
-------------

If you find a bug or write a useful program with the library, please let me
know: http://groups.yahoo.com/group/timexdatalinkusbdevelop
