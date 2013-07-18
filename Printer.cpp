////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// PaperBack -- high density backups on the plain paper                       //
//                                                                            //
// Copyright (c) 2007 Oleh Yuschuk                                            //
// ollydbg at t-online de (set Subject to 'paperback' or be filtered out!)    //
//                                                                            //
//                                                                            //
// This file is part of PaperBack.                                            //
//                                                                            //
// Paperback is free software; you can redistribute it and/or modify it under //
// the terms of the GNU General Public License as published by the Free       //
// Software Foundation; either version 3 of the License, or (at your option)  //
// any later version.                                                         //
//                                                                            //
// PaperBack is distributed in the hope that it will be useful, but WITHOUT   //
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      //
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for   //
// more details.                                                              //
//                                                                            //
// You should have received a copy of the GNU General Public License along    //
// with this program. If not, see <http://www.gnu.org/licenses/>.             //
//                                                                            //
//                                                                            //
// Note that bzip2 compression/decompression library, which is the part of    //
// this project, is covered by different license, which, in my opinion, is    //
// compatible with GPL.                                                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <direct.h>
#include <math.h>
#include "twain.h"
#include "CRYPTO/aes.h"
#include "CRYPTO/pwd2key.h"
#pragma hdrstop

#include "paperbak.h"
#include "resource.h"

// Initializes printer settings. This operation is done blindly, without
// displaying any dialogs. Call once during startup.
void Initializeprintsettings(void) {
  int i,nres,res[64][2];
  DEVMODE *pdevmode;
  DEVNAMES *pdevnames;
  // Get default printer page settings.
  if (pagesetup.lStructSize==0) {
    memset(&pagesetup,0,sizeof(PAGESETUPDLG));
    pagesetup.lStructSize=sizeof(PAGESETUPDLG);
    pagesetup.hwndOwner=hwmain;
    pagesetup.hDevMode=NULL;
    pagesetup.hDevNames=NULL;
    pagesetup.rtMinMargin.left=0;
    pagesetup.rtMinMargin.right=0;
    pagesetup.rtMinMargin.top=0;
    pagesetup.rtMinMargin.bottom=0;
    pagesetup.rtMargin.left=marginleft;
    pagesetup.rtMargin.right=marginright;
    pagesetup.rtMargin.top=margintop;
    pagesetup.rtMargin.bottom=marginbottom;
    pagesetup.Flags=PSD_RETURNDEFAULT|PSD_NOWARNING|PSD_MINMARGINS;
    if (marginunits==1)
      pagesetup.Flags|=PSD_INTHOUSANDTHSOFINCHES|PSD_MARGINS;
    else if (marginunits==2)
      pagesetup.Flags|=PSD_INHUNDREDTHSOFMILLIMETERS|PSD_MARGINS;
    PageSetupDlg(&pagesetup); };
  // By system default, all margins are usually set to 1 inch. This means too
  // much space is excluded from data carrying. So when all margins are 1 inch,
  // I set them to more sound values.
  if (pagesetup.Flags & PSD_INTHOUSANDTHSOFINCHES) {
    if (pagesetup.rtMargin.left==1000 &&
      pagesetup.rtMargin.right==1000 &&
      pagesetup.rtMargin.top==1000 &&
      pagesetup.rtMargin.bottom==1000
    ) {
      pagesetup.rtMargin.right=400;
      pagesetup.rtMargin.top=400;
      pagesetup.rtMargin.bottom=500;
    }; }
  else if (pagesetup.Flags & PSD_INHUNDREDTHSOFMILLIMETERS) {
    if (pagesetup.rtMargin.left==2500 &&
      pagesetup.rtMargin.right==2500 &&
      pagesetup.rtMargin.top==2500 &&
      pagesetup.rtMargin.bottom==2500
    ) {
      pagesetup.rtMargin.right=1000;
      pagesetup.rtMargin.top=1000;
      pagesetup.rtMargin.bottom=1250;
    };
  };
  // Even if I set preferred dmPrintQuality to high, some printer drivers
  // select lower resolution than physically available. Let's try to correct
  // this... er... feature.
  resx=resy=0;
  if (pagesetup.hDevNames!=NULL) {
    pdevnames=(DEVNAMES *)GlobalLock(pagesetup.hDevNames);
    if (pdevnames!=NULL) {
      // Ask for the length of the list of supported resolutions.
      nres=DeviceCapabilities((char *)pdevnames+pdevnames->wDeviceOffset,
        (char *)pdevnames+pdevnames->wOutputOffset,
        DC_ENUMRESOLUTIONS,NULL,NULL);
      // Ask for the resolutions. I'm to lazy to allocate the memory
      // dynamically and assume that no sound driver will support more than 64
      // different resolutions.
      if (nres>0 && nres<=64) {
        DeviceCapabilities((char *)pdevnames+pdevnames->wDeviceOffset,
        (char *)pdevnames+pdevnames->wOutputOffset,
        DC_ENUMRESOLUTIONS,(char *)res,NULL);
        for (i=0; i<nres; i++) {
          if (res[i][0]>=resx && res[i][1]>=resy) {
            resx=res[i][0]; resy=res[i][1];
          };
        };
      };
      GlobalUnlock(pagesetup.hDevNames);
    };
  };
  // Set up preferred printer defaults.
  if (pagesetup.hDevMode==NULL)
    pagesetup.hDevMode=GlobalAlloc(GHND,sizeof(DEVMODE));
  pdevmode=(DEVMODE *)GlobalLock(pagesetup.hDevMode);
  if (pdevmode!=NULL) {
    pdevmode->dmSize=sizeof(DEVMODE);
    pdevmode->dmFields|=DM_PRINTQUALITY|DM_COLOR|DM_DITHERTYPE;
    if (resx==0 || resy==0)
      pdevmode->dmPrintQuality=DMRES_HIGH;
    else {
      pdevmode->dmFields|=DM_YRESOLUTION;
      pdevmode->dmPrintQuality=(short)resx;
      pdevmode->dmYResolution=(short)resy; };
    pdevmode->dmColor=DMCOLOR_MONOCHROME;
    pdevmode->dmDitherType=DMDITHER_LINEART;
    GlobalUnlock(pagesetup.hDevMode);
  };
};

