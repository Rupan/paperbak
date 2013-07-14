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
#pragma hdrstop

#include "paperbak.h"
#include "resource.h"

static HINSTANCE htwaindll;            // Handle of TWAIN_32.DLL
static DSMENTRYPROC dsmentry;          // Address of DSM_Entry()
static TW_IDENTITY appid;              // Application's identity structure
static TW_IDENTITY source;             // Opened TWAIN source

// Processes data from the scanner.
int ProcessDIB(HGLOBAL hdata,int offset) {
  int i,j,sizex,sizey,ncolor;
  uchar scale[256],*data,*pdata,*pbits;
  BITMAPINFO *pdib;
  pdib=(BITMAPINFO *)GlobalLock(hdata);
  if (pdib==NULL)
    return -1;                         // Something is wrong with this DIB
  // Check that bitmap is more or less valid.
  if (pdib->bmiHeader.biSize!=sizeof(BITMAPINFOHEADER) ||
    pdib->bmiHeader.biPlanes!=1 ||
    (pdib->bmiHeader.biBitCount!=8 && pdib->bmiHeader.biBitCount!=24) ||
    (pdib->bmiHeader.biBitCount==24 && pdib->bmiHeader.biClrUsed!=0) ||
    pdib->bmiHeader.biCompression!=BI_RGB ||
    pdib->bmiHeader.biWidth<128 || pdib->bmiHeader.biWidth>32768 ||
    pdib->bmiHeader.biHeight<128 || pdib->bmiHeader.biHeight>32768
  ) {
    GlobalUnlock(hdata);
    return -1; };                      // Not a known bitmap!
  sizex=pdib->bmiHeader.biWidth;
  sizey=pdib->bmiHeader.biHeight;
  ncolor=pdib->bmiHeader.biClrUsed;
  // Convert bitmap to 8-bit grayscale. Note that scan lines are DWORD-aligned.
  data=(uchar *)GlobalAlloc(GMEM_FIXED,sizex*sizey);
  if (data==NULL) {
    GlobalUnlock(hdata);
    return -1; };
  if (pdib->bmiHeader.biBitCount==8) {
    // 8-bit bitmap with palette.
    if (ncolor>0) {
      for (i=0; i<ncolor; i++) {
        scale[i]=(uchar)((pdib->bmiColors[i].rgbBlue+
        pdib->bmiColors[i].rgbGreen+pdib->bmiColors[i].rgbRed)/3);
      }; }
    else {
      for (i=0; i<256; i++) scale[i]=(uchar)i; };
    if (offset==0)
      offset=sizeof(BITMAPINFOHEADER)+ncolor*sizeof(RGBQUAD);
    pdata=data;
    for (j=0; j<sizey; j++) {
      offset=(offset+3) & 0xFFFFFFFC;
      pbits=((uchar *)(pdib))+offset;
      for (i=0; i<sizex; i++) {
        *pdata++=scale[*pbits++]; };
      offset+=sizex;
    }; }
  else {
    // 24-bit bitmap without palette.
    if (offset==0)
      offset=sizeof(BITMAPINFOHEADER)+ncolor*sizeof(RGBQUAD);
    pdata=data;
    for (j=0; j<sizey; j++) {
      offset=(offset+3) & 0xFFFFFFFC;
      pbits=((uchar *)(pdib))+offset;
      for (i=0; i<sizex; i++) {
        *pdata++=(uchar)((pbits[0]+pbits[1]+pbits[2])/3);
        pbits+=3; };
      offset+=sizex*3;
    };
  };
  // Decode bitmap. This is what we are for here.
  Startbitmapdecoding(&procdata,data,sizex,sizey);
  // Free original bitmap and report success.
  GlobalUnlock(hdata);
  return 0;
};

