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


// Clears descriptor of processed file with given index.
void Closefproc(int slot) {
  if (slot<0 || slot>=NFILE)
    return;                            // Error in input data
  if (fproc[slot].datavalid!=NULL)
    GlobalFree((HGLOBAL)fproc[slot].datavalid);
  if (fproc[slot].data!=NULL)
    GlobalFree((HGLOBAL)fproc[slot].data);
  memset(fproc+slot,0,sizeof(t_fproc));
  Updatefileinfo(slot,fproc+slot);
};

// Starts new decoded page. Returns non-negative index to table of processed
// files on success or -1 on error.
int Startnextpage(t_superblock *superblock) {
  int i,slot,freeslot;
  t_fproc *pf;
  // Check whether file is already in the list of processed files. If not,
  // initialize new descriptor.
  freeslot=-1;
  for (slot=0,pf=fproc; slot<NFILE; slot++,pf++) {
    if (pf->busy==0) {                 // Empty descriptor
      if (freeslot<0) freeslot=slot;
      continue; };
    if (strnicmp(pf->name,superblock->name,64)!=0)
      continue;                        // Different file name
    if (pf->mode!=superblock->mode)
      continue;                        // Different compression mode
    if (pf->modified.dwLowDateTime!=superblock->modified.dwLowDateTime ||
      pf->modified.dwHighDateTime!=superblock->modified.dwHighDateTime)
      continue;                        // Different timestamp - wrong version?
    if (pf->datasize!=superblock->datasize)
      continue;                        // Different compressed size
    if (pf->origsize!=superblock->origsize)
      continue;                        // Different original size
    // File found. Check for the case of two backup copies printed with
    // different settings.
    if (pf->pagesize!=superblock->pagesize)
      pf->pagesize=0;
    break; };
  if (slot>=NFILE) {
    // No matching descriptor, create new one.
    if (freeslot<0) {
      Reporterror("Maximal number of processed files exceeded");
      return -1; };
    slot=freeslot;
    pf=fproc+slot;
    memset(pf,0,sizeof(t_fproc));
    // Allocate block and recovery tables.
    pf->nblock=(superblock->datasize+NDATA-1)/NDATA;
    pf->datavalid=(uchar *)GlobalAlloc(GPTR,pf->nblock);
    pf->data=(uchar *)GlobalAlloc(GPTR,pf->nblock*NDATA);
    if (pf->datavalid==NULL || pf->data==NULL) {
      if (pf->datavalid!=NULL) GlobalFree((HGLOBAL)pf->datavalid);
      if (pf->data!=NULL) GlobalFree((HGLOBAL)pf->data);
      Reporterror("Low memory");
      return -1; };
    // Initialize remaining fields.
    memcpy(pf->name,superblock->name,64);
    pf->modified=superblock->modified;
    pf->attributes=superblock->attributes;
    pf->filecrc=superblock->filecrc;
    pf->datasize=superblock->datasize;
    pf->pagesize=superblock->pagesize;
    pf->origsize=superblock->origsize;
    pf->mode=superblock->mode;
    if (pf->pagesize>0)
      pf->npages=(pf->datasize+pf->pagesize-1)/pf->pagesize;
    else
      pf->npages=0;
    pf->ndata=0;
    for (i=0; i<pf->npages && i<8; i++)
      pf->rempages[i]=i+1;
    // Initialize statistics and declare descriptor as busy.
    pf->goodblocks=0;
    pf->badblocks=0;
    pf->restoredbytes=0;
    pf->recoveredblocks=0;
    pf->busy=1; };
  // Invalidate page limits and report success.
  pf=fproc+slot;
  pf->page=superblock->page;
  pf->ngroup=superblock->ngroup;
  pf->minpageaddr=0xFFFFFFFF;
  pf->maxpageaddr=0;
  Updatefileinfo(slot,pf);
  return slot;
};

