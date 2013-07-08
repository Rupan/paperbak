PaperBack
===

This project is an attempt to:

 1) document the process by which Paperbak is built
 2) port the application to another compiler (VC++) and/or OS (Linux)

The upstream source of this application is:

 http://ollydbg.de/Paperbak/

A copy of the homepage is mirrored in this repository as upstream.html.

===
Building
===

1) Download the free Embarcadero C++ Compiler 5.5 from here (scroll down):
 https://downloads.embarcadero.com/free/c_builder
2) Install it, then review C:\Borland\BCC55\readme.txt.
3) Create bcc32.cfg and ilink32.cfg as directed.
4) Place this source code in a path without spaces (i.e. C:\paperbak), then build it:
 make -f paperbak.mak
