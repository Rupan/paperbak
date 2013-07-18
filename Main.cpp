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

#define MAINPROG
#include "paperbak.h"
#include "resource.h"

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// DATA FORMAT //////////////////////////////////
//
// Data is kept in matrix 32x32 points. It consists of:
//
//   4-byte address (combined with redundancy count) or special marker;
//   90-byte compressed and encrypted data;
//   2-byte CRC of address and data (CCITT version);
//   32-byte Reed-Solomon error correction code of the previous 96 bytes.
//
// Top left point is the LSB of the low byte of address. Second point in the
// topmost row is the second bit, etc. I have selected horizontal orientation
// of bytes because jet printers in draft mode may shift rows in X. This may
// lead to the loss of 4 bytes, but not 32 at once.
//
// Even rows are XORed with 0x55555555 and odd with 0xAAAAAAAA to prevent long
// lines or columns of zeros or ones in uncompressed data.
//
// For each ngroup=redundancy data blocks, program creates one artificial block
// filled with data which is the XOR of ngroup blocks, additionally XORed with
// 0xFF. Blocks within the group are distributed through the sheet into the
// different rows and columns, thus increasing the probability of data recovery,
// even if some parts are completely missing. Redundancy blocks contain ngroup
// in the most significant 4 bits.

// TODO: manual restoration of damaged blocks.


HMENU            hmenu;                // Handle of main menu

// Window function of About dialog box.
int CALLBACK Aboutdlgproc(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  char s[1024];
  switch (msg) {
    case WM_INITDIALOG:
      sprintf(s,"\nPaperBack v%i.%02i\n"
        "Copyright © 2007 Oleh Yuschuk\n"
        "Parts copyright © 2013 Michael Mohr\n\n"
        "----- THIS SOFTWARE IS FREE -----\n"
        "Released under GNU Public License (GPL 3+)\n"
        "Full sources available\n\n"
        "Reed-Solomon ECC:\n"
        "Copyright © 2002 Phil Karn (GPL)\n\n"
        "Bzip2 data compression:\n"
        "Copyright © 1996-2010 Julian R. Seward (see sources)\n\n"
        "AES and SHA code:\n"
        "Copyright © 1998-2010, Brian Gladman (3-clause BSD)",
        VERSIONHI,VERSIONLO);
      SetDlgItemText(hw,ABOUT_TEXT,s);
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wp)==ABOUT_OK || LOWORD(wp)==ABOUT_CANCEL)
        EndDialog(hw,0);
      break;
    case WM_SYSCOMMAND:
      if ((wp & 0xFFF0)==SC_CLOSE)
        EndDialog(hw,0);
      break;
    default: break;
  };
  return 0;
};

// Displays About dialog box.
void About(void) {
  DialogBox(hinst,"DIALOG_ABOUT",hwmain,(DLGPROC)Aboutdlgproc);
};