// Adds block recognized by decoder to file described by file descriptor with
// specified index. Returns 0 on success and -1 on any error.
int Addblock(t_block *block,int slot) {
  int i,j;
  t_fproc *pf;
  if (slot<0 || slot>=NFILE)
    return -1;                         // Invalid index of file descriptor
  pf=fproc+slot;
  if (pf->busy==0)
    return -1;                         // Index points to unused descriptor
  // Add block to descriptor.
  if (block->recsize==0) {
    // Ordinary data block.
    i=block->addr/NDATA;
    if ((ulong)(i*NDATA)!=block->addr)
      return -1;                       // Invalid data alignment
    if (i>=pf->nblock)
      return -1;                       // Data outside the data size
    if (pf->datavalid[i]!=1) {
      memcpy(pf->data+block->addr,block->data,NDATA);
      pf->datavalid[i]=1;              // Valid data
      pf->ndata++; };
    pf->minpageaddr=min(pf->minpageaddr,block->addr);
    pf->maxpageaddr=max(pf->maxpageaddr,block->addr+NDATA); }
  else {
    // Data recovery block. I write it to all free locations within the group.
    if (block->recsize!=(ulong)(pf->ngroup*NDATA))
      return -1;                       // Invalid recovery scope
    i=block->addr/block->recsize;
    if (i*block->recsize!=block->addr)
      return -1;                       // Invalid data alignment
    i=block->addr/NDATA;
    for (j=i; j<i+pf->ngroup; j++) {
      if (j>=pf->nblock)
        return -1;                     // Data outside the data size
      if (pf->datavalid[j]!=0) continue;
      memcpy(pf->data+j*NDATA,block->data,NDATA);
      pf->datavalid[j]=2; };           // Valid recovery data
    pf->minpageaddr=min(pf->minpageaddr,block->addr);
    pf->maxpageaddr=max(pf->maxpageaddr,block->addr+block->recsize);
  };
  // Report success.
  return 0;
};

// Processes gathered data. Returns -1 on error, 0 if file is complete and
// number of pages to scan if there is still missing data. In the last case,
// fills list of several first remaining pages in file descriptor.
int Finishpage(int slot,int ngood,int nbad,ulong nrestored) {
  int i,j,r,rmin,rmax,nrec,irec,firstblock,nrempages;
  uchar *pr,*pd;
  t_fproc *pf;
  if (slot<0 || slot>=NFILE)
    return -1;                         // Invalid index of file descriptor
  pf=fproc+slot;
  if (pf->busy==0)
    return -1;                         // Index points to unused descriptor
  // Update statistics. Note that it grows also when the same page is scanned
  // repeatedly.
  pf->goodblocks+=ngood;
  pf->badblocks+=nbad;
  pf->restoredbytes+=nrestored;
  // Restore bad blocks if corresponding recovery blocks are available (max. 1
  // per group).
  if (pf->ngroup>0) {
    rmin=(pf->minpageaddr/(NDATA*pf->ngroup))*pf->ngroup;
    rmax=(pf->maxpageaddr/(NDATA*pf->ngroup))*pf->ngroup;
    // Walk groups of data on current page, one by one.
    for (r=rmin; r<=rmax; r+=pf->ngroup) {
      if (r+pf->ngroup>pf->nblock)
        break;                         // Inconsistent data
      // Count blocks with recovery data in the group.
      nrec=0;
      for (i=r; i<r+pf->ngroup; i++) {
        if (pf->datavalid[i]==2) {
          nrec++; irec=i;
          pf->datavalid[i]=0;          // Prepare for next round
        };
      };
      if (nrec==1) {
        // Exactly one block in group is missing, recovery is possible.
        pr=pf->data+irec*NDATA;
        // Invert recovery data.
        for (j=0; j<NDATA; j++) *pr++^=0xFF;
        // XOR recovery data with good data blocks.
        for (i=r; i<r+pf->ngroup; i++) {
          if (i==irec) continue;
          pr=pf->data+irec*NDATA;
          pd=pf->data+i*NDATA;
          for (j=0; j<NDATA; j++) {
            *pr++^=*pd++;
          };
        };
        pf->datavalid[irec]=1;
        pf->recoveredblocks++;
        pf->ndata++;
      };
    };
  };
  // Check whether there are still bad blocks on the page.
  firstblock=(pf->page-1)*(pf->pagesize/NDATA);
  for (j=firstblock; j<firstblock+(int)(pf->pagesize/NDATA) && j<pf->nblock; j++) {
    if (pf->datavalid[j]!=1) break; };
  if (j<firstblock+(int)(pf->pagesize/NDATA) && j<pf->nblock)
    Message("Unrecoverable errors on page, please scan it again",0);
  else if (nbad>0)
    Message("Page processed, all bad blocks successfully restored",0);
  else
    Message("Page processed",0);
  // Calculate list of (partially) incomplete pages.
  nrempages=0;
  if (pf->pagesize>0) {
    for (i=0; i<pf->npages && nrempages<8; i++) {
      firstblock=i*(pf->pagesize/NDATA);
      for (j=firstblock; j<firstblock+(int)(pf->pagesize/NDATA) && j<pf->nblock; j++) {
        if (pf->datavalid[j]==1)
          continue;
        // Page incomplete.
        pf->rempages[nrempages++]=i+1;
        break;
      };
    };
  };
  if (nrempages<8)
    pf->rempages[nrempages]=0;
  Updatefileinfo(slot,pf);
  if (pf->ndata==pf->nblock) {
    if (autosave==0)
      Message("File restored. Press \"Save\" to save it to disk",0);
    else {
      Message("File complete",0);
      Saverestoredfile(slot,0);
    };
  };
  return 0; ////////////////////////////////////////////////////////////////////
};

