PaperBack
===

This goal of this project is to:

1) document the process by which Paperbak is built
2) fix its crypto implementation

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

crypto
===

This directory contains Brian Gladman's AES and SHA code, including HMAC and key derivation routines.
The sources were built as follows:

in AES (from https://github.com/Rupan/aes):
First, edit aes_x86_v1.asm and add 'use32' to the .text section header (otherwise 16-bit code is generated).
nasm -f obj -F borland aes_x86_v1.asm
bcc32 -Hc -w -Vx -Ve -C -ff -X- -a8 -b -d -k- -vi -y -v -c -DASM_X86_V1C -DLITTLE_ENDIAN aeskey.c aestab.c aes_modes.c

in SHA (from https://github.com/Rupan/sha):
bcc32 -Hc -w -Vx -Ve -C -ff -X- -a8 -b -d -k- -vi -y -v -c -DLITTLE_ENDIAN -DUSE_SHA256 hmac.c pwd2key.c sha2.c

then put them all into a library:
tlib crypto.lib /C +aeskey+aestab+aes_x86_v1+hmac+pwd2key+sha2+aes_modes

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

Changelog
===

1.00 - First public release
1.10 - Fix crypto implementation
       Switch to precompiled libraries