// Window function of Options dialog box.
int CALLBACK Optionsdlgproc(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int i,sel,maxres,dpiset,dotset,comprset,redset;
  char s[TEXTLEN];
  static int resfactor[8] = { 2, 3, 4, 5, 6, 8, 10, 15 };
  switch (msg) {
    case WM_INITDIALOG:
      // Initialize printer resolution list.
      maxres=min(resx,resy);
      if (maxres==0) maxres=600;
      if (dpi==0) dpi=200;
      dpiset=0;
      SendMessage(GetDlgItem(hw,OPT_DENSITY),CB_SETEXTENDEDUI,1,0);
      for (i=0; i<8; i++) {
        sprintf(s,"%i dpi",maxres/resfactor[i]);
        SendMessage(GetDlgItem(hw,OPT_DENSITY),CB_ADDSTRING,0,(LPARAM)s);
        if (dpiset==0 && (i==7 || dpi>=maxres/resfactor[i])) {
          SendMessage(GetDlgItem(hw,OPT_DENSITY),CB_SELECTSTRING,0,(LPARAM)s);
          dpi=maxres/resfactor[i];
          dpiset=1;
        };
      };
      // Initialize dot size list.
      if (dotpercent==0) dotpercent=70;
      dotset=0;
      SendMessage(GetDlgItem(hw,OPT_DOTSIZE),CB_SETEXTENDEDUI,1,0);
      for (i=0; i<6; i++) {
        sprintf(s,"%i %%",100-i*10);
        SendMessage(GetDlgItem(hw,OPT_DOTSIZE),CB_ADDSTRING,0,(LPARAM)s);
        if (dotset==0 && (i==5 || dotpercent>=100-i*10)) {
          SendMessage(GetDlgItem(hw,OPT_DOTSIZE),CB_SELECTSTRING,0,(LPARAM)s);
          dotpercent=100-i*10;
          dotset=1;
        };
      };
      // Initialize compression list.
      comprset=0;
      SendMessage(GetDlgItem(hw,OPT_COMPRESS),CB_SETEXTENDEDUI,1,0);
      for (i=0; i<3; i++) {
        if (i==0) strcpy(s,"None");
        else if (i==1) strcpy(s,"Fast");
        else strcpy(s,"Maximal");
        SendMessage(GetDlgItem(hw,OPT_COMPRESS),CB_ADDSTRING,0,(LPARAM)s);
        if (comprset==0 && (i==compression || i==2)) {
          SendMessage(GetDlgItem(hw,OPT_COMPRESS),CB_SELECTSTRING,0,(LPARAM)s);
          compression=i;
          comprset=1;
        };
      };
      // Initialize redundancy list.
      if (redundancy==0) redundancy=NGROUP;
      redset=0;
      SendMessage(GetDlgItem(hw,OPT_REDUND),CB_SETEXTENDEDUI,1,0);
      for (i=NGROUPMIN; i<=NGROUPMAX; i++) {
        sprintf(s,"1 : %i",i);
        SendMessage(GetDlgItem(hw,OPT_REDUND),CB_ADDSTRING,0,(LPARAM)s);
        if (redset==0 && (i==redundancy || i==NGROUPMAX)) {
          SendMessage(GetDlgItem(hw,OPT_REDUND),CB_SELECTSTRING,0,(LPARAM)s);
          redundancy=i;
          redset=1;
        };
      };
      // Initialize header/footer checkbox.
      CheckDlgButton(hw,OPT_HEADER,(printheader?BST_CHECKED:BST_UNCHECKED));
      // Initialize border checkbox.
      CheckDlgButton(hw,OPT_BORDER,(printborder?BST_CHECKED:BST_UNCHECKED));
      // Initialize autosave checkbox.
      CheckDlgButton(hw,OPT_AUTOSAVE,(autosave?BST_CHECKED:BST_UNCHECKED));
      // Initialize best quality checkbox.
      CheckDlgButton(hw,OPT_HIQ,(bestquality?BST_CHECKED:BST_UNCHECKED));
      // Initialize encryption checkbox.
      CheckDlgButton(hw,OPT_ENCRYPT,(encryption?BST_CHECKED:BST_UNCHECKED));
      // Initialize open text checkbox.
      CheckDlgButton(hw,OPT_OPENTEXT,(opentext?BST_CHECKED:BST_UNCHECKED));
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wp)==OPT_OK) {
        // Get resolution.
        sel=SendMessage(GetDlgItem(hw,OPT_DENSITY),CB_GETCURSEL,0,0);
        maxres=min(resx,resy);
        if (maxres==0) maxres=600;
        dpi=maxres/resfactor[sel];
        // Get dot size.
        sel=SendMessage(GetDlgItem(hw,OPT_DOTSIZE),CB_GETCURSEL,0,0);
        dotpercent=100-sel*10;
        // Get compression.
        compression=SendMessage(GetDlgItem(hw,OPT_COMPRESS),CB_GETCURSEL,0,0);
        // Get redundancy.
        sel=SendMessage(GetDlgItem(hw,OPT_REDUND),CB_GETCURSEL,0,0);
        redundancy=NGROUPMIN+sel;
        // Get header/footer option.
        printheader=(IsDlgButtonChecked(hw,OPT_HEADER)==BST_CHECKED);
        // Get border option.
        printborder=(IsDlgButtonChecked(hw,OPT_BORDER)==BST_CHECKED);
        // Get autosave option.
        autosave=(IsDlgButtonChecked(hw,OPT_AUTOSAVE)==BST_CHECKED);
        // Get best quality option.
        bestquality=(IsDlgButtonChecked(hw,OPT_HIQ)==BST_CHECKED);
        // Get encryption option.
        encryption=(IsDlgButtonChecked(hw,OPT_ENCRYPT)==BST_CHECKED);
        // Get open text option.
        opentext=(IsDlgButtonChecked(hw,OPT_OPENTEXT)==BST_CHECKED);
        EndDialog(hw,0); }
      else if (LOWORD(wp)==OPT_CANCEL)
        EndDialog(hw,0);
      break;
    case WM_SYSCOMMAND:
      if ((wp & 0xFFF0)==SC_CLOSE)
        EndDialog(hw,0);
      break;
    default: break;
  };
  return 0;
};