// Saves file with specified index and closes file descriptor (if force is 1,
// attempts to save data even if file is not yet complete). Returns 0 on
// success and -1 on error.
int Saverestoredfile(int slot,int force) {
  int n,success;
  ushort filecrc;
  ulong l,length;
  uchar *bufout,*data,*tempdata,*salt,key[AESKEYLEN],iv[16];
  t_fproc *pf;
  aes_decrypt_ctx ctx[1];
  HANDLE hfile;
  if (slot<0 || slot>=NFILE)
    return -1;                         // Invalid index of file descriptor
  pf=fproc+slot;
  if (pf->busy==0 || pf->nblock==0)
    return -1;                         // Index points to unused descriptor
  if (pf->ndata!=pf->nblock && force==0)
    return -1;                         // Still incomplete data
  Message("",0);
  // If data is encrypted, decrypt it to temporary buffer. Decryption in place
  // is possible, but the whole data would be lost if password is incorrect.
  if (pf->mode & PBM_ENCRYPTED) {
    if (pf->datasize & 0x0000000F) {
      Reporterror("Encrypted data is not aligned");
      return -1; };
    if (Getpassword()!=0)
      return -1;                       // User cancelled decryption
    tempdata=(uchar *)GlobalAlloc(GMEM_FIXED,pf->datasize);
    if (tempdata==NULL) {
      Reporterror("Low memory, can't decrypt data");
      return -1; };
    n=strlen(password);
    salt=(uchar *)(pf->name)+32; // hack: put the salt & iv at the end of the name field
    derive_key((const uchar *)password, n, salt, 16, 524288, key, AESKEYLEN);
    memset(password,0,sizeof(password));
    memset(ctx,0,sizeof(aes_decrypt_ctx));
    if(aes_decrypt_key((const uchar *)key,AESKEYLEN,ctx) == EXIT_FAILURE) {
      memset(key,0,AESKEYLEN);
      Reporterror("Failed to set decryption key");
      return -1; };
    memset(key,0,AESKEYLEN);
    memcpy(iv, salt+16, 16); // the second 16-byte block in 'salt' is the IV
    if(aes_cbc_decrypt(pf->data,tempdata,pf->datasize,iv,ctx) == EXIT_FAILURE) {
      Reporterror("Failed to decrypt data");
      memset(ctx,0,sizeof(aes_decrypt_ctx));
      return -1; };
    memset(ctx,0,sizeof(aes_decrypt_ctx));
    filecrc=Crc16(tempdata,pf->datasize);
    if (filecrc!=pf->filecrc) {
      Reporterror("Invalid password, please try again");
      GlobalFree((HGLOBAL)tempdata);
      return -1; }
    else {
      GlobalFree((HGLOBAL)pf->data);
      pf->data=tempdata;
      pf->mode&=~PBM_ENCRYPTED;
    };
  };
  // If data is compressed, unpack it to temporary buffer.
  if ((pf->mode & PBM_COMPRESSED)==0) {
    // Data is not compressed.
    data=pf->data; length=pf->origsize;
    bufout=NULL; }
  else {
    // Data is compressed. Create temporary buffer.
    if (pf->origsize==0)
      pf->origsize=pf->datasize*4;     // Weak attempt to recover
    bufout=(uchar *)GlobalAlloc(GMEM_FIXED,pf->origsize);
    if (bufout==NULL) {
      Reporterror("Low memory");
      return -1; };
    // Unpack data.
    length=pf->origsize;
    success=BZ2_bzBuffToBuffDecompress((char *)bufout,(uint *)&length,
      pf->data,pf->datasize,0,0);
    if (success!=BZ_OK) {
      GlobalFree((HGLOBAL)bufout);
      Reporterror("Unable to unpack data");
      return -1; };
    data=bufout; };
  // Ask user for file name.
  if (Selectoutfile(pf->name)!=0) {    // Cancelled by user
    if (bufout!=NULL) GlobalFree((HGLOBAL)bufout);
    return -1; };
  // Open file and save data.
  hfile=CreateFile(outfile,GENERIC_WRITE,0,NULL,
    CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if (hfile==INVALID_HANDLE_VALUE) {
    if (bufout!=NULL) GlobalFree((HGLOBAL)bufout);
    Reporterror("Unable to create file");
    return -1; };
  WriteFile(hfile,data,length,&l,NULL);
  // Restore old modification date and time.
  SetFileTime(hfile,&pf->modified,&pf->modified,&pf->modified);
  // Close file and restore old basic attributes.
  CloseHandle(hfile);
  SetFileAttributes(outfile,pf->attributes);
  if (bufout!=NULL) GlobalFree((HGLOBAL)bufout);
  if (l!=length) {
    Reporterror("I/O error");
    return -1; };
  // Close file descriptor and report success.
  Closefproc(slot);
  Message("File saved",0);
  return 0;
};