// Frees resources allocated by printing routines.
void Closeprintsettings(void) {
  if ((pagesetup.hDevMode)!=NULL)
    GlobalFree(pagesetup.hDevMode);
  if ((pagesetup.hDevNames)!=NULL)
    GlobalFree(pagesetup.hDevNames);
  ;
};

// Displays dialog asking to enter page borders. I assume that the structure
// pagesetup is already initialized by call to Initializeprintsettings().
void Setuppage(void) {
  pagesetup.rtMinMargin.left=0;
  pagesetup.rtMinMargin.right=0;
  pagesetup.rtMinMargin.top=0;
  pagesetup.rtMinMargin.bottom=0;
  pagesetup.Flags=PSD_MARGINS|PSD_MINMARGINS;
  PageSetupDlg(&pagesetup);
};

// Service function, puts block of data to bitmap as a grid of 32x32 dots in
// the position with given index. Bitmap is treated as a continuous line of
// cells, where end of the line is connected to the start of the next line.
static void Drawblock(int index,t_data *block,uchar *bits,int width,int height,
  int border,int nx,int ny,int dx,int dy,int px,int py,int black
) {
  int i,j,x,y,m,n;
  ulong t;
  // Convert cell index into the X-Y bitmap coordinates.
  x=(index%nx)*(NDOT+3)*dx+2*dx+border;
  y=(index/nx)*(NDOT+3)*dy+2*dy+border;
  bits+=(height-y-1)*width+x;
  // Add CRC.
  block->crc=(ushort)(Crc16((uchar *)block,NDATA+sizeof(ulong))^0x55AA);
  // Add error correction code.
  Encode8((uchar *)block,block->ecc,127);
  // Print block. To increase the reliability of empty or half-empty blocks
  // and close-to-0 addresses, I XOR all data with 55 or AA.
  for (j=0; j<32; j++) {
    t=((ulong *)block)[j];
    if ((j & 1)==0)
      t^=0x55555555;
    else
      t^=0xAAAAAAAA;
    x=0;
    for (i=0; i<32; i++) {
      if (t & 1) {
        for (m=0; m<py; m++) {
          for (n=0; n<px; n++) {
            bits[x-m*width+n]=(uchar)black;
          };
        };
      };
      t>>=1;
      x+=dx;
    };
    bits-=dy*width;
  };
};

// Service function, clips regular 32x32-dot raster to bitmap in the position
// with given block coordinates (may be outside the bitmap).
static void Fillblock(int blockx,int blocky,uchar *bits,int width,int height,
  int border,int nx,int ny,int dx,int dy,int px,int py,int black
) {
  int i,j,x0,y0,x,y,m,n;
  ulong t;
  // Convert cell coordinates into the X-Y bitmap coordinates.
  x0=blockx*(NDOT+3)*dx+2*dx+border;
  y0=blocky*(NDOT+3)*dy+2*dy+border;
  // Print raster.
  for (j=0; j<32; j++) {
    if ((j & 1)==0)
      t=0x55555555;
    else {
      if (blocky<0 && j<=24) t=0;
      else if (blocky>=ny && j>8) t=0;
      else if (blockx<0) t=0xAA000000;
      else if (blockx>=nx) t=0x000000AA;
      else t=0xAAAAAAAA; };
    for (i=0; i<32; i++) {
      if (t & 1) {
        for (m=0; m<py; m++) {
          for (n=0; n<px; n++) {
            x=x0+i*dx+n;
            y=y0+j*dy+m;
            if (x<0 || x>=width || y<0 || y>=height)
              continue;
            bits[(height-y-1)*width+x]=(uchar)black;
          };
        };
      };
      t>>=1;
    };
  };
};

// Stops printing and cleans print descriptor.
void Stopprinting(t_printdata *print) {
  // Finish compression.
  if (print->compression!=0) {
    BZ2_bzCompressEnd(&print->bzstream);
    print->compression=0; };
  // Close input file.
  if (print->hfile!=NULL && print->hfile!=INVALID_HANDLE_VALUE) {
    CloseHandle(print->hfile); print->hfile=NULL; };
  // Deallocate memory.
  if (print->buf!=NULL) {
    GlobalFree((HGLOBAL)print->buf); print->buf=NULL; };
  if (print->readbuf!=NULL) {
    GlobalFree((HGLOBAL)print->readbuf); print->readbuf=NULL; };
  if (print->drawbits!=NULL) {
    GlobalFree((HGLOBAL)print->drawbits); print->drawbits=NULL; };
  // Free other resources.
  if (print->startdoc!=0) {
    EndDoc(print->dc); print->startdoc=0; };
  if (print->dc!=NULL) {
    DeleteDC(print->dc); print->dc=NULL; };
  if (print->hfont6!=NULL && print->hfont6!=GetStockObject(SYSTEM_FONT))
    DeleteObject(print->hfont6);
  print->hfont6=NULL;
  if (print->hfont10!=NULL && print->hfont10!=GetStockObject(SYSTEM_FONT))
    DeleteObject(print->hfont10);
  print->hfont10=NULL;
  if (print->hbmp!=NULL) {
    DeleteObject(print->hbmp); print->hbmp=NULL; print->dibbits=NULL; };
  // Stop printing.
  print->step=0;
};