// Displays Options dialog box.
void Options(void) {
  DialogBox(hinst,"DIALOG_OPTIONS",hwmain,(DLGPROC)Optionsdlgproc);
};

// Window function of Confirm password dialog box.
int CALLBACK Confirmdlgproc(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int i;
  char passcopy[PASSLEN];
  switch (msg) {
    case WM_INITDIALOG:
      SendMessage(GetDlgItem(hw,PAS_ENTER),EM_SETLIMITTEXT,PASSLEN-1,0);
      SendMessage(GetDlgItem(hw,PAS_CONFIRM),EM_SETLIMITTEXT,PASSLEN-1,0);
      if (opentext==0) {
        SendMessage(GetDlgItem(hw,PAS_ENTER),EM_SETPASSWORDCHAR,'*',0);
        SendMessage(GetDlgItem(hw,PAS_CONFIRM),EM_SETPASSWORDCHAR,'*',0); };
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wp)==PAS_OK) {
        GetDlgItemText(hw,PAS_ENTER,password,PASSLEN);
        GetDlgItemText(hw,PAS_CONFIRM,passcopy,PASSLEN);
        if (strcmp(password,passcopy)!=0) {
          SetDlgItemText(hw,PAS_TEXT,"Different passwords. Please try again:");
          SetDlgItemText(hw,PAS_ENTER,"");
          SetDlgItemText(hw,PAS_CONFIRM,"");
          break; };
        // Clear passcopy and edit controls. We don't need to leave password
        // in memory or in the swap file. Question: what to do with Edit undo
        // feature?
        for (i=0; i<PASSLEN-1; i++) passcopy[i]=(char)('A'+i%32);
        passcopy[PASSLEN-1]=0;
        SetDlgItemText(hw,PAS_ENTER,passcopy);
        SetDlgItemText(hw,PAS_CONFIRM,passcopy);
        EndDialog(hw,0); }
      else if (LOWORD(wp)==PAS_CANCEL) {
        for (i=0; i<PASSLEN-1; i++) passcopy[i]=(char)('A'+i%32);
        passcopy[PASSLEN-1]=0;
        SetDlgItemText(hw,PAS_ENTER,passcopy);
        SetDlgItemText(hw,PAS_CONFIRM,passcopy);
        strcpy(password,passcopy);
        EndDialog(hw,-1); };
      break;
    case WM_SYSCOMMAND:
      if ((wp & 0xFFF0)==SC_CLOSE) {
        for (i=0; i<PASSLEN-1; i++) passcopy[i]=(char)('A'+i%32);
        passcopy[PASSLEN-1]=0;
        SetDlgItemText(hw,PAS_ENTER,passcopy);
        SetDlgItemText(hw,PAS_CONFIRM,passcopy);
        strcpy(password,passcopy);
        EndDialog(hw,-1); };
      break;
    default: break;
  };
  return 0;
};

// Displays Enter password dialog with confirmation line. Returns 0 on success
// (password is in global variable pasword) or non-zero if user pressed Cancel.
int Confirmpassword(void) {
  return DialogBox(hinst,"DIALOG_CONFIRM",hwmain,(DLGPROC)Confirmdlgproc);
};

