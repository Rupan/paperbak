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

static HFONT     hfont20;              // 20-point bold font
static int       font20height;         // Real height of hfont20

 
////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// BUTTONS ////////////////////////////////////

#define BTN_PRINT      1000            // Identifier of Print button
#define BTN_SCAN       1001            // Identifier of Scan button
#define BTN_READ       1002            // Identifier of Open bitmap button
#define BTN_STOP       1003            // Identifier of Stop button
#define BTN_PAGE       1004            // Identifier of Page setup button
#define BTN_OPTIONS    1005            // Identifier of Options button
#define BTN_CLOSE      1006            // Identifier of Close button

static HWND      hbuttonframe;         // Frame that owns the buttons
static HWND      hprint;               // Print button
static HWND      hscan;                // Scan button
static HWND      hopen;                // Open bitmap button
static HWND      hstop;                // Interrupt current operation
static HWND      hpage;                // Page setup button
static HWND      hoptions;             // Options button
static HWND      hclose;               // Close button

// Windows function of button frame.
LRESULT CALLBACK Buttonframewp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  RECT rc;
  PAINTSTRUCT ps;
  HDC dc;
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;                        // Erasing is not necessary
    case WM_PAINT:
      GetClientRect(hw,&rc);
      dc=BeginPaint(hw,&ps);
      FillRect(dc,&rc,graybrush);
      EndPaint(hw,&ps);
      break;
    case WM_COMMAND:
      if (HIWORD(wp)!=BN_CLICKED)
        break;
      switch (LOWORD(wp)) {
        case BTN_PRINT:                // Print button pressed
          if (Selectinfile()==0)
            Printfile(infile,NULL);
          break;
        case BTN_SCAN:                 // Scan button pressed
          OpenTWAINinterface();
          Updatebuttons();
          break;
        case BTN_READ:                 // Open bitmap button pressed
          Decodebitmap(NULL);
          break;
        case BTN_STOP:                 // Stop button pressed
          Stopbitmapdecoding(&procdata);
          Stopprinting(&printdata);
          Clearqueue();
          Updatebuttons();
          Message("Processing interrupted",0);
          break;
        case BTN_PAGE:                 // Page setup button pressed
          Setuppage();
          break;
        case BTN_OPTIONS:              // Options button pressed
          Options();
          break;
        case BTN_CLOSE:                // Close button pressed
          PostMessage(hwmain,WM_CLOSE,0,0);
        break;
      };
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0L;
};

// Enables or disables buttons according to the processing mode. Button "Close"
// is always enabled.
void Updatebuttons(void) {
  if (procdata.step!=0 || printdata.step!=0) {
    EnableWindow(hprint,0);
    EnableWindow(hscan,0);
    EnableWindow(hopen,0);
    EnableWindow(hstop,1);
    EnableWindow(hpage,0);
    EnableWindow(hoptions,0); }
  else {
    EnableWindow(hprint,1);
    if (twainstate<=1) {
      EnableWindow(hscan,0);
      EnableWindow(hopen,1); }
    else if (twainstate<=3) {
      EnableWindow(hscan,1);
      EnableWindow(hopen,1); }
    else {
      EnableWindow(hscan,0);
      EnableWindow(hopen,0); };
    EnableWindow(hstop,0);
    EnableWindow(hpage,1);
    EnableWindow(hoptions,1);
  };
  // Now update the whole main window.
  RedrawWindow(hwmain,NULL,NULL,RDW_UPDATENOW|RDW_ALLCHILDREN);
  SetFocus(hwmain);
};

