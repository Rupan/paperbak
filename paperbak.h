////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                          THIS SOFTWARE IS FREE!                            //
//                                                                            //
// This program is free software; you can redistribute it and/or modify it    //
// under the terms of the GNU General Public License as published by the Free //
// Software Foundation; either version 2 of the License, or (at your option)  //
// any later version. This program is distributed in the hope that it will be //
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of     //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General  //
// Public License (http://www.fsf.org/copyleft/gpl.html) for more details.    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "BZLIB\bzlib.h"

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// GENERAL DEFINITIONS //////////////////////////////

#ifdef MAINPROG
  #define unique                       // Define MAINPROG in single C unit!
#else
  #define unique extern
#endif

#define VERSIONHI      1               // Major version
#define VERSIONLO      10              // Minor version

#define MAINDX         800             // Max width of the main window, pixels
#define MAINDY         600             // Max height of the main window, pixels

#define TEXTLEN        256             // Maximal length of strings
#define PASSLEN        33              // Maximal length of password, incl. 0

#define AESKEYLEN      24              // AES key length in bytes (16, 24, or 32)

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

unique HINSTANCE hinst;                // Application's instance
unique HWND      hwmain;               // Handle of the main window


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// DATA PROPERTIES ////////////////////////////////

// Don't change the definitions below! Program may crash if any modified!
#define NDOT           32              // Block X and Y size, dots
#define NDATA          90              // Number of data bytes in a block
#define MAXSIZE        0x0FFFFF80      // Maximal (theoretical) length of file
#define SUPERBLOCK     0xFFFFFFFF      // Address of superblock

#define NGROUP         5               // For NGROUP blocks (1..15), 1 recovery
#define NGROUPMIN      2
#define NGROUPMAX      10

typedef struct t_data {                // Block on paper
  ulong          addr;                 // Offset of the block or special code
  uchar          data[NDATA];          // Useful data
  ushort         crc;                  // Cyclic redundancy of addr and data
  uchar          ecc[32];              // Reed-Solomon's error correction code
} t_data;

#if sizeof(t_data)!=128
  #error t_data: Invalid data alignment
#endif

#define PBM_COMPRESSED 0x01            // Paper backup is compressed
#define PBM_ENCRYPTED  0x02            // Paper backup is encrypted

typedef struct t_superdata {           // Identification block on paper
  ulong          addr;                 // Expecting SUPERBLOCK
  ulong          datasize;             // Size of (compressed) data
  ulong          pagesize;             // Size of (compressed) data on page
  ulong          origsize;             // Size of original (uncompressed) data
  uchar          mode;                 // Special mode bits, set of PBM_xxx
  uchar          attributes;           // Basic file attributes
  ushort         page;                 // Actual page (1-based)
  FILETIME       modified;             // Time of last file modification
  ushort         filecrc;              // CRC of compressed decrypted file
  char           name[64];             // File name - may have all 64 chars
  ushort         crc;                  // Cyclic redundancy of previous fields
  uchar          ecc[32];              // Reed-Solomon's error correction code
} t_superdata;

#if sizeof(t_superdata)!=sizeof(t_data)
  #error t_superdata: Invalid data alignment
#endif

typedef struct t_block {               // Block in memory
  ulong          addr;                 // Offset of the block
  ulong          recsize;              // 0 for data, or length of covered data
  uchar          data[NDATA];          // Useful data
} t_block;

typedef struct t_superblock {          // Identification block in memory
  ulong          addr;                 // Expecting SUPERBLOCK
  ulong          datasize;             // Size of (compressed) data
  ulong          pagesize;             // Size of (compressed) data on page
  ulong          origsize;             // Size of original (uncompressed) data
  ulong          mode;                 // Special mode bits, set of PBM_xxx
  ushort         page;                 // Actual page (1-based)
  FILETIME       modified;             // Time of last file modification
  ulong          attributes;           // Basic file attributes
  ulong          filecrc;              // 16-bit CRC of decrypted packed file
  char           name[64];             // File name - may have all 64 chars
  int            ngroup;               // Actual NGROUP on the page
} t_superblock;