// Opens and decodes bitmap. Returns 0 on success and -1 on error.
int Decodebitmap(char *path) {
  int i,size;
  char s[TEXTLEN+MAXPATH],fil[MAXFILE],ext[MAXEXT];
  uchar *data,buf[sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)];
  FILE *f;
  BITMAPFILEHEADER *pbfh;
  BITMAPINFOHEADER *pbih;
  HCURSOR prevcursor;
  // Ask for file name.
  if (path==NULL || path[0]=='\0') {
    if (Selectinbmp()!=0) return -1; }
  else {
    strncpy(inbmp,path,sizeof(inbmp));
    inbmp[sizeof(inbmp)-1]='\0'; };
  fnsplit(inbmp,NULL,NULL,fil,ext);
  sprintf(s,"Reading %s%s...",fil,ext);
  Message(s,0);
  Updatebuttons();
  // Open file and verify that this is the valid bitmap of known type.
  f=fopen(inbmp,"rb");
  if (f==NULL) {                       // Unable to open file
    sprintf(s,"Unable to open %s%s",fil,ext);
    Reporterror(s);
    return -1; };
  // Reading 100-MB bitmap may take many seconds. Let's inform user by changing
  // mouse pointer.
  prevcursor=SetCursor(LoadCursor(NULL,IDC_WAIT));
  i=fread(buf,1,sizeof(buf),f);
  SetCursor(prevcursor);
  if (i!=sizeof(buf)) {                // Unable to read file
    sprintf(s,"Unable to read %s%s",fil,ext);
    Reporterror(s);
    fclose(f); return -1; };
  pbfh=(BITMAPFILEHEADER *)buf;
  pbih=(BITMAPINFOHEADER *)(buf+sizeof(BITMAPFILEHEADER));
  if (pbfh->bfType!='BM' ||
    pbih->biSize!=sizeof(BITMAPINFOHEADER) || pbih->biPlanes!=1 ||
    (pbih->biBitCount!=8 && pbih->biBitCount!=24) ||
    (pbih->biBitCount==24 && pbih->biClrUsed!=0) ||
    pbih->biCompression!=BI_RGB ||
    pbih->biWidth<128 || pbih->biWidth>32768 ||
    pbih->biHeight<128 || pbih->biHeight>32768
  ) {                                  // Invalid bitmap type
    sprintf(s,"Unsupported bitmap type: %s%s",fil,ext);
    Reporterror(s);
    fclose(f); return -1; };
  // Allocate buffer and read file.
  fseek(f,0,SEEK_END);
  size=ftell(f)-sizeof(BITMAPFILEHEADER);
  data=(uchar *)GlobalAlloc(GMEM_FIXED,size);
  if (data==NULL) {                    // Unable to allocate memory
    Reporterror("Low memory");
    fclose(f); return -1; };
  fseek(f,sizeof(BITMAPFILEHEADER),SEEK_SET);
  i=fread(data,1,size,f);
  fclose(f);
  if (i!=size) {                       // Unable to read bitmap
    sprintf(s,"Unable to read %s%s",fil,ext);
    Reporterror(s);
    GlobalFree((HGLOBAL)data);
    return -1; };
  // Process bitmap.
  ProcessDIB((HGLOBAL)data,pbfh->bfOffBits-sizeof(BITMAPFILEHEADER));
  GlobalFree((HGLOBAL)data);
  return 0;
};

// Opens TWAIN manager. Returns 0 on success and -1 on error.
int OpenTWAINmanager(void) {
  TW_UINT16 result;
  if (dsmentry==NULL || twainstate<2)
    return -1;                         // TWAIN DLL was not initialized
  if (twainstate>2)
    return 0;                          // Manager is already initialized
  appid.Id=0;
  appid.Version.MajorNum=VERSIONHI;
  appid.Version.MinorNum=VERSIONLO;
  appid.Version.Language=TWLG_ENGLISH_USA;
  appid.Version.Country=TWCY_USA;
  lstrcpy(appid.Version.Info,"PaperBack");
  appid.ProtocolMajor=TWON_PROTOCOLMAJOR;
  appid.ProtocolMinor=TWON_PROTOCOLMINOR;
  appid.SupportedGroups=DG_IMAGE|DG_CONTROL;
  lstrcpy(appid.Manufacturer,"");
  lstrcpy(appid.ProductFamily,"");
  lstrcpy(appid.ProductName,"PaperBack");
  result=dsmentry(&appid,NULL,
    DG_CONTROL,DAT_PARENT,MSG_OPENDSM,(TW_MEMREF)&hwmain);
  if (result!=TWRC_SUCCESS)
    return -1;
  else {
    twainstate=3;                      // TWAIN source manager open
    return 0;
  };
};

// Select image source. Returns 0 on success and -1 on error.
int SelectTWAINsource(void) {
  TW_UINT16 result;
  if (twainstate<3)                    // Manager inactive, try to initialize
    OpenTWAINmanager();
  if (twainstate!=3)
    return -1;                         // Not a good time for this operation
  result=dsmentry(&appid,NULL,
    DG_CONTROL,DAT_IDENTITY,MSG_USERSELECT,(TW_MEMREF)&source);
  return (result==TWRC_SUCCESS?0:-1);
};

// Opens and enables TWAIN interface. Returns 0 on success and -1 on error.
int OpenTWAINinterface(void) {
  TW_UINT16 result;
  TW_USERINTERFACE interf;
  if (twainstate<3)                    // Manager inactive, try to initialize
    OpenTWAINmanager();
  if (twainstate!=3)
    return -1;                         // Not a good time for this operation
  result=dsmentry(&appid,NULL,
    DG_CONTROL,DAT_IDENTITY,MSG_OPENDS,(TW_MEMREF)&source);
  if (result!=TWRC_SUCCESS) {
    // Unable to open source. The message is usually, but not always, correct.
    Reporterror("There are no scanner devices on the system");
    return -1; };
  interf.ShowUI=1;
  interf.ModalUI=0;
  interf.hParent=(TW_HANDLE)hwmain;
  result=dsmentry(&appid,&source,
    DG_CONTROL,DAT_USERINTERFACE,MSG_ENABLEDS,&interf);
  if (result!=TWRC_SUCCESS) {
    dsmentry(&appid,NULL,              // Unable to enable, go back to state 3
      DG_CONTROL,DAT_IDENTITY,MSG_CLOSEDS,(TW_MEMREF)&source);
    Reporterror("Unable to open scanner");
    return -1; };
  twainstate=5;
  return 0;
};