// Creates button frame and fills it with buttons. Frame owns buttons and so
// receives their BN_CLICKED notifications.
void Createbuttons(RECT *rc) {
  int x,dx;
  WNDCLASS wc;
  // Register window class of button frame.
  wc.style=CS_OWNDC;
  wc.lpfnWndProc=Buttonframewp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=graybrush;
  wc.lpszMenuName=NULL;
  wc.lpszClassName=BUTTONFRAME;
  if (!RegisterClass(&wc))
    return;
  // Create button frame.
  hbuttonframe=CreateWindow(
    BUTTONFRAME,"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
    rc->left,rc->top+2,rc->right-rc->left,BUTTONDY,
    hwmain,NULL,hinst,NULL);
  if (hbuttonframe==NULL)
    return;
  rc->top+=(BUTTONDY+DELTA+4);
  // Calculate button width. Button frame contains 7 buttons.
  dx=(rc->right-rc->left-(7-1)*DELTA)/7;
  x=0;
  // Create Print button.
  hprint=CreateWindow("BUTTON","Print",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,0,dx,BUTTONDY,
    hbuttonframe,(HMENU)BTN_PRINT,hinst,NULL);
  SendMessage(hprint,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  x+=dx+DELTA;
  // Create Scan button.
  hscan=CreateWindow("BUTTON","Scan",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,0,dx,BUTTONDY,
    hbuttonframe,(HMENU)BTN_SCAN,hinst,NULL);
  SendMessage(hscan,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  x+=dx+DELTA;
  // Create Open bitmap button.
  hopen=CreateWindow("BUTTON","Open bitmap",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,0,dx,BUTTONDY,
    hbuttonframe,(HMENU)BTN_READ,hinst,NULL);
  SendMessage(hopen,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  x+=dx+DELTA;
  // Create Stop button.
  hstop=CreateWindow("BUTTON","Stop",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,0,dx,BUTTONDY,
    hbuttonframe,(HMENU)BTN_STOP,hinst,NULL);
  SendMessage(hstop,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  x+=dx+DELTA;
  // Create Page setup button.
  hpage=CreateWindow("BUTTON","Page setup",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,0,dx,BUTTONDY,
    hbuttonframe,(HMENU)BTN_PAGE,hinst,NULL);
  SendMessage(hpage,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  x+=dx+DELTA;
  // Create Options button.
  hoptions=CreateWindow("BUTTON","Options",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,0,dx,BUTTONDY,
    hbuttonframe,(HMENU)BTN_OPTIONS,hinst,NULL);
  SendMessage(hoptions,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  x+=dx+DELTA;
  // Create Close button. It takes all the remaining space.
  hclose=CreateWindow("BUTTON","Close",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,0,rc->right-rc->left-x,BUTTONDY,
    hbuttonframe,(HMENU)BTN_CLOSE,hinst,NULL);
  SendMessage(hclose,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  // Enable or disable buttons according to the actual state.
  Updatebuttons();
};


////////////////////////////////////////////////////////////////////////////////
/////////////////////////// PROGRESS AND MESSAGE BAR ///////////////////////////

static HWND      hprogress;            // Progress and message bar

static char      message[TEXTLEN];     // Text to display
static int       showpercent;          // Percentage (error if negative)

// Windows function of progress and message bar.
LRESULT CALLBACK Progresswp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int n;
  char s[TEXTLEN+32];
  RECT rblack,rwhite;
  PAINTSTRUCT ps;
  HDC dc;
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;                        // Erasing is not necessary
    case WM_PAINT:
      // Prepare text and calculate its length.
      n=sprintf(s,"%s",message);
      if (showpercent>0) n+=sprintf(s+n," - %i %%",showpercent);
      // Prepare left and right rectangles.
      GetClientRect(hw,&rblack);
      rwhite=rblack;
      if (showpercent<=0)
        rblack.right=rblack.left;
      else
        rblack.right=rwhite.left=(rblack.right-rblack.left)*showpercent/100;
      dc=BeginPaint(hw,&ps);
      SelectObject(dc,hfont20);
      SetTextAlign(dc,TA_TOP|TA_CENTER);
      // Draw left rectangle - white text on black background.
      SetTextColor(dc,RGB(255,255,255));
      SetBkColor(dc,RGB(0,0,0));
      ExtTextOut(dc,(rblack.left+rwhite.right)/2,1,
        ETO_CLIPPED|ETO_OPAQUE,&rblack,s,n,NULL);
      // Draw second rectangle - black or lightred text on white or gray
      // background.
      if (showpercent<0)
        SetTextColor(dc,RGB(255,48,48));
      else
        SetTextColor(dc,RGB(0,0,0));
      if (showpercent<=0)
        SetBkColor(dc,GetSysColor(COLOR_BTNFACE));
      else
        SetBkColor(dc,RGB(255,255,255));
      ExtTextOut(dc,(rblack.left+rwhite.right)/2,1,
        ETO_CLIPPED|ETO_OPAQUE,&rwhite,s,n,NULL);
      EndPaint(hw,&ps);
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0L;
};

// Creates progress and message bar and places it at the top of the specified
// rectangle.
HWND Createprogressbar(RECT *rc) {
  int dy;
  WNDCLASS wc;
  // Register window class of progress bar.
  wc.style=CS_OWNDC;
  wc.lpfnWndProc=Progresswp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszMenuName=NULL;
  wc.lpszClassName=PROGRESSCLASS;
  if (!RegisterClass(&wc))
    return NULL;
  // Calculate required height of progress bar.  
  dy=font20height+2*GetSystemMetrics(SM_CXFRAME);
  // Set initial text to display in progress bar.
  sprintf(message,"PaperBack v%i.%02i",VERSIONHI,VERSIONLO);
  showpercent=0;
  // Create bar.
  hprogress=CreateWindowEx(
    WS_EX_CLIENTEDGE,PROGRESSCLASS,"",
    WS_CHILD|WS_VISIBLE,
    rc->left,rc->top,rc->right-rc->left,dy,
    hwmain,NULL,hinst,NULL);
  if (hprogress!=NULL)
    rc->top+=(dy+DELTA);
  return hprogress;
};

// Displays error message.
void Reporterror(char *text) {
  if (hprogress==NULL || text==NULL) return;
  strncpy(message,text,TEXTLEN-1);
  message[TEXTLEN-1]='\0';
  showpercent=-1;
  RedrawWindow(hprogress,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
};

// Displays progress. Call with percent=0 to display plain message.
void Message(char *text,int percent) {
  if (hprogress==NULL || text==NULL) return;
  if (percent==showpercent && strncmp(message,text,TEXTLEN-1)==0)
    return;                            // Unchanged appearance
  strncpy(message,text,TEXTLEN-1);
  message[TEXTLEN-1]='\0';
  showpercent=max(0,min(percent,100));
  RedrawWindow(hprogress,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
};

// Destroys progress bar and associated resources. Not absolutely necessary
// because Windows cleans up when application closes.
void Destroyprogressbar(void) {
  DestroyWindow(hprogress); hprogress=NULL;
  UnregisterClass(PROGRESSCLASS,hinst);
};


////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// DATA DISPLAY /////////////////////////////////

#define BSEL_GRID      1000            // Identifier of Show grid box
#define BSEL_BAD       1001            // Identifier of Show bad dots box
#define BSEL_NONE      1002            // Identifier of No overlay box
#define BSEL_POS       1003            // Identifier of coordinate window
#define BSEL_LEFT      1004            // Identifier of L button
#define BSEL_RIGHT     1005            // Identifier of R button
#define BSEL_UP        1006            // Identifier of U button
#define BSEL_DOWN      1007            // Identifier of D button

static HWND      hdataframe;           // Frame that owns the data tab
static HWND      hdatatab;             // Data tab control
static HWND      hdisplay;             // Window that displays decoded data

static int       displaymode;          // Display mode, one of DISP_xxx

// Viewing parameters used in DISP_QUALITY mode.
static HBRUSH    htone[17];            // Quality brushes
static HBRUSH    hbad;                 // Brush for unreadable blocks
static uchar     *qualitymap;          // Quality map (255: empty, 17: bad)
static int       mapnx;                // Map size in X, blocks
static int       mapny;                // Map size in Y, blocks
static int       mapscale;             // Map scale, pixels per block
static RECT      maprect;              // Page rectangle, pixels

// Viewing parameters used in DISP_BLOCK mode.
static HDC       blockdc;              // Memory DC that keeps block image
static HBITMAP   blockbmp;             // Bitmap that keeps block image
static uchar     *blockbits;           // Pointer to block image bitmap
static int       blockdx;              // X size of block image bitmap
static int       blockdy;              // Y size of block image bitmap
static int       blockindex;           // Index of displayed block

// Controls in block selection window.
static HWND      hblocksel;            // Block selection window
static HWND      hshowgrid;            // Always show full grid
static HWND      hshowbad;             // Show only frames around bad pixels
static HWND      hshownone;            // Show neither grid nor frames
static HWND      hpos;                 // Window displaying block coordinates
static HWND      hleft;                // Move left selector
static HWND      hright;               // Move right selector
static HWND      hup;                  // Move up selector
static HWND      hdown;                // Move down selector
static int       blockselx;            // X index of selected block
static int       blocksely;            // Y index of selected block
static int       blockdotmode;         // Overlay mode, one of BSEL_xxx

// Windows function of data frame window. This window is fully covered by the
// data tab, so drawing is not necessary.
LRESULT CALLBACK Dataframewp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int i;
  switch (msg) {
    case WM_NOTIFY:
      if (((NMHDR *)lp)->hwndFrom!=hdatatab ||
        ((NMHDR *)lp)->code!=TCN_SELCHANGE) break;
      i=SendMessage(hdatatab,TCM_GETCURSEL,0,0);
      Setdisplaymode(i);
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0;
};

// Windows function of data display window.
LRESULT CALLBACK Displaywp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int i,j,answer;
  uchar *pm;
  t_data result;
  RECT rc;
  HDC dc;
  PAINTSTRUCT ps;
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;                        // Erasing is not necessary
    case WM_LBUTTONDBLCLK:
      // Doubleclick in block display mode changes to quality map.
      if (displaymode==DISP_BLOCK) {
        Setdisplaymode(DISP_QUALITY);
        RedrawWindow(hw,NULL,NULL,RDW_UPDATENOW);
        break; };
      // Doubleclick on block in quality map mode displays block.
      if (displaymode!=DISP_QUALITY || mapnx<=0 || mapny<=0 || mapscale<=0)
        break;
      i=(LOWORD(lp)-maprect.left)/mapscale;
      j=(HIWORD(lp)-maprect.top)/mapscale;
      if (i>=0 && i<mapnx && j>=0 && j<mapny) {
        Setdisplaymode(DISP_BLOCK);
        answer=Decodeblock(&procdata,i,j,&result);
        Displayblockimage(&procdata,i,j,answer,&result);
        RedrawWindow(hw,NULL,NULL,RDW_UPDATENOW); };
      break;
    case WM_PAINT:
      GetClientRect(hw,&rc);
      dc=BeginPaint(hw,&ps);
      if (displaymode==DISP_QUALITY && qualitymap!=NULL && mapnx>0 && mapny>0) {
        // Draw quality map.
        FillRect(dc,&rc,graybrush);
        FillRect(dc,&maprect,(HBRUSH)GetStockObject(WHITE_BRUSH));
        rc.bottom=maprect.top;
        for (j=0; j<mapny; j++) {
          rc.top=rc.bottom; rc.bottom+=mapscale;
          rc.right=maprect.left;
          pm=qualitymap+j*mapnx;
          for (i=0; i<mapnx; i++,pm++) {
            rc.left=rc.right; rc.right+=mapscale;
            if (*pm==0xFF) continue;   // Block is not yet processed
            if (*pm<=16) FillRect(dc,&rc,htone[*pm]);
            else FillRect(dc,&rc,hbad);
          };
        }; }
      else if (displaymode==DISP_BLOCK && blockdc!=NULL && blockbits!=NULL) {
        // Draw data block.
        BitBlt(dc,0,0,blockdx,blockdy,blockdc,0,0,SRCCOPY); }
      else
        // Nothing to draw.
        FillRect(dc,&rc,graybrush);
      EndPaint(hw,&ps);
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0L;
};

// Window function of block selection window.
LRESULT CALLBACK Blockselectionwp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int answer;
  t_data result;
  RECT rc;
  HDC dc;
  PAINTSTRUCT ps;
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;                        // Erasing is not necessary
    case WM_PAINT:
      GetClientRect(hw,&rc);
      dc=BeginPaint(hw,&ps);
      SelectObject(dc,GetStockObject(BLACK_PEN));
      MoveToEx(dc,rc.left,rc.top,NULL);
      LineTo(dc,rc.right,rc.top);
      rc.top++;
      FillRect(dc,&rc,graybrush);
      EndPaint(hw,&ps);
      break;
    case WM_COMMAND:
      if (HIWORD(wp)!=BN_CLICKED)
        break;
      switch (LOWORD(wp)) {
        case BSEL_GRID:                // Show grid radio button clicked
        case BSEL_BAD:                 // Show bad dots radio button clicked
        case BSEL_NONE:                // No overlay radio button clicked
          if (blockdotmode!=LOWORD(wp)) {
            blockdotmode=LOWORD(wp);
            CheckRadioButton(hblocksel,BSEL_GRID,BSEL_NONE,blockdotmode);
            answer=Decodeblock(&procdata,blockselx,blocksely,&result);
            Displayblockimage(&procdata,blockselx,blocksely,answer,&result);
            RedrawWindow(hdisplay,NULL,NULL,RDW_UPDATENOW); };
          break;
        case BSEL_LEFT:                // Move selection left
          Changeblockselection(VK_LEFT); break;
        case BSEL_RIGHT:               // Move selection right
          Changeblockselection(VK_RIGHT); break;
        case BSEL_UP:                  // Move selection up
          Changeblockselection(VK_UP); break;
        case BSEL_DOWN:                // Move selection down
          Changeblockselection(VK_DOWN);
        break;
      };
      SetFocus(hwmain);
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0;
};

// Creates block selection window that owns block navigation buttons, block
// indicator and grid checkbox.
static HWND Createblockselector(HWND howner) {
  RECT rcowner;
  WNDCLASS wc;
  // Register class of block selection window.
  wc.style=CS_OWNDC;
  wc.lpfnWndProc=Blockselectionwp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=NULL;
  wc.lpszMenuName=NULL;
  wc.lpszClassName=BLOCKSELCLASS;
  if (!RegisterClass(&wc))
    return NULL;
  // Create window. Note that initially this window is hidden.
  GetClientRect(howner,&rcowner);
  hblocksel=CreateWindow(BLOCKSELCLASS,"",
    WS_CHILD|WS_CLIPCHILDREN,
    0,rcowner.bottom-75,rcowner.right,75,
    howner,NULL,hinst,NULL);
  if (hblocksel==NULL)
    return NULL;
  // Create overlay selection radio buttons.
  hshowgrid=CreateWindow("BUTTON","Show grid",
    WS_CHILD|WS_VISIBLE|BS_RADIOBUTTON	,
    6,7,100,20,
    hblocksel,(HMENU)BSEL_GRID,hinst,NULL);
  SendMessage(hshowgrid,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  hshowbad=CreateWindow("BUTTON","Show bad dots",
    WS_CHILD|WS_VISIBLE|BS_RADIOBUTTON	,
    6,29,100,20,
    hblocksel,(HMENU)BSEL_BAD,hinst,NULL);
  SendMessage(hshowbad,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  hshownone=CreateWindow("BUTTON","No overlay",
    WS_CHILD|WS_VISIBLE|BS_RADIOBUTTON	,
    6,51,100,20,
    hblocksel,(HMENU)BSEL_NONE,hinst,NULL);
  SendMessage(hshownone,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  CheckRadioButton(hblocksel,BSEL_GRID,BSEL_NONE,BSEL_BAD);
  blockdotmode=BSEL_BAD;
  // Create position display.
  hpos=CreateWindowEx(WS_EX_CLIENTEDGE,"STATIC","",
    WS_CHILD|WS_VISIBLE|WS_BORDER|SS_CENTER,
    (rcowner.right-130)/2,14,130,48,
    hblocksel,(HMENU)BSEL_POS,hinst,NULL);
  SendMessage(hpos,WM_SETFONT,(WPARAM)hfont20,0);
  // Create cursor buttons.
  hleft=CreateWindow("BUTTON","L",
    WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
    rcowner.right-110,23,32,32,
    hblocksel,(HMENU)BSEL_LEFT,hinst,NULL);
  SendMessage(hleft,WM_SETFONT,(WPARAM)hfont20,0);
  hright=CreateWindow("BUTTON","R",
    WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
    rcowner.right-36,23,32,32,
    hblocksel,(HMENU)BSEL_RIGHT,hinst,NULL);
  SendMessage(hright,WM_SETFONT,(WPARAM)hfont20,0);
  hup=CreateWindow("BUTTON","U",
    WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
    rcowner.right-73,5,32,32,
    hblocksel,(HMENU)BSEL_UP,hinst,NULL);
  SendMessage(hup,WM_SETFONT,(WPARAM)hfont20,0);
  hdown=CreateWindow("BUTTON","D",
    WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
    rcowner.right-73,41,32,32,
    hblocksel,(HMENU)BSEL_DOWN,hinst,NULL);
  SendMessage(hdown,WM_SETFONT,(WPARAM)hfont20,0);
  return hblocksel;
};

// Shows or hides block selector.
static void Showblockselector(int show) {
  ShowWindow(hblocksel,show?SW_RESTORE:SW_HIDE);
};

// Creates tab with display window (includes quality map and block view) and
// places it in the right half of the specified rectangle.
HWND Createdisplay(RECT *rc) {
  int i,leftpos;
  uchar buf[sizeof(BITMAPINFO)+256*sizeof(RGBQUAD)];
  BITMAPINFO *pbmi;
  RECT rcd;
  WNDCLASS wc;
  TCITEM titem;
  HDC dc;
  // Register class of display frame window.
  wc.style=CS_OWNDC;
  wc.lpfnWndProc=Dataframewp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=NULL;
  wc.lpszMenuName=NULL;
  wc.lpszClassName=DATAFRAMECLASS;
  if (!RegisterClass(&wc))
    return NULL;
  // Register class of display window.
  wc.style=CS_OWNDC|CS_DBLCLKS;
  wc.lpfnWndProc=Displaywp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=graybrush;
  wc.lpszMenuName=NULL;
  wc.lpszClassName=DISPLAYCLASS;
  if (!RegisterClass(&wc))
    return NULL;
  // Create data frame - dummy window with sole purpose to intercept tab
  // notifications within this source file.
  leftpos=(rc->right+rc->left+DELTA)/2;
  hdataframe=CreateWindow(DATAFRAMECLASS,"",
    WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
    leftpos,rc->top,rc->right-leftpos,rc->bottom-rc->top,
    hwmain,NULL,hinst,NULL);
  if (hdataframe==NULL) return NULL;
  // Create tab wrapper.
  hdatatab=CreateWindow(WC_TABCONTROL,"",
    WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_TABSTOP|TCS_TABS,
    0,0,rc->right-leftpos,rc->bottom-rc->top,
    hdataframe,NULL,hinst,NULL);
  if (hdatatab==NULL) {
    DestroyWindow(hdataframe);
    return NULL; };
  SendMessage(hdatatab,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  // Add tabs for quality map and block view.
  memset(&titem,0,sizeof(titem));
  titem.mask=TCIF_TEXT;
  titem.pszText="Quality map";
  SendMessage(hdatatab,TCM_INSERTITEM,DISP_QUALITY,(LPARAM)&titem);
  titem.pszText="Blocks";
  SendMessage(hdatatab,TCM_INSERTITEM,DISP_BLOCK,(LPARAM)&titem);
  // Create brushes to display quality map. 0 errors: green; 16 errors (maximum
  // allowed by implemented version of ECC): red; unrecoverable blocks: black.
  for (i=0; i<17; i++)
    htone[i]=CreateSolidBrush(RGB(119+8*i,119+8*(16-i),64));
  hbad=(HBRUSH)GetStockObject(BLACK_BRUSH);
  // Set default display parameters.
  displaymode=DISP_QUALITY;
  mapnx=mapny=0;
  // Create display window. The window is same for both tabs, I only redraw its
  // contents.
  GetClientRect(hdatatab,&rcd);
  SendMessage(hdatatab,TCM_ADJUSTRECT,0,(LPARAM)&rcd);
  rcd.top+=2;
  hdisplay=CreateWindowEx(WS_EX_CLIENTEDGE,DISPLAYCLASS,"",
    WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
    rcd.left,rcd.top,rcd.right-rcd.left,rcd.bottom-rcd.top,
    hdatatab,NULL,hinst,NULL);
  // Create memory DC and allocate grayscale bitmap that will be used as a
  // buffer for direct block drawing.
  dc=GetDC(hdisplay);
  blockdc=CreateCompatibleDC(dc);
  if (blockdc==NULL) {
    blockdx=blockdy=0;
    blockbmp=NULL;
    blockbits=NULL; }
  else {
    blockdx=                           // Bitmap scan lines are DWORD-aligned
      (rcd.right-rcd.left) & 0xFFFFFFFC;
    blockdy=rcd.bottom-rcd.top;
    pbmi=(BITMAPINFO *)buf;
    memset(pbmi,0,sizeof(BITMAPINFOHEADER));
    pbmi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth=blockdx;
    pbmi->bmiHeader.biHeight=blockdy;
    pbmi->bmiHeader.biPlanes=1;
    pbmi->bmiHeader.biBitCount=8;
    pbmi->bmiHeader.biCompression=BI_RGB;
    pbmi->bmiHeader.biSizeImage=0;
    pbmi->bmiHeader.biClrUsed=256;
    pbmi->bmiHeader.biClrImportant=256;
    // First 128 colors are different shades of gray, from black to white.
    for (i=0; i<128; i++) {
      pbmi->bmiColors[i].rgbBlue=(uchar)(i*2);
      pbmi->bmiColors[i].rgbGreen=(uchar)(i*2);
      pbmi->bmiColors[i].rgbRed=(uchar)(i*2);
      pbmi->bmiColors[i].rgbReserved=0; };
    // Last 128 colors are gray shades, in same order, under semi-transparent
    // red. Alpha-blending for poor?..
    for (i=128; i<256; i++) {
      pbmi->bmiColors[i].rgbBlue=(uchar)(i-128);
      pbmi->bmiColors[i].rgbGreen=(uchar)(i-128);
      pbmi->bmiColors[i].rgbRed=(uchar)i;
      pbmi->bmiColors[i].rgbReserved=0; };
    blockbmp=CreateDIBSection(blockdc,pbmi,DIB_RGB_COLORS,
      (void **)&blockbits,NULL,0);
    SelectObject(blockdc,blockbmp);
    // Fill memory bitmap with 20% gray. Note that this gray is different from
    // the button gray, which is by the way not exactly gray in the default
    // scheme, but who cares?
    if (blockbits!=NULL) {
      memset(blockbits,128*80/100,blockdx*blockdy);
    };
  };
  ReleaseDC(hdisplay,dc);
  blockindex=0;
  // Create block selection window and immediately hide it, because this tool
  // is necessary only in DISP_BLOCK view.
  Createblockselector(hdisplay);
  Showblockselector(0);
  // Correct rectangle and report success.
  rc->right=leftpos-DELTA;
  return hdatatab;
};

// Sets display mode: quality map (DISP_QUALITY) or block picture (DISP_BLOCK).
void Setdisplaymode(int mode) {
  if (mode!=displaymode) {
    displaymode=mode;
    SendMessage(hdatatab,TCM_SETCURSEL,mode,0);
    Showblockselector(mode==DISP_BLOCK);
    InvalidateRect(hdisplay,NULL,FALSE);
  };
};

// Starts new quality map for page with maximal nx*ny blocks.
void Initqualitymap(int nx,int ny) {
  int x0,y0;
  RECT rc;
  // Delete previous quality map.
  if (qualitymap!=NULL)
    GlobalFree((HGLOBAL)qualitymap);
  // Allocate new quality map. Value 0xFF means non-processed element.
  if (nx<=0 || ny<=0)
    qualitymap=NULL;
  else {
    qualitymap=(uchar *)GlobalAlloc(GMEM_FIXED,nx*ny);
    if (qualitymap==NULL) nx=ny=0;
    else memset(qualitymap,0xFF,nx*ny); };
  // Calculate map geometry.
  if (nx>0 && ny>0) {
    GetClientRect(hdisplay,&rc);
    mapscale=min((rc.right-1)/nx,(rc.bottom-1)/ny);
    if (mapscale<1) mapscale=1;
    x0=(rc.right-mapscale*nx-1)/2; if (x0<0) x0=0;
    y0=(rc.bottom-mapscale*ny-1)/2; if (y0<0) y0=0;
    maprect.left=x0; maprect.right=x0+mapscale*nx+1;
    maprect.top=y0; maprect.bottom=y0+mapscale*ny+1; }
  else {
    mapscale=0;
    maprect.left=maprect.right=0;
    maprect.top=maprect.bottom=0; };
  mapnx=nx;
  mapny=ny;
  blockselx=blocksely=0;
  // Update image on the screen.
  if (displaymode==DISP_QUALITY)
    InvalidateRect(hdisplay,NULL,FALSE);
  ;
};

// Adds state of block in column x and row y to quality map.
void Addblocktomap(int x,int y,int nrestored) {
  RECT rc;
  HDC dc;
  if (qualitymap==NULL || x<0 || x>=mapnx || y<0 || y>=mapny)
    return;                            // Bad parameters or uninitialized map
  qualitymap[y*mapnx+x]=(uchar)nrestored;
  if (displaymode==DISP_QUALITY) {
    rc.left=maprect.left+x*mapscale;
    rc.right=rc.left+mapscale;
    rc.top=maprect.top+y*mapscale;
    rc.bottom=rc.top+mapscale;
    dc=GetDC(hdisplay);
    if (nrestored>=17)
      FillRect(dc,&rc,hbad);
    else
      FillRect(dc,&rc,htone[nrestored]);
    ReleaseDC(hdisplay,dc);
  };
};

// Draws block image to the memory context and, if block tab is active,
// displays it on the screen.
void Displayblockimage(t_procdata *pdata,int posx,int posy,
  int answer,t_data *result) {
  int i,j,x,x0,x1,y,y0,y1,n;
  int bufx,bufy,bufdx,bufdy,scale,orientation;
  ulong baddots[32],u;
  float xpeak,xstep,ypeak,ystep,fscale,offsetx,offsety;
  char s[128];
  uchar *buf,*pbuf,*pblock;
  if (blockdc==NULL || blockbits==NULL)
    return;                            // Block display is not initialized
  // Preset bitmap to 20% gray.
  memset(blockbits,128*80/100,blockdx*blockdy);
  // Draw block.
  if (pdata!=NULL && pdata->unsharp!=NULL && pdata->xstep>0 && pdata->ystep>0) {
    // Get frequently used variables.
    bufdx=pdata->bufdx;
    bufdy=pdata->bufdy;
    if (answer>=0) {                   // Block recognized
      xpeak=pdata->blockxpeak;
      xstep=pdata->blockxstep;
      ypeak=pdata->blockypeak;
      ystep=pdata->blockystep; }
    else {                             // Block not recognized, show center
      xstep=pdata->xstep;
      xpeak=(bufdx-xstep)/2.0;
      ystep=pdata->ystep;
      ypeak=(bufdy-ystep)/2.0; };
    buf=pdata->unsharp;
    orientation=pdata->orientation;
    // Determine scaling factor. Integer factor is much easier to handle. I use
    // mean block size. Local size differs only slightly but may cause scale
    // variations from one block to another.
    scale=min(blockdx/(pdata->xstep*1.1),blockdy/(pdata->ystep*1.1));
    if (scale<1) scale=1;
    fscale=scale;
    // Calculate offset between display amd buffer (pdata->unsharp) coordinates.
    offsetx=xpeak+(xstep-blockdx/fscale)*0.5;
    if (blockdy<blockdx)               // Draw in the middle
      offsety=ypeak+(ystep-blockdy/fscale)*0.5;
    else                               // Align to top
      offsety=ypeak+(ystep-(2*blockdy-blockdx)/fscale)*0.5;
    // Draw block.
    x0=(xpeak-xstep*0.1-offsetx)*fscale+0.5;
    if (x0<0) x0=0;
    x1=(xpeak+xstep*1.1-offsetx)*fscale+0.5;
    if (x1>blockdx) x1=blockdx;
    y0=(ypeak-ystep*0.1-offsety)*fscale+0.5;
    if (y0<0) y0=0;
    y1=(ypeak+ystep*1.1-offsety)*fscale+0.5;
    if (y1>blockdy) y1=blockdy;
    for (y=y0; y<y1; y++) {
      bufy=y/fscale+offsety;
      if (bufy<0) continue;
      if (bufy>=bufdy) break;
      pbuf=buf+bufy*bufdx;
      pblock=blockbits+y*blockdx+x0;
      for (x=x0; x<x1; x++,pblock++) {
        bufx=x/fscale+offsetx;
        if (bufx<0) continue;
        if (bufx>=bufdx) break;
        *pblock=(uchar)(pbuf[bufx]>>1);
      };
    };
    // Draw block borders.
    if (answer>=0 && blockdotmode!=BSEL_NONE) {
      x0=(xpeak-offsetx)*fscale+1.5;
      x1=(xpeak+xstep-offsetx)*fscale+1.5;
      y0=(ypeak-offsety)*fscale+1.5;
      y1=(ypeak+ystep-offsety)*fscale+1.5;
      for (x=x0; x<=x1; x++) {
        // Horizontal borders.
        if (x<0) continue;
        if (x>=blockdx) break;
        if (y0>=0) blockbits[y0*blockdx+x]|=0x80;
        if (y1<blockdy) blockbits[y1*blockdx+x]|=0x80; };
      for (y=y0; y<=y1; y++) {
        // Vertical borders.
        if (y<0) continue;
        if (y>=blockdy) break;
        if (x0>=0) blockbits[y*blockdx+x0]|=0x80;
        if (x1<blockdx) blockbits[y*blockdx+x1]|=0x80;
      };
    };
    if (blockdotmode!=BSEL_NONE &&
      (result==NULL || answer>=17 || blockdotmode==BSEL_GRID)
    ) {
      // If data can't be restored, or on special request, draw grid. First
      // vertical lines.
      y0=(ypeak+ystep/(NDOT+3.0)*1.5-offsety)*fscale+1.5;
      if (y0<0) y0=0;
      y1=(ypeak+ystep/(NDOT+3.0)*(NDOT+1.5)-offsety)*fscale+1.5;
      if (y1>=blockdy) y1=blockdy-1;
      for (i=0; i<=NDOT; i++) {
        x=(xpeak+xstep/(NDOT+3.0)*(i+1.5)-offsetx)*fscale+1.5;
        if (x<0 || x>=blockdx) continue;
        pblock=blockbits+y0*blockdx+x;
        for (y=y0; y<=y1; y++,pblock+=blockdx) *pblock|=0x80; };
      // Draw horizontal lines.
      x0=(xpeak+xstep/(NDOT+3.0)*1.5-offsetx)*fscale+1.5;
      if (x0<0) x0=0;
      x1=(xpeak+xstep/(NDOT+3.0)*(NDOT+1.5)-offsetx)*fscale+1.5;
      if (x1>=blockdx) x1=blockdx-1;
      for (j=0; j<=NDOT; j++) {
        y=(ypeak+ystep/(NDOT+3.0)*(j+1.5)-offsety)*fscale+1.5;
        if (y<0 || y>=blockdy) continue;
        pblock=blockbits+y*blockdx+x0;
        for (x=x0; x<=x1; x++) *pblock++|=0x80;
      }; }
    else if (blockdotmode!=BSEL_NONE && answer>=0) {
      // Data restored. Circumfere bad dots.
      memcpy(baddots,result,sizeof(t_data));
      for (i=0; i<32; i++)
        baddots[i]^=((ulong *)&pdata->uncorrected)[i];
      for (j=0; j<NDOT; j++) {
        for (i=0; i<NDOT; i++) {
          switch (orientation) {
            case 0: u=baddots[i] & (1<<j); break;
            case 1: u=baddots[NDOT-1-j] & (1<<i); break;
            case 2: u=baddots[NDOT-1-i] & (1<<(NDOT-1-j)); break;
            case 3: u=baddots[j] & (1<<(NDOT-1-i)); break;
            case 4: u=baddots[j] & (1<<i); break;
            case 5: u=baddots[i] & (1<<(NDOT-1-j)); break;
            case 6: u=baddots[NDOT-1-j] & (1<<(NDOT-1-i)); break;
            case 7: u=baddots[NDOT-1-i] & (1<<j); break; };
          if (u==0)
            continue;
          x0=(xpeak+xstep/(NDOT+3.0)*(i+1.5)-offsetx)*fscale+1.5;
          x1=(xpeak+xstep/(NDOT+3.0)*(i+2.5)-offsetx)*fscale+1.5;
          y0=(ypeak+ystep/(NDOT+3.0)*(j+1.5)-offsety)*fscale+1.5;
          y1=(ypeak+ystep/(NDOT+3.0)*(j+2.5)-offsety)*fscale+1.5;
          if (x0<0 || x1>=blockdx || y0<0 || y1>=blockdy)
            continue;                  // Draw only complete squares
          for (x=x0; x<=x1; x++) {
            blockbits[y0*blockdx+x]|=0x80;
            blockbits[y1*blockdx+x]|=0x80; };
          for (y=y0; y<=y1; y++) {
            blockbits[y*blockdx+x0]|=0x80;
            blockbits[y*blockdx+x1]|=0x80;
          };
        };
      };
    };
  };
  // Request repainting.
  if (displaymode==DISP_BLOCK)
    InvalidateRect(hdisplay,NULL,FALSE);
  // Save coordinates of displayed block.
  blockselx=posx;
  blocksely=posy;
  // Display coordinates and block quality.
  if (pdata==NULL)
    SetWindowText(hpos,"");
  else {
    n=sprintf(s,"(%i,%i)\n",posx,posy);
    if (answer<0)
      sprintf(s+n,"No frame");
    else if (answer==0)
      sprintf(s+n,"Good");
    else if (answer==1)
      sprintf(s+n,"1 byte bad");
    else if (answer<17)
      sprintf(s+n,"%i bytes bad",answer);
    else
      sprintf(s+n,"Unreadable");
    SetWindowText(hpos,s);
  };
};

int Changeblockselection(WPARAM wp) {
  int x,y,answer;
  t_data result;
  if (displaymode!=DISP_BLOCK)
    return 1;                          // Keystroke is not processed
  x=blockselx;
  y=blocksely;
  switch (wp) {
    case VK_LEFT: x--; break;
    case VK_RIGHT: x++; break;
    case VK_UP: y--; break;
    case VK_DOWN: y++; break;
    case VK_HOME: x=0; break;
    case VK_END: x=mapnx-1; break;
    case VK_PRIOR: y=0; break;
    case VK_NEXT: y=mapny-1; break;
    case VK_SPACE:                     // Space toggles grid (NONE - BAD)
      if (blockdotmode==BSEL_BAD) blockdotmode=BSEL_NONE;
      else blockdotmode=BSEL_BAD;
      CheckRadioButton(hblocksel,BSEL_GRID,BSEL_NONE,blockdotmode);
      break;
    default: return 1; };              // Keystroke is not processed
  if (x>=mapnx) x=mapnx-1; if (x<0) x=0;
  if (y>=mapny) y=mapny-1; if (y<0) y=0; 
  if (wp==VK_SPACE || x!=blockselx || y!=blocksely) {
    answer=Decodeblock(&procdata,x,y,&result);
    Displayblockimage(&procdata,x,y,answer,&result);
    RedrawWindow(hdisplay,NULL,NULL,RDW_UPDATENOW); };
  return 0;                            // Keystroke processed
};

// Does... er... anyway, not what its name may suggest.
void Destroydisplay(void) {
  int i;
  DestroyWindow(hdisplay); hdisplay=NULL;
  if (qualitymap!=NULL) {
    GlobalFree((HGLOBAL)qualitymap); qualitymap=NULL; };
  for (i=0; i<17; i++) DeleteObject(htone[i]);
  if (blockdc!=NULL) {
    DeleteDC(blockdc); blockdc=NULL; };
  DeleteObject(blockbmp); blockbmp=NULL;
  blockbits=NULL;
  DestroyWindow(hdatatab); hdatatab=NULL;
  DestroyWindow(hdataframe); hdataframe=NULL;
  UnregisterClass(DISPLAYCLASS,hinst);
  UnregisterClass(DATAFRAMECLASS,hinst);
};


////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// FILE DISPLAY /////////////////////////////////

#define INFO_DISCARD   1000            // Identifier of Discard button
#define INFO_SAVE      1001            // Identifier of Save button

static HWND      hinfoframe;           // Frame that owns the info tab
static HWND      hinfotab;             // Info tab control
static HWND      hdataname;            // Name of currently processed data
static HWND      horigsize;            // Original data size
static HWND      hmoddate;             // Date of modification
static HWND      hpagecount;           // Total number of pages
static HWND      hgoodcount;           // Number of recognized blocks so far
static HWND      hbadcount;            // Number of bad blocks so far
static HWND      hcorrcount;           // Number of corrected bytes
static HWND      hpagelist;            // List of pages to scan
static HWND      hinfobtns;            // Parent for Save/Discard buttons
static HWND      hdiscard;             // Discard button
static HWND      hsavedata;            // Save button

// Windows function of info frame window. This window is fully covered by the
// info tab, so drawing is not necessary.
LRESULT CALLBACK Infoframewp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int slot;
  switch (msg) {
    case WM_NOTIFY:
      if (((NMHDR *)lp)->hwndFrom==hinfotab &&
        ((NMHDR *)lp)->code==TCN_SELCHANGE
      ) {
        slot=SendMessage(hinfotab,TCM_GETCURSEL,0,0);
        Updatefileinfo(slot,fproc+slot);
      };
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0;
};

// Windows function of window that owns info buttons.
LRESULT CALLBACK Infobtnwp(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
  int slot;
  RECT rc;
  PAINTSTRUCT ps;
  HDC dc;
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;                        // Erasing is not necessary
    case WM_PAINT:
      GetClientRect(hw,&rc);
      dc=BeginPaint(hw,&ps);
      FillRect(dc,&rc,graybrush);
      EndPaint(hw,&ps);
      break;
    case WM_COMMAND:
      if (HIWORD(wp)!=BN_CLICKED)
        break;
      slot=SendMessage(hinfotab,TCM_GETCURSEL,0,0);
      switch (LOWORD(wp)) {
        case INFO_DISCARD:             // Discard button pressed
          Closefproc(slot);
          Message("",0);
          break;
        case INFO_SAVE:                // Save button pressed
          Saverestoredfile(slot,0);
        break;
      };
      break;
    default: return DefWindowProc(hw,msg,wp,lp);
  };
  return 0;
};

// Creates tab with info window and places it in the specified rectangle.
HWND Createinfo(RECT *rc) {
  int i,x,y;
  char s[TEXTLEN];
  RECT rci;
  WNDCLASS wc;
  TCITEM titem;
  HWND htemp;
  // Register class of info frame window.
  wc.style=CS_OWNDC;
  wc.lpfnWndProc=Infoframewp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=NULL;
  wc.lpszMenuName=NULL;
  wc.lpszClassName=INFOFRAMECLASS;
  if (!RegisterClass(&wc))
    return NULL;
  // Register class of window that owns info buttons.
  wc.style=CS_OWNDC;
  wc.lpfnWndProc=Infobtnwp;
  wc.cbClsExtra=wc.cbWndExtra=0;
  wc.hInstance=hinst;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=NULL;
  wc.lpszMenuName=NULL;
  wc.lpszClassName=INFOBTNCLASS;
  if (!RegisterClass(&wc))
    return NULL;
  // Create info frame - dummy window with sole purpose to intercept tab
  // notifications within this source file.
  hinfoframe=CreateWindow(INFOFRAMECLASS,"",
    WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
    rc->left,rc->top,rc->right-rc->left,rc->bottom-rc->top,
    hwmain,NULL,hinst,NULL);
  if (hinfoframe==NULL) return NULL;
  // Create info tab.
  hinfotab=CreateWindow(WC_TABCONTROL,"",
    WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_TABSTOP|TCS_TABS,
    0,0,rc->right-rc->left,rc->bottom-rc->top,
    hinfoframe,NULL,hinst,NULL);
  if (hinfotab==NULL) {
    DestroyWindow(hinfoframe); hinfoframe=NULL;
    return NULL; };
  SendMessage(hinfotab,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  // Add tabs for processed files. Initially they are anonymous.
  memset(&titem,0,sizeof(titem));
  titem.mask=TCIF_TEXT;
  for (i=0; i<NFILE; i++) {
    sprintf(s,"File %i",i+1);
    titem.pszText=s;
    SendMessage(hinfotab,TCM_INSERTITEM,i,(LPARAM)&titem); };
  // Get working rectangle of info tab window.
  GetClientRect(hinfotab,&rci);
  SendMessage(hinfotab,TCM_ADJUSTRECT,0,(LPARAM)&rci);
  // Add controls, same for all tabs. When tab selection changes, I only update
  // their contents.
  x=rci.left+10;
  y=rci.top+30;
  // File name.
  htemp=CreateWindow("STATIC","File",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,50,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  hdataname=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC"," (None)",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+50,y,rci.right-x-60,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(hdataname,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+24;
  // Original file size.
  htemp=CreateWindow("STATIC","Original size",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,140,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  horigsize=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC","",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+140,y,rci.right-x-150,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(horigsize,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+12;
  // Date of last modification.
  htemp=CreateWindow("STATIC","Modified on",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,140,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  hmoddate=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC","",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+140,y,rci.right-x-150,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(hmoddate,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+12;
  // Number of pages.
  htemp=CreateWindow("STATIC","Total pages",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,140,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  hpagecount=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC","",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+140,y,rci.right-x-150,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(hpagecount,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+24;
  // Number of good blocks so far.
  htemp=CreateWindow("STATIC","Good blocks",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,140,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  hgoodcount=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC","",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+140,y,rci.right-x-150,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(hgoodcount,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+12;
  // Number of bad blocks so far.
  htemp=CreateWindow("STATIC","Bad blocks",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,140,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  hbadcount=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC","",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+140,y,rci.right-x-150,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(hbadcount,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+12;
  // Total number of bytes corrected by ECC.
  htemp=CreateWindow("STATIC","ECC corrections",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,140,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  hcorrcount=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC","",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+140,y,rci.right-x-150,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(hcorrcount,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+24;
  // List of pages to scan.
  htemp=CreateWindow("STATIC","Pages to scan",
    WS_CHILD|WS_VISIBLE|SS_LEFT,
    x,y+1,140,22,
    hinfotab,NULL,hinst,NULL);
  SendMessage(htemp,WM_SETFONT,(WPARAM)hfont20,0);
  hpagelist=CreateWindowEx(WS_EX_STATICEDGE,
    "STATIC","",
    WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP,
    x+140,y,rci.right-x-150,font20height+4,
    hinfotab,NULL,hinst,NULL);
  SendMessage(hpagelist,WM_SETFONT,(WPARAM)hfont20,0);
  y+=font20height+24;
  // Create window that will own Discard and Save buttons. Alternatively, one
  // might subclass hinfotab.
  hinfobtns=CreateWindow(INFOBTNCLASS,"",
    WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
    x,y,rci.right-x-10,BUTTONDY,
    hinfotab,NULL,hinst,NULL);
  // Create Discard and Save button.
  hdiscard=CreateWindow("BUTTON","Discard",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    0,0,(rci.right-x-20)/2,BUTTONDY,
    hinfobtns,(HMENU)INFO_DISCARD,hinst,NULL);
  SendMessage(hdiscard,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  EnableWindow(hdiscard,0);
  hsavedata=CreateWindow("BUTTON","Save",
    WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    (rci.right-x-20)/2+10,0,(rci.right-x-20)/2,BUTTONDY,
    hinfobtns,(HMENU)INFO_SAVE,hinst,NULL);
  SendMessage(hsavedata,WM_SETFONT,(WPARAM)GetStockObject(ANSI_VAR_FONT),0);
  EnableWindow(hsavedata,0);
  return hinfotab;
};

void Updatefileinfo(int slot,t_fproc *fproc) {
  int i,n;
  char s[TEXTLEN];
  // Select specified tab.
  SendMessage(hinfotab,TCM_SETCURSEL,slot,0);
  if (fproc->name[0]=='\0') {
    // Special case: empty descriptor.
    SetWindowText(hdataname," (None)");
    SetWindowText(horigsize,"");
    SetWindowText(hmoddate,"");
    SetWindowText(hpagecount,"");
    SetWindowText(hgoodcount,"");
    SetWindowText(hbadcount,"");
    SetWindowText(hcorrcount,"");
    SetWindowText(hpagelist,"");
    EnableWindow(hdiscard,0);
    EnableWindow(hsavedata,0); }
  else {
    // File name.
    memcpy(s+1,fproc->name,64); s[0]=' '; s[65]='\0';
    SetWindowText(hdataname,s);
    // Original data size.
    sprintf(s," %u bytes",fproc->origsize);
    SetWindowText(horigsize,s);
    // Date of last modification.
    Filetimetotext(&fproc->modified,s+1,TEXTLEN-1); s[0]=' ';
    SetWindowText(hmoddate,s);
    // Total number of pages.
    sprintf(s," %u",fproc->npages);
    SetWindowText(hpagecount,s);
    // Number of recognized blocks so far.
    sprintf(s," %u",fproc->goodblocks);
    SetWindowText(hgoodcount,s);
    // Number of bad blocks so far.
    sprintf(s," %u",fproc->badblocks);
    SetWindowText(hbadcount,s);
    // Number of corrected bytes.
    sprintf(s," %u bytes",fproc->restoredbytes);
    SetWindowText(hcorrcount,s);
    // List of pages to process (up to 7).
    n=0; s[0]='\0';
    for (i=0; i<7; i++) {
      if (fproc->rempages[i]==0) break;
      if (i!=0) s[n++]=',';
      n+=sprintf(s+n," %i",fproc->rempages[i]); };
    if (i==7 && fproc->rempages[i]!=0)
      sprintf(s+n,"...");
    else if (i==0 && fproc->ndata==fproc->nblock)
      sprintf(s," Finished, press \"Save\"");
    SetWindowText(hpagelist,s);
    // Enable or disable buttons.
    EnableWindow(hdiscard,1);
    EnableWindow(hsavedata,fproc->ndata==fproc->nblock);
  };
};

// Suggest the function?
void Destroyinfo(void) {
  DestroyWindow(hinfotab); hinfotab=NULL;
  DestroyWindow(hinfoframe); hinfoframe=NULL;
  UnregisterClass(INFOFRAMECLASS,hinst);
};

// Creates and places controls on the main window. Call this function once
// after main window is created.
int Createcontrols(void) {
  RECT rcmain;
  SIZE extent;
  HDC dcmain;
  // Create 20-point straight bold font used in controls and calculate its
  // height.
  hfont20=CreateFont(
    20,0,0,0,FW_BOLD,
    FALSE,FALSE,FALSE,
    ANSI_CHARSET,OUT_TT_PRECIS,
    CLIP_DEFAULT_PRECIS,PROOF_QUALITY,
    DEFAULT_PITCH|FF_SWISS|0x04,NULL);
  if (hfont20==NULL)                   // Emergency action
    hfont20=(HFONT)GetStockObject(SYSTEM_FIXED_FONT);
  dcmain=GetDC(hwmain);
  if (GetTextExtentPoint32(dcmain,"Wgf_A",5,&extent)==0)
    font20height=20;
  else
    font20height=extent.cy+2;
  ReleaseDC(hwmain,dcmain);
  // Get size of client area of the main window and correct borders.
  GetClientRect(hwmain,&rcmain);
  rcmain.left+=DELTA;
  rcmain.right-=DELTA;
  rcmain.top+=DELTA;
  rcmain.bottom-=DELTA;
  // Create controls. Each call cuts part of the reactangle.
  Createprogressbar(&rcmain);
  Createbuttons(&rcmain);
  Createdisplay(&rcmain);
  Createinfo(&rcmain);
  return 0;
};