////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// CRC //////////////////////////////////////

ushort Crc16(uchar *data,int length);


////////////////////////////////////////////////////////////////////////////////
////////////////////////// REED-SOLOMON ECC ROUTINES ///////////////////////////

void   Encode8(uchar *data,uchar *parity,int pad);
int    Decode8(uchar *data, int *eras_pos, int no_eras,int pad);


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// PRINTER ////////////////////////////////////

#define PACKLEN        65536           // Length of data read buffer 64 K

typedef struct t_printdata {           // Print control structure
  int            step;                 // Next data printing step (0 - idle)
  char           infile[MAXPATH];      // Name of input file
  char           outbmp[MAXPATH];      // Name of output bitmap (empty: paper)
  HANDLE         hfile;                // Handle of input file
  FILETIME       modified;             // Time of last file modification
  ulong          attributes;           // File attributes
  ulong          origsize;             // Original file size, bytes
  ulong          readsize;             // Amount of data read from file so far
  ulong          datasize;             // Size of (compressed) data
  ulong          alignedsize;          // Data size aligned to next 16 bytes
  ulong          pagesize;             // Size of (compressed) data on page
  int            compression;          // 0: none, 1: fast, 2: maximal
  int            encryption;           // 0: none, 1: encrypt
  int            printheader;          // Print header and footer
  int            printborder;          // Print border around bitmap
  int            redundancy;           // Redundancy
  uchar          *buf;                 // Buffer for compressed file
  ulong          bufsize;              // Size of buf, bytes
  uchar          *readbuf;             // Read buffer, PACKLEN bytes long
  bz_stream      bzstream;             // Compression control structure
  int            bufcrc;               // 16-bit CRC of (packed) data in buf
  t_superdata    superdata;            // Identification block on paper
  HDC            dc;                   // Printer device context
  int            frompage;             // First page to print (0-based)
  int            topage;               // Last page (0-based, inclusive)
  int            ppix;                 // Printer X resolution, pixels per inch
  int            ppiy;                 // Printer Y resolution, pixels per inch
  int            width;                // Page width, pixels
  int            height;               // Page height, pixels
  HFONT          hfont6;               // Font 1/6 inch high
  HFONT          hfont10;              // Font 1/10 inch high
  int            extratop;             // Height of title line, pixels
  int            extrabottom;          // Height of info line, pixels
  int            black;                // Palette index of dots colour
  int            borderleft;           // Left page border, pixels
  int            borderright;          // Right page border, pixels
  int            bordertop;            // Top page border, pixels
  int            borderbottom;         // Bottom page border, pixels
  int            dx,dy;                // Distance between dots, pixels
  int            px,py;                // Dot size, pixels
  int            nx,ny;                // Grid dimensions, blocks
  int            border;               // Border around the data grid, pixels
  HBITMAP        hbmp;                 // Handle of memory bitmap
  uchar          *dibbits;             // Pointer to DIB bits
  uchar          *drawbits;            // Pointer to file bitmap bits
  uchar          bmi[sizeof(BITMAPINFO)+256*sizeof(RGBQUAD)]; // Bitmap info
  int            startdoc;             // Print job started
} t_printdata;


unique PAGESETUPDLG pagesetup;         // Structure with printer page settings
unique int       resx,resy;            // Printer resolution, dpi (may be 0!)
unique t_printdata printdata;          // Print control structure

void   Initializeprintsettings(void);
void   Closeprintsettings(void);
void   Setuppage(void);
void   Stopprinting(t_printdata *print);
void   Nextdataprintingstep(t_printdata *print);
void   Printfile(char *path,char *bmp);


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// DECODER ////////////////////////////////////

#define M_BEST         0x00000001      // Search for best possible quality