// Gets picture(s) from the TWAIN.
int GetpicturefromTWAIN(void) {
  int nextimage;
  TW_UINT16 result,xferres;
  TW_IMAGEINFO imageinfo;
  TW_PENDINGXFERS pending;
  HGLOBAL hdata;
  if (twainstate!=5)
    return -1;                         // Not a good time to get the picture
  nextimage=1;
  while (nextimage) {
    // Get image information, like resolution and size.
    result=dsmentry(&appid,&source,
      DG_IMAGE,DAT_IMAGEINFO,MSG_GET,(TW_MEMREF)&imageinfo);
    if (result!=TWRC_SUCCESS)
      continue;                        // Is it correct? I follow the TWAIN...
    // Start data transfer.
    hdata=NULL;
    xferres=dsmentry(&appid,&source,
      DG_IMAGE,DAT_IMAGENATIVEXFER,MSG_GET,(TW_MEMREF)&hdata);
    // After transfer is finished, no matter how, finish transfer.
    result=dsmentry(&appid,&source,
      DG_CONTROL,DAT_PENDINGXFERS,MSG_ENDXFER,(TW_MEMREF)&pending);
    if (result!=TWRC_SUCCESS || pending.Count==0)
      nextimage=0;
    switch (xferres) {
      case TWRC_XFERDONE:              // Transfer finished, hbmp valid
        if (hdata!=NULL) {
          ProcessDIB(hdata,0);
          GlobalUnlock(hdata);
          GlobalFree(hdata); };
        break;
      case TWRC_CANCEL:                // Bitmap exists but has invalid data
        if (hdata!=NULL) {
          GlobalUnlock(hdata);
          GlobalFree(hdata); };
        break;
      case TWRC_FAILURE:               // Bitmap invalid, stop scanning
        dsmentry(&appid,&source,
          DG_CONTROL,DAT_PENDINGXFERS,MSG_RESET,(TW_MEMREF)&pending);
        nextimage=0;
      break;
    };
  };
  return 0;
};

// Disables and closes TWAIN interface. Returns 0 on success and -1 on error.
int CloseTWAINinterface(void) {
  TW_UINT16 result;
  TW_USERINTERFACE interf;
  if (twainstate==5) {
    // Disable source.
    interf.ShowUI=0;
    interf.ModalUI=0;
    interf.hParent=(TW_HANDLE)hwmain;
    result=dsmentry(&appid,&source,
      DG_CONTROL,DAT_USERINTERFACE,MSG_DISABLEDS,&interf);
    if (result!=TWRC_SUCCESS) return -1;
    twainstate=4; };
  if (twainstate==4) {
    // Close source.
    result=dsmentry(&appid,NULL,
      DG_CONTROL,DAT_IDENTITY,MSG_CLOSEDS,(TW_MEMREF)&source);
    if (result!=TWRC_SUCCESS) return -1;
    twainstate=3; };
  return 0;
};

// Detaches from the TWAIN. Returns 0 on success and -1 on error.
int CloseTWAINmanager(void) {
  TW_UINT16 result;
  CloseTWAINinterface();               // Reduces state to at least 3
  if (twainstate==3) {
    result=dsmentry(&appid,NULL,
      DG_CONTROL,DAT_PARENT,MSG_OPENDSM,(TW_MEMREF)&hwmain);
    if (result!=TWRC_SUCCESS) return -1;
    twainstate=2;
  };
  return 0;
};

int PassmessagetoTWAIN(MSG *msg) {
  TW_UINT16 result;
  TW_EVENT twevent;
  if (dsmentry==NULL)                  
    return TWRC_NOTDSEVENT;            // Oops, TWAIN not initialized!
  twevent.pEvent=(TW_MEMREF)msg;
  twevent.TWMessage=MSG_NULL;
  result=dsmentry(&appid,&source,
    DG_CONTROL,DAT_EVENT,MSG_PROCESSEVENT,&twevent);
  // Check for message from the TWAIN interface.
  switch (twevent.TWMessage) {
    case MSG_XFERREADY:
      GetpicturefromTWAIN();
      break;
    case MSG_CLOSEDSREQ:
      CloseTWAINinterface();
      Updatebuttons();
      break;
    case MSG_CLOSEDSOK:
      CloseTWAINinterface();
      Updatebuttons();
      break;
    default: break;
  };
  return result;
};

int LoadTWAINlibrary(void) {
  htwaindll=LoadLibrary("twain_32.dll");
  if (htwaindll!=NULL) {
    dsmentry=(DSMENTRYPROC)GetProcAddress(htwaindll,"DSM_Entry");
    if (dsmentry==NULL) {
      FreeLibrary(htwaindll);
      htwaindll=NULL; }
    else {
      twainstate=2;
    }; }
  else
    dsmentry=NULL;
  return (dsmentry!=NULL);
};

void CloseTWAINlibrary(void) {
  if (htwaindll!=NULL) FreeLibrary(htwaindll);
  htwaindll=NULL;
  dsmentry=NULL;
};