// Opens input file and allocates memory buffers.
static void Preparefiletoprint(t_printdata *print) {
  ulong l;
  FILETIME created,accessed,modified;
  // Get file attributes.
  print->attributes=GetFileAttributes(print->infile);
  if (print->attributes==0xFFFFFFFF)
    print->attributes=FILE_ATTRIBUTE_NORMAL;
  // Open input file.
  print->hfile=CreateFile(print->infile,GENERIC_READ,FILE_SHARE_READ,
    NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if (print->hfile==INVALID_HANDLE_VALUE) {
    Reporterror("Unable to open file");
    Stopprinting(print);
    return; };
  // Get time of last file modification.
  GetFileTime(print->hfile,&created,&accessed,&modified);
  if (modified.dwHighDateTime==0)
    print->modified=created;
  else
    print->modified=modified;
  // Get original (uncompressed) file size.
  print->origsize=GetFileSize(print->hfile,&l);
  if (print->origsize==0 || print->origsize>MAXSIZE || l!=0) {
    Reporterror("Invalid file size");
    Stopprinting(print);
    return; };
  print->readsize=0;
  // Allocate buffer for compressed file. (If compression is off, buffer will
  // contain uncompressed data). As AES encryption works on 16-byte records,
  // buffer is aligned to next 16-bit border.
  print->bufsize=(print->origsize+15) & 0xFFFFFFF0;
  print->buf=(uchar *)GlobalAlloc(GMEM_FIXED,print->bufsize);
  if (print->buf==NULL) {
    Reporterror("Low memory");
    Stopprinting(print);
    return; };
  // Allocate read buffer. Because compression may take significant time, I
  // pack data in pieces of PACKLEN bytes.
  print->readbuf=(uchar *)GlobalAlloc(GMEM_FIXED,PACKLEN);
  if (print->readbuf==NULL) {
    Reporterror("Low memory");
    Stopprinting(print);
    return; };
  // Set options.
  print->compression=compression;
  print->encryption=encryption;
  print->printheader=printheader;
  print->printborder=printborder;
  print->redundancy=redundancy;
  // Step finished.
  print->step++;
};

// Initializes bzip2 compression engine.
static void Preparecompressor(t_printdata *print) {
  int success;
  // Check whether compression is requested at all.
  if (print->compression==0) {
    print->step++;
    return; };
  // Initialize compressor. On error, I silently disable compression.
  memset(&print->bzstream,0,sizeof(print->bzstream));
  success=BZ2_bzCompressInit(&print->bzstream,
    (print->compression==1?1:9),0,0);
  if (success!=BZ_OK) {
    print->compression=0;              // Disable compression
    print->step++;
    return; };
  print->bzstream.next_out=(char *)print->buf;
  print->bzstream.avail_out=print->bufsize;
  // Step finished.
  print->step++;
};

// Compresses file.
static void Readandcompress(t_printdata *print) {
  int success;
  ulong size,l;
  // Read next piece of data.
  size=print->origsize-print->readsize;
  if (size>PACKLEN) size=PACKLEN;
  success=ReadFile(print->hfile,print->readbuf,size,&l,NULL);
  if (success==0 || l!=size) {
    Reporterror("Unable to read file");
    Stopprinting(print);
    return; };
  // If compression is active, compress next piece of data. Otherwise, just
  // copy data to buffer.
  if (print->compression) {
    Message("Compressing file",(print->readsize+size)*100/print->origsize);
    print->bzstream.next_in=(char *)print->readbuf;
    print->bzstream.avail_in=size;
    success=BZ2_bzCompress(&print->bzstream,BZ_RUN);
    if (print->bzstream.avail_in!=0 || success!=BZ_RUN_OK) {
      Reporterror("Unable to compress data. Try to disable compression.");
      Stopprinting(print);
      return; };
    print->readsize+=size;
    // If compression runs out of memory, probably the data is already packed.
    // Silently restart without compression.
    if (print->readsize<print->origsize && print->bzstream.avail_out==0) {
      BZ2_bzCompressEnd(&print->bzstream);
      print->compression=0;
      SetFilePointer(print->hfile,0,NULL,FILE_BEGIN);
      print->readsize=0;
      return;
    }; }
  else {
    Message("Reading file",(print->readsize+size)*100/print->origsize);
    memcpy(print->buf+print->readsize,print->readbuf,size);
    print->readsize+=size; };
  // If all data is read, finish step.
  if (print->readsize==print->origsize)
    print->step++;
  ;
};

// Finishes compression (may take significant time) and closes input file.
static void Finishcompression(t_printdata *print) {
  int success;
  ulong l;
  // Finish compression.
  if (print->compression) {
    success=BZ2_bzCompress(&print->bzstream,BZ_FINISH);
    // If compression runs out of memory, probably the data is already packed.
    // Silently restart without compression.
    if (success==BZ_FINISH_OK && print->bzstream.avail_out==0) {
      BZ2_bzCompressEnd(&print->bzstream);
      print->compression=0;
      SetFilePointer(print->hfile,0,NULL,FILE_BEGIN);
      print->readsize=0;
      print->step--;
      return; };
    // If compression routine reports other error, stop processing.
    if (success!=BZ_STREAM_END) {
      Reporterror("Unable to compress data. Try to disable compression.");
      Stopprinting(print);
      return; };
    // File compressed. Update size of compressed data and finish.
    print->datasize=print->bzstream.total_out_lo32;
    BZ2_bzCompressEnd(&print->bzstream); }
  else
    print->datasize=print->origsize;
  // Align size of (compressed) data to next 16-byte border. Note that bzip2
  // doesn't mind if data passed to decompressor is longer than expected.
  print->alignedsize=(print->datasize+15) & 0xFFFFFFF0;
  // Zero aligning bytes.
  for (l=print->datasize; l<print->alignedsize; l++)
    print->buf[l]='\0';
  // Close file.
  CloseHandle(print->hfile);
  print->hfile=NULL;
  // Free read buffer. We no longer need it.
  GlobalFree((HGLOBAL)print->readbuf);
  print->readbuf=NULL;
  // Step finished.
  print->step++;
};

// Writes (presumably) random data into the specified buffer
static BOOL WINAPI GenerateRandomData(DWORD dwLen, BYTE *pbBuffer) {
  BOOL result = TRUE;
  HCRYPTPROV hProv;

  if(!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    return FALSE;
  if(!CryptGenRandom(hProv, dwLen, pbBuffer))
    result = FALSE;
  CryptReleaseContext(hProv, 0);
  return result;
}

// Encrypts data. I ask to enter password individually for each file. AES-256
// encryption is very fast, so we don't need to split it into several steps.
static void Encryptdata(t_printdata *print) {
  int n;
  uchar *salt,key[AESKEYLEN],iv[16];
  aes_encrypt_ctx ctx[1];
  // Calculate 16-bit CRC of possibly compressed but unencrypted data. I use
  // it to verify data after decryption: the safe way to assure that password
  // is entered correctly.
  print->bufcrc=Crc16(print->buf,print->alignedsize);
  // Skip rest of this step if encryption is not required.
  if (print->encryption==0) {
    print->step++;
    return; };
  // Ask for password. If user cancels, skip file.
  Message("Encrypting data...",0);
  if (Confirmpassword()!=0) {          // User cancelled encryption
    Message("",0);
    Stopprinting(print);
    return; };
  // Empty password means: leave data unencrypted.
  if (password[0]=='\0') {
    print->encryption=0;
    print->step++;
    return; };
  // Encryption routine expects that password is exactly PASSLEN bytes long.
  // Fill rest of the password with zeros.
  n=strlen(password);
  salt=(uchar *)(print->superdata.name)+32; // hack: put the salt & iv at the end of the name field
  if(GenerateRandomData(32, salt) == FALSE) {
    Message("Failed to generate salt/iv",0);
    Stopprinting(print);
    return; };
  derive_key((const uchar *)password, n, salt, 16, 524288, key, AESKEYLEN);
  memset(password,0,sizeof(password));
  // Initialize encryption.
  memset(ctx,0,sizeof(aes_encrypt_ctx));
  if(aes_encrypt_key((const uchar *)key, AESKEYLEN, ctx) == EXIT_FAILURE) {
    memset(key,0,AESKEYLEN);
    Message("Failed to set encryption key",0);
    Stopprinting(print);
    return; };
  memset(key,0,AESKEYLEN);
  // Encrypt data. AES works with 16-byte data chunks.
  memcpy(iv, salt+16, 16); // the second 16-byte block in 'salt' is the IV
  if(aes_cbc_encrypt(print->buf, print->buf, print->alignedsize, iv, ctx) == EXIT_FAILURE) {
    memset(ctx,0,sizeof(aes_encrypt_ctx));
    Message("Failed to encrypt data",0);
    Stopprinting(print);
    return; };
  // Clear password and encryption control block. We no longer need them.
  memset(ctx,0,sizeof(aes_encrypt_ctx));
  // Step finished.
  print->step++;
};

// Prepares for printing. Despite its size, this routine is very quick.
static void Initializeprinting(t_printdata *print) {
  int i,dx,dy,px,py,nx,ny,width,height,success,rastercaps;
  char fil[MAXPATH],nam[MAXFILE],ext[MAXEXT],jobname[TEXTLEN];
  BITMAPINFO *pbmi;
  SIZE extent;
  PRINTDLG printdlg;
  DOCINFO dinfo;
  DEVNAMES *pdevnames;
  // Prepare superdata.
  print->superdata.addr=SUPERBLOCK;
  print->superdata.datasize=print->alignedsize;
  print->superdata.origsize=print->origsize;
  if (print->compression)
    print->superdata.mode|=PBM_COMPRESSED;
  if (print->encryption)
    print->superdata.mode|=PBM_ENCRYPTED;
  print->superdata.attributes=(uchar)(print->attributes &
    (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|
    FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_ARCHIVE|
    FILE_ATTRIBUTE_NORMAL));
  print->superdata.modified=print->modified;
  print->superdata.filecrc=(ushort)print->bufcrc;
  fnsplit(print->infile,NULL,NULL,nam,ext);
  fnmerge(fil,NULL,NULL,nam,ext);
  // Note that name in superdata may be not null-terminated.
  strncpy(print->superdata.name,fil,32); // don't overwrite the salt and iv at the end of this buffer
  print->superdata.name[31] = '\0'; // ensure that later string operations don't overflow into binary data
  // If printing to paper, ask user to select printer and, if necessary, adjust
  // parameters. I do not enforce high quality or high resolution - the user is
  // the king (well, a sort of).
  if (print->outbmp[0]=='\0') {
    // Open standard Print dialog box.
    memset(&printdlg,0,sizeof(PRINTDLG));
    printdlg.lStructSize=sizeof(PRINTDLG);
    printdlg.hwndOwner=hwmain;
    printdlg.hDevMode=pagesetup.hDevMode;
    printdlg.hDevNames=pagesetup.hDevNames;
    printdlg.hDC=NULL;                 // Returns DC
    printdlg.Flags=PD_ALLPAGES|PD_RETURNDC|PD_NOSELECTION|PD_PRINTSETUP;
    printdlg.nFromPage=1;              // It's hard to calculate the number of
    printdlg.nToPage=9999;             // pages in advance.
    printdlg.nMinPage=1;
    printdlg.nMaxPage=9999;
    printdlg.nCopies=1;
    printdlg.hInstance=hinst;
    success=PrintDlg(&printdlg);
    // Save important information.
    print->dc=printdlg.hDC;
    print->frompage=printdlg.nFromPage-1;
    print->topage=printdlg.nToPage-1;
    // Clean up to prevent memory leaks.
    if (pagesetup.hDevMode==NULL)
      pagesetup.hDevMode=printdlg.hDevMode;
    else if (printdlg.hDevMode!=pagesetup.hDevMode)
      GlobalFree(printdlg.hDevMode);
    if (pagesetup.hDevNames==NULL)
      pagesetup.hDevNames=printdlg.hDevNames;
    else if (printdlg.hDevNames!=pagesetup.hDevNames)
      GlobalFree(printdlg.hDevNames);
    // Analyse results.
    if (success==0) {                  // User cancelled printing
      Message("",0);
      Stopprinting(print);
      return; };
    if (print->dc==NULL) {             // Prointer DC is unavailable
      Reporterror("Unable to access printer");
      Stopprinting(print);
      return; };
    // Assure that printer is capable of displaying bitmaps.
    rastercaps=GetDeviceCaps(print->dc,RASTERCAPS);
    if ((rastercaps & RC_DIBTODEV)==0) {
      Reporterror("The selected printer can't print bitmaps");
      Stopprinting(print);
      return; };
    // Get resolution and size of print area in pixels.
    print->ppix=GetDeviceCaps(print->dc,LOGPIXELSX);
    print->ppiy=GetDeviceCaps(print->dc,LOGPIXELSY);
    width=GetDeviceCaps(print->dc,HORZRES);
    height=GetDeviceCaps(print->dc,VERTRES);
    // Create fonts to draw title and comment. If system is unable to create
    // any font, I get standard one. Of course, standard font will be almost
    // invisible with printer's resolution.
    if (print->printheader) {
      print->hfont6=CreateFont(print->ppiy/6,0,0,0,FW_LIGHT,0,0,0,
        ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
        PROOF_QUALITY,FF_SWISS,NULL);
      print->hfont10=CreateFont(print->ppiy/10,0,0,0,FW_LIGHT,0,0,0,
        ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
        PROOF_QUALITY,FF_SWISS,NULL);
      if (print->hfont6==NULL)
        print->hfont6=(HFONT)GetStockObject(SYSTEM_FONT);
      if (print->hfont10==NULL)
        print->hfont10=(HFONT)GetStockObject(SYSTEM_FONT);
      // Set text color (gray) and alignment (centered).
      SetTextColor(print->dc,RGB(128,128,128));
      SetTextAlign(print->dc,TA_TOP|TA_CENTER);
      // Calculate height of title and info lines on the paper.
      SelectObject(print->dc,print->hfont6);
      if (GetTextExtentPoint32(print->dc,"Page",4,&extent)==0)
        print->extratop=print->ppiy/4;
      else
        print->extratop=extent.cy+print->ppiy/16;
      SelectObject(print->dc,print->hfont10);
      if (GetTextExtentPoint32(print->dc,"Page",4,&extent)==0)
        print->extrabottom=print->ppiy/6;
      else
        print->extrabottom=extent.cy+print->ppiy/24;
      ; }
    else {
      print->hfont6=NULL;
      print->hfont10=NULL;
      print->extratop=print->extrabottom=0; };
    // Dots on paper are black (palette index 0 in the memory bitmap that will
    // be created later in this subroutine).
    print->black=0; }
  // I treat printing to bitmap as a debugging feature and set some more or
  // less sound defaults.
  else {
    print->dc=NULL;
    print->frompage=0;
    print->topage=9999;
    if (resx==0 || resy==0) {
      print->ppix=300; print->ppiy=300; }
    else {
      print->ppix=resx; print->ppiy=resy; };
    if (pagesetup.Flags & PSD_INTHOUSANDTHSOFINCHES) {
      width=pagesetup.ptPaperSize.x*print->ppix/1000;
      height=pagesetup.ptPaperSize.y*print->ppiy/1000; }
    else if (pagesetup.Flags & PSD_INHUNDREDTHSOFMILLIMETERS) {
      width=pagesetup.ptPaperSize.x*print->ppix/2540;
      height=pagesetup.ptPaperSize.y*print->ppiy/2540; }
    else {                             // Use default A4 size (210x292 mm)
      width=print->ppix*8270/1000;
      height=print->ppiy*11690/1000; };
    print->hfont6=NULL;
    print->hfont10=NULL;
    print->extratop=print->extrabottom=0;
    // To simplify recognition of grid on high-contrast bitmap, dots on the
    // bitmap are dark gray.
    print->black=64; };
  // Calculate page borders in the pixels of printer's resolution.
  if (pagesetup.Flags & PSD_INTHOUSANDTHSOFINCHES) {
    print->borderleft=pagesetup.rtMargin.left*print->ppix/1000;
    print->borderright=pagesetup.rtMargin.right*print->ppix/1000;
    print->bordertop=pagesetup.rtMargin.top*print->ppiy/1000;
    print->borderbottom=pagesetup.rtMargin.bottom*print->ppiy/1000; }
  else if (pagesetup.Flags & PSD_INHUNDREDTHSOFMILLIMETERS) {
    print->borderleft=pagesetup.rtMargin.left*print->ppix/2540;
    print->borderright=pagesetup.rtMargin.right*print->ppix/2540;
    print->bordertop=pagesetup.rtMargin.top*print->ppiy/2540;
    print->borderbottom=pagesetup.rtMargin.bottom*print->ppiy/2540; }
  else {
    print->borderleft=print->ppix;
    print->borderright=print->ppix/2;
    print->bordertop=print->ppiy/2;
    print->borderbottom=print->ppiy/2; }
  // Calculate size of printable area, in the pixels of printer's resolution.
  width-=
    print->borderleft+print->borderright;
  height-=
    print->bordertop+print->borderbottom+print->extratop+print->extrabottom;
  // Calculate data point raster (dx,dy) and size of the point (px,py) in the
  // pixels of printer's resolution. Note that pixels, at least in theory, may
  // be non-rectangular.
  dx=max(print->ppix/dpi,2);
  px=max((dx*dotpercent)/100,1);
  dy=max(print->ppiy/dpi,2);
  py=max((dy*dotpercent)/100,1);
  // Calculate width of the border around the data grid.
  if (print->printborder)
    print->border=dx*16;
  else if (print->outbmp[0]!='\0')
    print->border=25;
  else
    print->border=0;
  // Calculate the number of data blocks that fit onto the single page. Single
  // page must contain at least redundancy data blocks plus 1 recovery checksum,
  // and redundancy+1 superblocks with name and size of the data. Data and
  // recovery blocks should be placed into different columns.
  nx=(width-px-2*print->border)/(NDOT*dx+3*dx);
  ny=(height-py-2*print->border)/(NDOT*dy+3*dy);
  if (nx<print->redundancy+1 || ny<3 || nx*ny<2*print->redundancy+2) {
    Reporterror("Printable area is too small, reduce borders or block size");
    Stopprinting(print);
    return; };
  // Calculate final size of the bitmap where I will draw the image.
  width=(nx*(NDOT+3)*dx+px+2*print->border+3) & 0xFFFFFFFC;
  height=ny*(NDOT+3)*dy+py+2*print->border;
  // Fill in bitmap header. To simplify processing, I use 256-color bitmap
  // (1 byte per pixel).
  pbmi=(BITMAPINFO *)print->bmi;
  memset(pbmi,0,sizeof(BITMAPINFOHEADER));
  pbmi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  pbmi->bmiHeader.biWidth=width;
  pbmi->bmiHeader.biHeight=height;
  pbmi->bmiHeader.biPlanes=1;
  pbmi->bmiHeader.biBitCount=8;
  pbmi->bmiHeader.biCompression=BI_RGB;
  pbmi->bmiHeader.biSizeImage=0;
  pbmi->bmiHeader.biXPelsPerMeter=0;
  pbmi->bmiHeader.biYPelsPerMeter=0;      
  pbmi->bmiHeader.biClrUsed=256;
  pbmi->bmiHeader.biClrImportant=256;
  for (i=0; i<256; i++) {
    pbmi->bmiColors[i].rgbBlue=(uchar)i;
    pbmi->bmiColors[i].rgbGreen=(uchar)i;
    pbmi->bmiColors[i].rgbRed=(uchar)i;
    pbmi->bmiColors[i].rgbReserved=0; };
  // Create bitmap. Direct drawing is faster than tens of thousands of API
  // calls.
  if (print->outbmp[0]=='\0') {        // Print to paper
    print->hbmp=CreateDIBSection(print->dc,pbmi,DIB_RGB_COLORS,
      (void **)&(print->dibbits),NULL,0);
    if (print->hbmp==NULL || print->dibbits==NULL) {
      Reporterror("Low memory, can't print");
      Stopprinting(print);
      return;
    }; }
  else {                               // Save to bitmap
    print->drawbits=(uchar *)GlobalAlloc(GMEM_FIXED,width*height);
    if (print->drawbits==NULL) {
      Reporterror("Low memory, can't create bitmap");
      return;
    };
  };
  // Calculate the total size of useful data, bytes, that fits onto the page.
  // For each redundancy blocks, I create one recovery block. For each chain, I
  // create one superblock that contains file name and size, plus at least one
  // superblock at the end of the page.
  print->pagesize=((nx*ny-print->redundancy-2)/(print->redundancy+1))*
    print->redundancy*NDATA;
  print->superdata.pagesize=print->pagesize;
  // Save calculated parameters.
  print->width=width;
  print->height=height;
  print->dx=dx;
  print->dy=dy;
  print->px=px;
  print->py=py;
  print->nx=nx;
  print->ny=ny;
  // Start printing.
  if (print->outbmp[0]=='\0') {
    if (pagesetup.hDevNames!=NULL)
      pdevnames=(DEVNAMES *)GlobalLock(pagesetup.hDevNames);
    else
      pdevnames=NULL;
    memset(&dinfo,0,sizeof(DOCINFO));
    dinfo.cbSize=sizeof(DOCINFO);
    sprintf(jobname,"PaperBack - %.64s",print->superdata.name);
    dinfo.lpszDocName=jobname;
    if (pdevnames==NULL)
      dinfo.lpszOutput=NULL;
    else
      dinfo.lpszOutput=(char *)pdevnames+pdevnames->wOutputOffset;
    success=StartDoc(print->dc,&dinfo);
    if (pdevnames!=NULL)
      GlobalUnlock(pagesetup.hDevNames);
    if (success<=0) {
      Reporterror("Unable to print");
      Stopprinting(print);
      return; };
    print->startdoc=1;
  };
  // Step finished.
  print->step++;
};

// Prints one complete page or saves one bitmap.
static void Printnextpage(t_printdata *print) {
  int dx,dy,px,py,nx,ny,width,height,border,redundancy,black;
  int i,j,k,l,n,success,basex,nstring,npages,rot;
  char s[TEXTLEN],ts[TEXTLEN/2];
  char drv[MAXDRIVE],dir[MAXDIR],nam[MAXFILE],ext[MAXEXT],path[MAXPATH+32];
  uchar *bits;
  ulong u,size,pagesize,offset;
  t_data block,cksum;
  HANDLE hbmpfile;
  BITMAPFILEHEADER bmfh;
  BITMAPINFO *pbmi;
  // Calculate offset of this page in data.
  offset=print->frompage*print->pagesize;
  if (offset>=print->datasize || print->frompage>print->topage) {
    // All requested pages are printed, finish this step.
    print->step++;
    return; };
  // Report page.
  npages=(print->datasize+print->pagesize-1)/print->pagesize;
  sprintf(s,"Processing page %i of %i...",print->frompage+1,npages);
  Message(s,0);
  // Get frequently used variables.
  dx=print->dx;
  dy=print->dy;
  px=print->px;
  py=print->py;
  nx=print->nx;
  ny=print->ny;
  width=print->width;
  border=print->border;
  size=print->alignedsize;
  pagesize=print->pagesize;
  redundancy=print->redundancy;
  black=print->black;
  if (print->outbmp[0]=='\0')
    bits=print->dibbits;
  else
    bits=print->drawbits;
  // Start new page.
  if (print->outbmp[0]=='\0') {
    success=StartPage(print->dc);
    if (success<=0) {
      Reporterror("Unable to print");
      Stopprinting(print);
      return;
    };
  };
  // Check if we can reduce the vertical size of the table on the last page.
  // To assure reliable orientation, I request at least 3 rows.
  l=min(size-offset,pagesize);
  n=(l+NDATA-1)/NDATA;                 // Number of pure data blocks on page
  nstring=                             // Number of groups (length of string)
    (n+redundancy-1)/redundancy;
  n=(nstring+1)*(redundancy+1)+1;      // Total number of blocks to print
  n=max((n+nx-1)/nx,3);                // Number of rows (at least 3)
  if (ny>n) ny=n;
  height=ny*(NDOT+3)*dy+py+2*border;
  // Initialize bitmap to all white.
  memset(bits,255,height*width);
  // Draw vertical grid lines.
  for (i=0; i<=nx; i++) {
    if (print->printborder) {
      basex=i*(NDOT+3)*dx+border;
      for (j=0; j<ny*(NDOT+3)*dy+py+2*border; j++,basex+=width) {
        for (k=0; k<px; k++) bits[basex+k]=0;
      }; }
    else {
      basex=i*(NDOT+3)*dx+width*border+border;
      for (j=0; j<ny*(NDOT+3)*dy; j++,basex+=width) {
        for (k=0; k<px; k++) bits[basex+k]=0;
      };
    };
  };
  // Draw horizontal grid lines.
  for (j=0; j<=ny; j++) {
    if (print->printborder) {
      for (k=0; k<py; k++) {
        memset(bits+(j*(NDOT+3)*dy+k+border)*width,0,width);
      }; }
    else {
      for (k=0; k<py; k++) {
        memset(bits+(j*(NDOT+3)*dy+k+border)*width+border,0,
        nx*(NDOT+3)*dx+px);
      };
    };
  };
  // Fill borders with regular raster.
  if (print->printborder) {
    for (j=-1; j<=ny; j++) {
      Fillblock(-1,j,bits,width,height,border,nx,ny,dx,dy,px,py,black);
      Fillblock(nx,j,bits,width,height,border,nx,ny,dx,dy,px,py,black); };
    for (i=0; i<nx; i++) {
      Fillblock(i,-1,bits,width,height,border,nx,ny,dx,dy,px,py,black);
      Fillblock(i,ny,bits,width,height,border,nx,ny,dx,dy,px,py,black);
    };
  };
  // Update superblock.
  print->superdata.page=
    (ushort)(print->frompage+1);       // Page number is 1-based
  // First block in every string (including redundancy string) is a superblock.
  // To improve redundancy, I avoid placing blocks belonging to the same group
  // in the same column (consider damaged diode in laser printer).
  for (j=0; j<=redundancy; j++) {
    k=j*(nstring+1);
    if (nstring+1>=nx)
      k+=(nx/(redundancy+1)*j-k%nx+nx)%nx;
    Drawblock(k,(t_data *)&print->superdata,
    bits,width,height,border,nx,ny,dx,dy,px,py,black); };
  // Now the most important part - encode and draw data, group by group!
  for (i=0; i<nstring; i++) {
    // Prepare redundancy block.
    cksum.addr=offset ^ (redundancy<<28);
    memset(cksum.data,0xFF,NDATA);
    // Process data group.
    for (j=0; j<redundancy; j++) {
      // Fill block with data.
      block.addr=offset;
      if (offset<size) {
        l=size-offset;
        if (l>NDATA) l=NDATA;
        memcpy(block.data,print->buf+offset,l); }
      else
        l=0;
      // Bytes beyond the data are set to 0.
      while (l<NDATA)
        block.data[l++]=0;
      // Update redundancy block.
      for (l=0; l<NDATA; l++) cksum.data[l]^=block.data[l];
      // Find cell where block will be placed on the paper. The first block in
      // every string is the superblock.
      k=j*(nstring+1);
      if (nstring+1<nx)
        k+=i+1;
      else {
        // Optimal shift between the first columns of the strings is
        // nx/(redundancy+1). Next line calculates how I must rotate the j-th
        // string. Best understandable after two bottles of Weissbier.
        rot=(nx/(redundancy+1)*j-k%nx+nx)%nx;
        k+=(i+1+rot)%(nstring+1); };
      Drawblock(k,&block,bits,width,height,border,nx,ny,dx,dy,px,py,black);
      offset+=NDATA;
    };
    // Process redundancy block in the similar way.
    k=redundancy*(nstring+1);
    if (nstring+1<nx)
      k+=i+1;
    else {
      rot=(nx/(redundancy+1)*redundancy-k%nx+nx)%nx;
      k+=(i+1+rot)%(nstring+1); };
    Drawblock(k,&cksum,bits,width,height,border,nx,ny,dx,dy,px,py,black);
  };
  // Print superblock in all remaining cells.
  for (k=(nstring+1)*(redundancy+1); k<nx*ny; k++) {
    Drawblock(k,(t_data *)&print->superdata,
    bits,width,height,border,nx,ny,dx,dy,px,py,black); };
  // When printing to paper, print title at the top of the page and info text
  // at the bottom.
  if (print->outbmp[0]=='\0') {
    if (print->printheader) {
      // Print title at the top of the page.
      Filetimetotext(&print->modified,ts,sizeof(ts));
      n=sprintf(s,"%.64s [%s, %i bytes] - page %i of %i",
        print->superdata.name,ts,print->origsize,print->frompage+1,npages);
      SelectObject(print->dc,print->hfont6);
      TextOut(print->dc,print->borderleft+width/2,print->bordertop,s,n);
      // Print info at the bottom of the page.
      n=sprintf(s,"Recommended scanner resolution %i dots per inch",
        max(print->ppix*3/dx,print->ppiy*3/dy));
      SelectObject(print->dc,print->hfont10);
      TextOut(print->dc,
        print->borderleft+width/2,
        print->bordertop+print->extratop+height+print->ppiy/24,s,n);
      ;
    };
    // Transfer bitmap to paper and send page to printer.
    SetDIBitsToDevice(print->dc,
      print->borderleft,print->bordertop+print->extratop,
      width,height,0,0,0,height,bits,
      (BITMAPINFO *)print->bmi,DIB_RGB_COLORS);
    EndPage(print->dc); }
  else {
    // Save bitmap to file. First, get file name.
    fnsplit(print->outbmp,drv,dir,nam,ext);
    if (ext[0]=='\0') strcpy(ext,".bmp");
    if (npages>1)
      sprintf(path,"%s%s%s_%04i%s",drv,dir,nam,print->frompage+1,ext);
    else
      sprintf(path,"%s%s%s%s",drv,dir,nam,ext);
    // Create bitmap file.
    hbmpfile=CreateFile(path,GENERIC_WRITE,0,NULL,
      CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (hbmpfile==INVALID_HANDLE_VALUE) {
      Reporterror("Unable to create bitmap file");
      Stopprinting(print);
      return; };
    // Create and save bitmap file header.
    success=1;
    n=sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD);
    bmfh.bfType='BM';
    bmfh.bfSize=sizeof(bmfh)+n+width*height;
    bmfh.bfReserved1=bmfh.bfReserved2=0;
    bmfh.bfOffBits=sizeof(bmfh)+n;
    if (WriteFile(hbmpfile,&bmfh,sizeof(bmfh),&u,NULL)==0 || u!=sizeof(bmfh))
      success=0;
    // Update and save bitmap info header and palette.
    if (success) {
      pbmi=(BITMAPINFO *)print->bmi;
      pbmi->bmiHeader.biWidth=width;
      pbmi->bmiHeader.biHeight=height;
      pbmi->bmiHeader.biXPelsPerMeter=(print->ppix*10000)/254;
      pbmi->bmiHeader.biYPelsPerMeter=(print->ppiy*10000)/254;
      if (WriteFile(hbmpfile,pbmi,n,&u,NULL)==0 || u!=(ulong)n) success=0; };
    // Save bitmap data.
    if (success) {
      if (WriteFile(hbmpfile,bits,width*height,&u,NULL)==0 ||
        u!=(ulong)(width*height))
        success=0;
      ;  
    };
    CloseHandle(hbmpfile);
    if (success==0) {
      Reporterror("Unable to save bitmap");
      Stopprinting(print);
      return;
    };
  };
  // Page printed, proceed with next.
  print->frompage++;
};

// Prints data or saves it to bitmaps page by page.
void Nextdataprintingstep(t_printdata *print) {
  switch (print->step) {
    case 0:                            // Printer idle
      return;
    case 1:                            // Open file and allocate buffers
      Preparefiletoprint(print);
      break;
    case 2:                            // Initialize compression engine
      Preparecompressor(print);
      break;
    case 3:                            // Read next piece of data and compress
      Readandcompress(print);
      break;
    case 4:                            // Finish compression and close file
      Finishcompression(print);
      break;
    case 5:                            // Encrypt data
      Encryptdata(print);
      break;
    case 6:                            // Initialize printing
      Initializeprinting(print);
      break;
    case 7:                            // Print pages, one at a time
      Printnextpage(print);
      break;
    case 8:                            // Finish printing.
      Stopprinting(print);
      Message("",0);
      print->step=0;
    default: break;                    // Internal error
  };
  if (print->step==0) Updatebuttons(); // Right or wrong, decoding finished
};

// Sends specified file to printer (bmp=NULL) or to bitmap file.
void Printfile(char *path,char *bmp) {
  // Stop printing of previous file, if any.
  Stopprinting(&printdata);
  // Prepare descriptor.
  memset(&printdata,0,sizeof(printdata));
  strncpy(printdata.infile,path,MAXPATH-1);
  if (bmp!=NULL)
    strncpy(printdata.outbmp,bmp,MAXPATH-1);
  // Start printing.
  printdata.step=1;
  Updatebuttons();
};