typedef struct t_procdata {            // Descriptor of processed data
  int            step;                 // Next data processing step (0 - idle)
  int            mode;                 // Set of M_xxx
  uchar          *data;                // Pointer to bitmap
  int            sizex;                // X bitmap size, pixels
  int            sizey;                // Y bitmap size, pixels
  int            gridxmin,gridxmax;    // Rought X grid limits, pixels
  int            gridymin,gridymax;    // Rought Y grid limits, pixels
  int            searchx0,searchx1;    // X grid search limits, pixels
  int            searchy0,searchy1;    // Y grid search limits, pixels
  int            cmean;                // Mean grid intensity (0..255)
  int            cmin,cmax;            // Minimal and maximal grid intensity
  float          sharpfactor;          // Estimated sharpness correction factor
  float          xpeak;                // Base X grid line, pixels
  float          xstep;                // X grid step, pixels
  float          xangle;               // X tilt, radians
  float          ypeak;                // Base Y grid line, pixels
  float          ystep;                // Y grid step, pixels
  float          yangle;               // Y tilt, radians
  float          blockborder;          // Relative width of border around block
  int            bufdx,bufdy;          // Dimensions of block buffers, pixels
  uchar          *buf1,*buf2;          // Rotated and sharpened block
  int            *bufx,*bufy;          // Block grid data finders
  uchar          *unsharp;             // Either buf1 or buf2
  uchar          *sharp;               // Either buf1 or buf2
  float          blockxpeak,blockypeak;// Exact block position in unsharp
  float          blockxstep,blockystep;// Exact block dimensions in unsharp
  int            nposx;                // Number of blocks to scan in X
  int            nposy;                // Number of blocks to scan in X
  int            posx,posy;            // Next block to scan
  t_data         uncorrected;          // Data before ECC for block display
  t_block        *blocklist;           // List of blocks recognized on page
  t_superblock   superblock;           // Page header
  int            maxdotsize;           // Maximal size of the data dot, pixels
  int            orientation;          // Data orientation (-1: unknown)
  int            ngood;                // Page statistics: good blocks
  int            nbad;                 // Page statistics: bad blocks
  int            nsuper;               // Page statistics: good superblocks
  int            nrestored;            // Page statistics: restored bytes
} t_procdata;

unique int       orientation;          // Orientation of bitmap (-1: unknown)
unique t_procdata procdata;            // Descriptor of processed data

void   Nextdataprocessingstep(t_procdata *pdata);
void   Freeprocdata(t_procdata *pdata);
void   Startbitmapdecoding(t_procdata *pdata,uchar *data,int sizex,int sizey);
void   Stopbitmapdecoding(t_procdata *pdata);
int    Decodeblock(t_procdata *pdata,int posx,int posy,t_data *result);


////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// FILE PROCESSOR ////////////////////////////////

#define NFILE          5               // Max number of simultaneous files

typedef struct t_fproc {               // Descriptor of processed file
  int            busy;                 // In work
  // General file data.
  char           name[64];             // File name - may have all 64 chars
  FILETIME       modified;             // Time of last file modification
  ulong          attributes;           // Basic file attrributes
  ulong          datasize;             // Size of (compressed) data
  ulong          pagesize;             // Size of (compressed) data on page
  ulong          origsize;             // Size of original (uncompressed) data
  ulong          mode;                 // Special mode bits, set of PBM_xxx
  int            npages;               // Total number of pages
  ulong          filecrc;              // 16-bit CRC of decrypted packed file
  // Properties of currently processed page.
  int            page;                 // Currently processed page
  int            ngroup;               // Actual NGROUP on the page
  ulong          minpageaddr;          // Minimal address of block on page
  ulong          maxpageaddr;          // Maximal address of block on page
  // Gathered data.
  int            nblock;               // Total number of data blocks
  int            ndata;                // Number of decoded blocks so far
  uchar          *datavalid;           // 0:data invalid, 1:valid, 2:recovery
  uchar          *data;                // Gathered data
  // Statistics.
  int            goodblocks;           // Total number of good blocks read
  int            badblocks;            // Total number of unreadable blocks
  ulong          restoredbytes;        // Total number of bytes restored by ECC
  int            recoveredblocks;      // Total number of recovered blocks
  int            rempages[8];          // 1-based list of remaining pages
} t_fproc;

unique t_fproc   fproc[NFILE];         // Processed files