// Window function of Password dialog box.
int CALLBACK Passworddlgproc(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int i;
  char paserase[PASSLEN];
  switch (msg) {
    case WM_INITDIALOG:
      SendMessage(GetDlgItem(hw,PAS_ENTER),EM_SETLIMITTEXT,PASSLEN-1,0);
      if (opentext==0)
        SendMessage(GetDlgItem(hw,PAS_ENTER),EM_SETPASSWORDCHAR,'*',0);
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wp)==PAS_OK) {
        GetDlgItemText(hw,PAS_ENTER,password,PASSLEN);
        // Clear edit control. We don't need to leave password in memory or in
        // the swap file. Question: what to do with Edit undo feature?
        for (i=0; i<PASSLEN-1; i++) paserase[i]=(char)('A'+i%32);
        paserase[PASSLEN-1]=0;
        SetDlgItemText(hw,PAS_ENTER,paserase);
        EndDialog(hw,0); }
      else if (LOWORD(wp)==PAS_CANCEL) {
        for (i=0; i<PASSLEN-1; i++) password[i]=(char)('A'+i%32);
        password[PASSLEN-1]=0;
        SetDlgItemText(hw,PAS_ENTER,password);
        EndDialog(hw,-1); };
      break;
    case WM_SYSCOMMAND:
      if ((wp & 0xFFF0)==SC_CLOSE) {
        for (i=0; i<PASSLEN-1; i++) password[i]=(char)('A'+i%32);
        password[PASSLEN-1]=0;
        SetDlgItemText(hw,PAS_ENTER,password);
        EndDialog(hw,-1); };
      break;
    default: break;
  };
  return 0;
};

// Displays Password dialog without confirmation line. Returns 0 on success
// (password is in global variable pasword) or non-zero if user pressed Cancel.
int Getpassword(void) {
  return DialogBox(hinst,"DIALOG_PASSWORD",hwmain,(DLGPROC)Passworddlgproc);
};

// Windows function of main PaperBack window.
LRESULT CALLBACK Mainwp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int i,n;
  char path[MAXPATH],ext[MAXEXT];
  HDC dc;
  PAINTSTRUCT ps;
  switch (msg) {
    case WM_CREATE:
      DragAcceptFiles(hwmain,TRUE);    // This window accepts drag-and-drop
      break;
    case WM_DESTROY:
      DragAcceptFiles(hwmain,FALSE);   // Close drag-and-drop
      hwmain=NULL;                     // Window is no longer available
      PostQuitMessage(0); break;
    case WM_CLOSE:
      CloseTWAINmanager();
      DestroyWindow(hw);
      break;
    case WM_COMMAND:
      if (HIWORD(wp)==0) {             // Message is from menu
        switch (LOWORD(wp)) {
          case M_FILE_OPEN:            // Open bitmap file
            Decodebitmap(NULL);
            break;
          case M_FILE_SAVEBMP:         // Save file to bitmap
            if (Selectinfile()==0 &&
              Selectoutbmp()==0)
              Printfile(infile,outbmp);
            break;
          case M_FILE_SELECT:          // Select source
            SelectTWAINsource();
            break;
          case M_FILE_ACQUIRE:         // Acquire image(s)
            OpenTWAINinterface();
            Updatebuttons();
            break;
          case M_FILE_PAGE:            // Page setup
            Setuppage();
            break;
          case M_FILE_PRINT:           // Print data
            if (Selectinfile()==0)
              Printfile(infile,NULL);
            break;
          case M_FILE_EXIT:            // Close PaperBack
            DestroyWindow(hw);
            break;
          case M_HELP_ABOUT:           // Display About window
            About();
            break;
          default: break;
        };
      };
      break;
    case WM_DROPFILES:
      n=DragQueryFile((HDROP)wp,0xFFFFFFFF,path,MAXPATH);
      for (i=0; i<n; i++) {
        if (DragQueryFile((HDROP)wp,i,path,MAXPATH)>0) {
          fnsplit(path,NULL,NULL,NULL,ext);
          if (stricmp(ext,".bmp")==0)
            // Default action by bitmaps: decode.
            Addfiletoqueue(path,1);
          else
            // Default action by all other files: print.
            Addfiletoqueue(path,0);
          ;
        };
      };
      DragFinish((HDROP)wp);
      break;
    case WM_PAINT:
      dc=BeginPaint(hw,&ps);
      // Hey, do we have anything to do here? Background is already OK.
      EndPaint(hw,&ps);
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0L;
};

