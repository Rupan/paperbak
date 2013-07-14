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


////////////////////////////////////////////////////////////////////////////////
//////////////////////////// FILE-RELATED ROUTINES /////////////////////////////

// Converts file date and time into the text according to system defaults and
// places into the string s of length n. Returns number of characters in s.
int Filetimetotext(FILETIME *fttime,char *s,int n) {
  int l;
  SYSTEMTIME sttime;
  FileTimeToSystemTime(fttime,&sttime);
  l=GetDateFormat(LOCALE_USER_DEFAULT,DATE_SHORTDATE,&sttime,NULL,s,n);
  s[l-1]=' ';                          // Yuck, that's Windows
  l+=GetTimeFormat(LOCALE_USER_DEFAULT,TIME_NOSECONDS,&sttime,NULL,s+l,n-l);
  return l;
};

// Asks user to enter the name of the input data file (infile). Returns 0 on
// success and -1 on error or if user pressed Cancel.
int Selectinfile(void) {
  OPENFILENAME ofn;
  memset(&ofn,0,sizeof(ofn));
  // Correct Windows bu... feature.
  ofn.lStructSize=min(OPENFILENAME_SIZE_VERSION_400,sizeof(ofn));
  ofn.hwndOwner=hwmain;
  ofn.hInstance=hinst;
  ofn.lpstrFilter="Any file (*.*)\0*.*\0\0";
  ofn.lpstrFile=infile;
  ofn.nMaxFile=sizeof(infile);
  ofn.lpstrTitle="Open file to print";
  ofn.Flags=OFN_FILEMUSTEXIST|OFN_LONGNAMES;
  return (GetOpenFileName(&ofn)==0?-1:0);
};

// Asks user to enter the name of the output data file (outfile). Returns 0 on
// success and -1 on error or if user pressed Cancel.
int Selectoutfile(char defname[64]) {
  char s[65],drv[MAXDRIVE],dir[MAXDIR],fil[MAXFILE],ext[MAXEXT];
  OPENFILENAME ofn;
  // Split old path into components.
  fnsplit(outfile,drv,dir,fil,ext);
  // Substitute name and extention by those from the bitmap.
  strncpy(s,defname,64); s[64]='\0';
  fnsplit(s,NULL,NULL,fil,ext);
  fnmerge(outfile,drv,dir,fil,ext);
  // Call standard Save File dialog.
  memset(&ofn,0,sizeof(ofn));
  ofn.lStructSize=min(OPENFILENAME_SIZE_VERSION_400,sizeof(ofn));
  ofn.hwndOwner=hwmain;
  ofn.hInstance=hinst;
  ofn.lpstrFilter="Any file (*.*)\0*.*\0\0";
  ofn.lpstrFile=outfile;
  ofn.nMaxFile=sizeof(outfile);
  ofn.lpstrTitle="Save file as";
  ofn.Flags=OFN_LONGNAMES;
  return (GetSaveFileName(&ofn)==0?-1:0);
};

// Asks user to enter the name of the input bitmap file (inbmp). Returns 0 on
// success and -1 on error or if user pressed Cancel.
int Selectinbmp(void) {
  OPENFILENAME ofn;
  memset(&ofn,0,sizeof(ofn));
  ofn.lStructSize=min(OPENFILENAME_SIZE_VERSION_400,sizeof(ofn));
  ofn.hwndOwner=hwmain;
  ofn.hInstance=hinst;
  ofn.lpstrFilter="Bitmap file (*.bmp)\0*.bmp\0Any file (*.*)\0*.*\0\0";
  ofn.lpstrFile=inbmp;
  ofn.nMaxFile=sizeof(inbmp);
  ofn.lpstrTitle="Open bitmap";
  ofn.Flags=OFN_FILEMUSTEXIST|OFN_LONGNAMES;
  ofn.lpstrDefExt="bmp";
  return (GetOpenFileName(&ofn)==0?-1:0);
};

// Asks user to enter the name of the output bitmap file (outbmp). Returns 0 on
// success and -1 on error or if user pressed Cancel.
int Selectoutbmp(void) {
  char drv[MAXDRIVE],dir[MAXDIR],fil[MAXFILE],ext[MAXEXT];
  OPENFILENAME ofn;
  // Split old path into components.
  fnsplit(outbmp,drv,dir,fil,ext);
  // Substitute bitmap name by that of input file and set extention to .bmp.
  fnsplit(infile,NULL,NULL,fil,NULL);
  if (ext[0]=='\0') strcpy(ext,".bmp");
  fnmerge(outbmp,drv,dir,fil,ext);
  // Call standard Save File dialog.
  memset(&ofn,0,sizeof(ofn));
  ofn.lStructSize=min(OPENFILENAME_SIZE_VERSION_400,sizeof(ofn));
  ofn.hwndOwner=hwmain;
  ofn.hInstance=hinst;
  ofn.lpstrFilter="Bitmap file (*.bmp)\0*.bmp\0Any file (*.*)\0*.*\0\0";
  ofn.lpstrFile=outbmp;
  ofn.nMaxFile=sizeof(outbmp);
  ofn.lpstrTitle="Save bitmap as";
  ofn.Flags=OFN_LONGNAMES;
  ofn.lpstrDefExt="bmp";
  return (GetSaveFileName(&ofn)==0?-1:0);
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// FILE QUEUE //////////////////////////////////

#define NQUEUE          128            // Max number of pending bitmaps

typedef struct t_queue {               // Element of queue of pending tasks
  char           path[MAXPATH];        // File to process
  int            isbitmap;             // 0: print, 1: decode bitmap
} t_queue;

static t_queue   queue[NQUEUE];        // Queue of pending tasks
static int       nqueue;               // Actual number of pending tasks

// Removes all files from the queue of pending tasks.
void Clearqueue(void) {
  nqueue=0;
};

// Returns number of free slots in the queue of pending tasks.
int Getqueuefreecount(void) {
  return NQUEUE-nqueue;
};

// Adds task to queue (isbitmap=0: print data, 1: decode bitmap). Returns 0 if
// file is posted and -1 if queue is full.
int Addfiletoqueue(char *path,int isbitmap) {
  if (nqueue>=NQUEUE) {
    Reporterror("Input queue full");
    return -1; }
  else {
    strcpy(queue[nqueue].path,path);
    queue[nqueue].isbitmap=isbitmap;
    nqueue++;
    return 0;
  };
};

// Gets file from the queue of pending tasks. If queue is empty, returns -1.
// Otherwise, gets file name and returns requested action (0: print data, 1:
// decode bitmap).
int Getfilefromqueue(char *path) {
  int isbitmap;
  if (nqueue==0)
    return -1;                         // Input queue is empty
  strcpy(path,queue[0].path);
  isbitmap=queue[0].isbitmap;
  // YES, this solution is not professional. But it works. Period.
  nqueue--;
  if (nqueue>0)
    memmove(queue+0,queue+1,nqueue*sizeof(t_queue));
  return isbitmap;
};

