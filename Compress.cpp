#include <windows.h>
#include <dir.h>
#include "print2d.h"

/* Author: David Bourgin, 1995 */

#define DBITLEN        16              // Max. dictionary size is 2**DBITLEN
#define NBITS          8               // We deal with bytes, aren't we?

typedef struct t_centry {              // Entry in encoding dictionary
  ushort         character;
  ushort         code;
  struct t_centry *lptr;
  struct t_centry *rptr;
} t_centry;

typedef struct t_dentry {              // Entry in decoding dictionary
  ushort         character;
  struct t_dentry *prev;
} t_dentry;

static t_centry  *cdict;               // Pointer to encoding dictionary
static t_dentry  *ddict;               // Pointer to decoding dictionary

static uint      ndict;                // Current number of words in dictionary
static uchar     *bout;                // Pointer to output buffer
static ulong     nbout;                // Size of output buffer
static ulong     nout;                 // Actual number of output bytes
static uchar     *bin;                 // Pointer to input buffer
static ulong     nbin;                 // Size of input buffer
static ulong     nin;                  // Actual number of input bytes
static ulong     wrvalue;
static ulong     rdvalue;
static uint      wrbits;
static uint      rdbits;
static ulong     codelen;              // Current number of bits in output code


// Write codelen bits to the output buffer.
void Writecode(uint value) {
  wrvalue=(wrvalue << codelen)|value;
  wrbits+=codelen;
  while (wrbits>=NBITS) {
    wrbits-=NBITS;
    if (nout<nbout)                    // Check if buffer is not full first
      bout[nout++]=(uchar)(wrvalue>>wrbits);
    wrvalue&=(1<<wrbits)-1;
  };
};

// Compress the input buffer of length nbufin into the output buffer of length
// nbufout. Function returns the length of the compressed data or -1 if some
// error was detected during compression.
int Compress(uchar *bufin,ulong nbufin,uchar *bufout,ulong nbufout) {
  t_centry *currnode,*newnode;
  int i;
  ushort symbol;
  if (bufin==NULL || bufout==NULL || nbufin<=0 || nbufout<=sizeof(long))
    return -1;                         // Bad input parameters
  cdict=(t_centry *)GlobalAlloc(GPTR,(1<<DBITLEN)*sizeof(t_centry));
  if (cdict==NULL) return -1;
  for (i=0; i<(1<<DBITLEN); i++)
    cdict[i].code=(ushort)i;
  for (i=0; i<(1<<NBITS); i++)
    cdict[i].character=(ushort)i;
  ndict=1<<NBITS;
  codelen=NBITS+1;
  bout=bufout; nbout=nbufout;
  nin=0; nout=0;
  wrvalue=0; wrbits=0;
  currnode=cdict+bufin[nin++];
  while (nin<nbufin) {
    symbol=bufin[nin++];
    if (currnode->lptr==NULL)
      newnode=currnode;
    else {
      newnode=currnode->lptr;
      while ((newnode->character!=symbol) && (newnode->rptr!=NULL))
        newnode=newnode->rptr;
      ;
    };
    if ((newnode!=currnode) && (newnode->character==symbol))
      currnode=newnode;
    else {
      // Insert new node into the dictionary
      Writecode(currnode->code);
      cdict[ndict].character=(ushort)symbol;
      if (currnode==newnode)
        newnode->lptr=cdict+ndict;
      else
        newnode->rptr=cdict+ndict;
      ndict++;
      if (ndict==(1u<<codelen))
        codelen++;
      if (ndict==(1<<DBITLEN)) {
        // Reinitialize the dictionary
        for (i=0; i<(1<<DBITLEN); i++) {
          cdict[i].lptr=NULL;
          cdict[i].rptr=NULL; };
        ndict=1<<NBITS;
        codelen=NBITS+1;
      };
      currnode=cdict+symbol;
    };
  };
  Writecode(currnode->code);
  if (wrbits>0 && nout<nbout)
    bout[nout++]=(uchar)(wrvalue<<(NBITS-wrbits));
  GlobalFree((HGLOBAL)cdict);
  return nout;
};

// Sends the string in the output stream given by the link of the LZW
// dictionary. Variable character contains to the end of the routine the first
// character of the string. Attention, danger of stack overflow!
void Writelink(t_dentry *chain,uint *character) {
  if (chain->prev!=NULL) {
    Writelink(chain->prev,character);
    if (nout<nbout) bout[nout++]=(uchar)(chain->character);  }
  else {
    if (nout<nbout) bout[nout++]=(uchar)(*character=chain->character);
  };
};

// Writes the string of bytes associated to current_node and returns the
// first character of this string.
uint Writestring(uint prevcode,uint currcode,uint firstchar) {
  uint character;
  if (currcode<ndict)
    Writelink(ddict+currcode,&character);
  else {
    Writelink(ddict+prevcode,&character);
    if (nout<nbout) bout[nout++]=(uchar)(firstchar); }
  return character;
};

// Get codelen bits from the input array
uint Readcode(void) {
  uint read_code;
  while (rdbits<codelen && nin<nbin) {
    rdvalue=(rdvalue<<NBITS)| bin[nin++];
    rdbits+=NBITS;  };
  rdbits-=codelen;
  read_code=rdvalue>>rdbits;
  rdvalue&=((1<<rdbits)-1);
  return read_code;
};

// Decompress the input buffer of length nbufin into the output buffer of length
// nbufout. Function returns the length of the decompressed data or -1 if some
// error was detected during decompression.
int Decompress(uchar *bufin,ulong nbufin,uchar *bufout,ulong nbufout) {
  uint i;
  uint prevcode,currcode;
  uint firstchar;
  if (bufin==NULL || bufout==NULL || nbufin<=sizeof(long) || nbufout<=0)
    return -1;                         // Bad input parameters
  ddict=(t_dentry *)GlobalAlloc(GPTR,(1<<DBITLEN)*sizeof(t_dentry));
  if (ddict==NULL) return -1;
  for (i=0; i<(1<<NBITS); i++) ddict[i].character=(ushort)i;
  ndict=1<<NBITS;
  codelen=NBITS+1;
  rdvalue=0; rdbits=0;
  bin=bufin; nbin=nbufin;
  bout=bufout; nbout=nbufout;
  nin=0; nout=0; firstchar=0;
  prevcode=currcode=Readcode();
  firstchar=Writestring(prevcode,currcode,firstchar);
  while (nin<nbin || rdbits>=codelen) {
    currcode=Readcode();
    if (currcode>ndict) {
      GlobalFree((HGLOBAL)ddict);      // Error: index outside dictionary
      return -1; };
    firstchar=Writestring(prevcode,currcode,firstchar);
    if (ndict==(1<<DBITLEN)-2) {
      for (i=1<<NBITS; i<(1<<DBITLEN); i++)
        ddict[i].prev=NULL;
      ndict=1<<NBITS;
      codelen=NBITS+1;
      if (nin<nbin || rdbits>=codelen) {
        prevcode=currcode=Readcode();
        firstchar=Writestring(prevcode,currcode,firstchar);
      }; }
    else {
      ddict[ndict].character=(ushort)firstchar;
      ddict[ndict].prev=ddict+prevcode;
      ndict++;
      if (ndict+1==(1u<<codelen)) codelen++; };
    prevcode=currcode; }
  GlobalFree((HGLOBAL)ddict);
  return nout;
};