// Main PaperBack program.
int PASCAL WinMain(HINSTANCE hi,HINSTANCE hprev,LPSTR cmdline,int show) {
  int dx,dy,isbitmap;
  char path[MAXPATH],drv[MAXDRIVE],dir[MAXDIR],fil[MAXFILE];
  char s[TEXTLEN],inifile[MAXPATH];
  WNDCLASS wc;
  MSG msg;
  TW_UINT16 result;
  // Save the instance.
  hinst=hi;
  // Very Advanced Controls like Magnificious Supertabs are used here.
  InitCommonControls();
  // Create custom GDI objects.
  graybrush=CreateSolidBrush(GetSysColor(COLOR_3DFACE));
  // Create name of initialization file.
  GetModuleFileName(NULL,path,MAXPATH);
  fnsplit(path,drv,dir,fil,NULL);
  fnmerge(inifile,drv,dir,fil,".ini");
  // Get settings from the initialization file.
  GetPrivateProfileString("Settings","Infile","",infile,MAXPATH,inifile);
  GetPrivateProfileString("Settings","Inbmp","",inbmp,MAXPATH,inifile);
  GetPrivateProfileString("Settings","Outfile","",outfile,MAXPATH,inifile);
  GetPrivateProfileString("Settings","Outbmp","",outbmp,MAXPATH,inifile);
  dpi=GetPrivateProfileInt("Settings","Raster",200,inifile);
  dotpercent=GetPrivateProfileInt("Settings","Dot size",70,inifile);
  compression=GetPrivateProfileInt("Settings","Compression",2,inifile);
  redundancy=GetPrivateProfileInt("Settings","Redundancy",5,inifile);
  printheader=GetPrivateProfileInt("Settings","Header and footer",1,inifile);
  printborder=GetPrivateProfileInt("Settings","Border",0,inifile);
  autosave=GetPrivateProfileInt("Settings","Autosave",1,inifile);
  bestquality=GetPrivateProfileInt("Settings","Best quality",1,inifile);
  encryption=GetPrivateProfileInt("Settings","Encryption",0,inifile);
  opentext=GetPrivateProfileInt("Settings","Open password",0,inifile);
  // Get printer's page size.
  marginunits=GetPrivateProfileInt("Settings","Margin units",0,inifile);
  marginleft=GetPrivateProfileInt("Settings","Margin left",1000,inifile);
  marginright=GetPrivateProfileInt("Settings","Margin right",400,inifile);
  margintop=GetPrivateProfileInt("Settings","Margin top",400,inifile);
  marginbottom=GetPrivateProfileInt("Settings","Margin bottom",500,inifile);
  // Register class of main window.
  wc.style=CS_OWNDC;
  wc.lpfnWndProc=Mainwp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=LoadIcon(hinst,"ICON_MAIN");
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=graybrush;
  wc.lpszMenuName=NULL;
  wc.lpszClassName=MAINCLASS;
  if (!RegisterClass(&wc))
    return 1;
  // Create main window. It will accept drag-and-drop files.
  dx=min(GetSystemMetrics(SM_CXSCREEN),MAINDX);
  dy=min(GetSystemMetrics(SM_CYSCREEN),MAINDY);
  hmenu=LoadMenu(hinst,"MENU_MAIN");
  hwmain=CreateWindowEx(
    WS_EX_ACCEPTFILES,
    MAINCLASS,"PaperBack",
    WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|
      WS_CLIPCHILDREN|WS_VISIBLE,
    CW_USEDEFAULT,CW_USEDEFAULT,dx,dy,
    NULL,hmenu,hinst,NULL);
  if (hwmain==NULL)
    return 1;
  // Create controls in the window.
  if (Createcontrols()!=0)
    return 1;
  // Load TWAIN interface. Failure is not critical - we still can print data!
  twainstate=1;
  if (LoadTWAINlibrary()==0) {
    EnableMenuItem(hmenu,M_FILE_SELECT,MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(hmenu,M_FILE_ACQUIRE,MF_BYCOMMAND|MF_GRAYED); };
  Updatebuttons();
  // Initialize printer settings.
  Initializeprintsettings();
  // And now, the main Windows loop.
  while (1) {
    // Check if there are some unprocessed Windows messages and process them at
    // once.
    while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
      // Pass all messages to the TWAIN interface.
      result=TWRC_NOTDSEVENT;
      if (twainstate>=5)
        result=(TW_UINT16)PassmessagetoTWAIN(&msg);
      // If message was not processed by TWAIN, it must be processed by
      // the application.
      if (result==TWRC_NOTDSEVENT) {
        // Pass keyboard messages first to block display.
        if (msg.message!=WM_KEYDOWN || Changeblockselection(msg.wParam)==1) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        };
      };
    };
    if (msg.message==WM_QUIT) break;
    // Execute next steps of data decoding and file printing.
    if (procdata.step!=0)
      Nextdataprocessingstep(&procdata);
    else if (printdata.step!=0)
      Nextdataprintingstep(&printdata);
    // Some TWAIN drivers require very frequent calls when scanning, don't
    // sleep.
    if (twainstate>=5)
      continue;
    // Check whether new printing or decoding task can be executed.
    if (procdata.step==0 && printdata.step==0) {
      isbitmap=Getfilefromqueue(path);
      if (isbitmap==0) Printfile(path,NULL);
      else if (isbitmap==1) Decodebitmap(path); };
    // If we have nothing to do, be kind to other applications.
    if (twainstate<5 && procdata.step==0 && printdata.step==0 &&
      GetQueueStatus(QS_ALLINPUT)==0
    ) {
      Sleep(1);                        // Reduces CPU time practically to zero
    };
  };
  // Save settings to the initialization file.
  WritePrivateProfileString("Settings","Infile",infile,inifile);
  WritePrivateProfileString("Settings","Inbmp",inbmp,inifile);
  WritePrivateProfileString("Settings","Outfile",outfile,inifile);
  WritePrivateProfileString("Settings","Outbmp",outbmp,inifile);
  sprintf(s,"%i",dpi);
    WritePrivateProfileString("Settings","Raster",s,inifile);
  sprintf(s,"%i",dotpercent);
    WritePrivateProfileString("Settings","Dot size",s,inifile);
  sprintf(s,"%i",compression);
    WritePrivateProfileString("Settings","Compression",s,inifile);
  sprintf(s,"%i",redundancy);
    WritePrivateProfileString("Settings","Redundancy",s,inifile);
  sprintf(s,"%i",printheader);
    WritePrivateProfileString("Settings","Header and footer",s,inifile);
  sprintf(s,"%i",printborder);
    WritePrivateProfileString("Settings","Border",s,inifile);
  sprintf(s,"%i",autosave);
    WritePrivateProfileString("Settings","Autosave",s,inifile);
  sprintf(s,"%i",bestquality);
    WritePrivateProfileString("Settings","Best quality",s,inifile);
  sprintf(s,"%i",encryption);
    WritePrivateProfileString("Settings","Encryption",s,inifile);
  sprintf(s,"%i",opentext);
    WritePrivateProfileString("Settings","Open password",s,inifile);
  // Save printer's page size.
  if (pagesetup.Flags & PSD_INTHOUSANDTHSOFINCHES) marginunits=1;
  else if (pagesetup.Flags & PSD_INHUNDREDTHSOFMILLIMETERS) marginunits=2;
  else marginunits=0;
  if (marginunits) {
    sprintf(s,"%i",marginunits);
      WritePrivateProfileString("Settings","Margin units",s,inifile);
    sprintf(s,"%i",pagesetup.rtMargin.left);
      WritePrivateProfileString("Settings","Margin left",s,inifile);
    sprintf(s,"%i",pagesetup.rtMargin.right);
      WritePrivateProfileString("Settings","Margin right",s,inifile);
    sprintf(s,"%i",pagesetup.rtMargin.top);
      WritePrivateProfileString("Settings","Margin top",s,inifile);
    sprintf(s,"%i",pagesetup.rtMargin.bottom);
      WritePrivateProfileString("Settings","Margin bottom",s,inifile);
    ;
  };
  // Clear password. It's a bad idea to keep password in the swap file after
  // shutdown.
  memset(password,0,sizeof(password));
  // Clean up.
  Freeprocdata(&procdata);
  Stopprinting(&printdata);
  CloseTWAINmanager();
  CloseTWAINlibrary();
  Closeprintsettings();
  // Destroy objects.
  DeleteObject(graybrush);
  return 0;
};

