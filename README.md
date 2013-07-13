PaperBack
===

This project is an attempt to:

1) document the process by which Paperbak is built
2) port the application to another compiler (VC++) and/or OS (Linux)

The upstream source of this application is:

 http://ollydbg.de/Paperbak/

A copy of the homepage is mirrored in this repository as upstream.html.

bzip2
===
bzip2, a compression library, is used with this project.  Its source code can be downloaded here:

http://www.bzip.org/downloads.html

The library included in this repository, bzip2.lib, can be built as follows:

bcc32 -Hc -w -Vx -Ve -C -ff -X- -a8 -b -d -k- -vi -y -v -c compress.c crctable.c decompress.c bzlib.c blocksort.c huffman.c randtable.c
tlib bzip2.lib /C +compress+crctable+decompress+bzlib+blocksort+huffman+randtable

Building
===

1) Download the free Embarcadero C++ Compiler 5.5 from here (scroll down):

https://downloads.embarcadero.com/free/c_builder

2) Install it, then review C:\Borland\BCC55\readme.txt.
3) Create bcc32.cfg and ilink32.cfg as directed.
4) Edit the system path so that it includes the Borland binaries.  In regedit, navigate to:

HKEY_LOCAL_MACHINE -> System -> CurrentControlSet -> Control -> Session Manager -> Environment -> PATH
Now prepend the path C:\Borland\BCC55\Bin

5) Place this source code in a path without spaces (i.e. C:\paperbak), then build it:

make -f paperbak.mak