void   Closefproc(int slot);
int    Startnextpage(t_superblock *superblock);
int    Addblock(t_block *block,int slot);
int    Finishpage(int slot,int ngood,int nbad,ulong nrestored);
int    Saverestoredfile(int slot,int force);


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// SCANNER ////////////////////////////////////

unique int       twainstate;           // According to TWAIN specifications

int    Decodebitmap(char *path);
int    SelectTWAINsource(void);
int    OpenTWAINmanager(void);
int    OpenTWAINinterface(void);
int    CloseTWAINmanager(void);
int    PassmessagetoTWAIN(MSG *msg);
int    LoadTWAINlibrary(void);
void   CloseTWAINlibrary(void);


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// CONTROLS ///////////////////////////////////

#define MAINCLASS      "P2DMAIN"       // Name of the class of main window
#define DATAFRAMECLASS "P2DATAFRAME"   // Name of the class of data frame
#define DISPLAYCLASS   "P2DDISPL"      // Name of the class of display window
#define PROGRESSCLASS  "P2DPROGRESS"   // Name of the class of progress window
#define BUTTONFRAME    "P2BTNFRAME"    // Name of the class of button frame
#define INFOFRAMECLASS "P2INFOFRAME"   // Name of the class of info frame
#define INFOBTNCLASS   "P2INFOBTNS"    // Name of the class of info btn holder
#define BLOCKSELCLASS  "P2BLOCKSEL"    // Name of the class of block selector

#define DELTA          3               // Distance between panels in main window
#define BUTTONDY       32              // Height of the buttons

// Display modes.
#define DISP_QUALITY   0               // Display quality map
#define DISP_BLOCK     1               // Display block image

unique HBRUSH    graybrush;            // Button face brush (usually gray)

void   Reporterror(char *text);
void   Message(char *text,int percent);

void   Updatebuttons(void);
void   Setdisplaymode(int mode);
void   Initqualitymap(int nx,int ny);
void   Addblocktomap(int x,int y,int nrestored);
void   Displayblockimage(t_procdata *pdata,int posx,int posy,
         int answer,t_data *result);
int    Changeblockselection(WPARAM wp);
void   Updatefileinfo(int slot,t_fproc *fproc);
int    Createcontrols(void);


////////////////////////////////////////////////////////////////////////////////
////////////////////////////// SERVICE FUNCTIONS ///////////////////////////////

int    Filetimetotext(FILETIME *fttime,char *s,int n);
int    Selectinfile(void);                     
int    Selectoutfile(char defname[64]);
int    Selectinbmp(void);
int    Selectoutbmp(void);

void   Clearqueue(void);
int    Getqueuefreecount(void);
int    Addfiletoqueue(char *path,int isbitmap);
int    Getfilefromqueue(char *path);


////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// USER INTERFACE ////////////////////////////////

unique char      infile[MAXPATH];      // Last selected file to read
unique char      outbmp[MAXPATH];      // Last selected bitmap to save
unique char      inbmp[MAXPATH];       // Last selected bitmap to read
unique char      outfile[MAXPATH];     // Last selected data file to save

unique char      password[PASSLEN];    // Encryption password

unique int       dpi;                  // Dot raster, dots per inch
unique int       dotpercent;           // Dot size, percent of dpi
unique int       compression;          // 0: none, 1: fast, 2: maximal
unique int       redundancy;           // Redundancy (NGROUPMIN..NGROUPMAX)
unique int       printheader;          // Print header and footer
unique int       printborder;          // Border around bitmap
unique int       autosave;             // Autosave completed files
unique int       bestquality;          // Determine best quality
unique int       encryption;           // Encrypt data before printing
unique int       opentext;             // Enter passwords in open text

unique int       marginunits;          // 0:undef, 1:inches, 2:millimeters
unique int       marginleft;           // Left printer page margin
unique int       marginright;          // Right printer page margin
unique int       margintop;            // Top printer page margin
unique int       marginbottom;         // Bottom printer page margin

void   Options(void);
int    Confirmpassword();
int    Getpassword(void);

