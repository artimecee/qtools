//---------------------------------------------------
//Utility for working with defective flash blocks
//---------------------------------------------------

#include "include.h"

//*********************************
//* Building a list of bedblocks
//*********************************
void defect_list(int start, int len) {
  
FILE* out; 
int blk;
int badcount=0;
int pn;

out=fopen("badblock.lst","w");
if (out == 0) {
  printf("\nCannot create file badblock.lst\n");
  return;
}
fprintf(out,"List of defective blocks");

//loading the partition table from a flash drive
load_ptable("@");

printf("\nBuilding a list of defective blocks in the interval %08x - %08x\n",start,start+len);

//main loop in blocks
for(blk=start;blk<(start+len);blk++) {
 printf("\r Checking block %08x",blk); fflush(stdout);
 if (check_block(blk)) {
     printf(" - badblock");
     fprintf(out,"\n%08x",blk);
     //increase the bedblock counter
     badcount++;
     //display the name of the section with this block
     if (validpart) {
       //search for the section in which this block lies
       pn=block_to_part(blk);
       if (pn != -1) {
         printf(" (%s+%x)",part_name(pn),blk-part_start(pn));
         fprintf(out," (%s+%x)",part_name(pn),blk-part_start(pn));
       }       	 
    }
    printf("\n");
  }
}     // blk
fprintf(out,"\n");
fclose (out);
printf("\r * Total defective blocks: %i\n",badcount);
}
 

//********************************************************8 
//* Scan for ECC errors
//* flag=0 - all errors, 1 - only correctable ones
//********************************************************8 
void ecc_scan(int start, int len, int flag) {
  
int blk;
int errcount=0;
int pg,sec;
int stat;
FILE* out; 

printf("\nBuilding a list of ECC errors in the interval %08x - %08x\n",start,start+len);

out=fopen("eccerrors.lst","w");
fprintf(out,"List of ECC errors");
if (out == 0) {
  printf("\nCannot create file eccerrors.lst\n");
  return;
}

//turn on ECC
mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe); 
mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe); 

for(blk=start;blk<(start+len);blk++) {
 printf("\r Checking block %08x",blk); fflush(stdout);
 if (check_block(blk)) {
     printf(" - badblock\n");
     continue;
 }
 bch_reset(); 
 for (pg=0;pg<ppb;pg++) {
   setaddr(blk,pg);
   mempoke(nand_cmd,0x34); //reading data
   for (sec=0;sec<spp;sec++) {
    mempoke(nand_exec,0x1);
    nandwait();
    stat=check_ecc_status();
    if (stat == 0) continue;
    if ((stat == -1) && (flag == 1)) continue;
    if (stat == -1) { 
      printf("\r!  Block %x Page %d sector %d: uncorrectable read error\n",blk,pg,sec);
      fprintf(out,"\r!  Block %x Page %d sector %d: uncorrectable read error\n",blk,pg,sec);
    }  
    else {
      printf("\r!  Block %x Page %d sector %d: bit adjusted: %d\n",blk,pg,sec,stat);
      fprintf(out,"\r!  Block %x Page %d sector %d: bit adjusted: %d\n",blk,pg,sec,stat);
    }  
    errcount++;
   }
 }
} 
printf("\r * Total errors: %i\n",errcount);
}
 

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {

unsigned int start=0,len=0,opt;
int mflag=0, uflag=0, sflag=0;
int dflag=0;
int badloc=0;
int eflag=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:b:l:dm:k:u:s:e:")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\nUtility for working with defective flash drive blocks\n\
 The following keys are valid:\n\n\
-p <tty> - serial port for communicating with the bootloader\n\
-k # - chipset code (-kl - get a list of codes)\n\
-b <blk> - starting number of the block to be read (default 0)\n\
-l <num> - number of readable blocks, 0 - until the end of the flash drive\n\n\
-d - display a list of existing defective blocks\n\
-e# - display a list of ECC errors, #=0 - all errors, 1 - only correctable ones\n\
-m blk - mark blk block as defective\n\
-u blk - remove the sign of defects from the blk block\n\
-s L### - permanently set the marker position to byte ###, L=U(user) or S(spare)\n\
");
    return;

   case 'k':
    define_chipset(optarg);
    break;

   case 'p':
    strcpy(devname,optarg);
    break;

   case 'b':
     sscanf(optarg,"%x",&start);
     break;

   case 'l':
     sscanf(optarg,"%x",&len);
     break;

   case 'm':
     sscanf(optarg,"%x",&mflag);
     break;

   case 'u':
     sscanf(optarg,"%x",&uflag);
     break;

   case 's':
     parse_badblock_arg(optarg, &sflag, &badloc);
     break;

   case 'd':
     dflag=1;
     break;

   case 'e':
     sscanf(optarg,"%i",&eflag);
     if ((eflag != 0) && (eflag != 1)) {
       printf("\nInvalid key value -e\n");
       return;
     }  
     break;
     
   case '?':
   case ':':  
     return;
  }
}  
if ((eflag != -1) && (dflag != 0)) {
  printf("\nThe -e and -d switches are incompatible\n");
  return;
}  

#ifdef WIN32
if (*devname == '\0')
{
   printf("\n - Serial port not specified\n"); 
   return; 
}
#endif


if (!open_port(devname))  {
#ifndef WIN32
   printf("\n - Serial port %s does not open\n", devname); 
#else
   printf("\n - Serial port COM%s does not open\n", devname); 
#endif
   return; 
}

hello(0);

//Reset all controller operations
mempoke(nand_cmd,1); 
mempoke(nand_exec,0x1);
nandwait();

//setting the position of the bedblock marker
if (sflag) set_badmark_pos (sflag, badloc);

//###################################################
//Defective block list mode:
//###################################################

if (dflag) {
  if (len == 0) len=maxblock-start; //to the end of the flash drive
  defect_list(start,len);
  return;
}  

//###################################################
//# Mode for building a list of ECC errors
//###################################################
if (eflag != -1) {
  if (len == 0) len=maxblock-start; //to the end of the flash drive
  ecc_scan(start,len,eflag);
  return;
}  

//###################################################
//# Mark a block as defective
//###################################################
if (mflag) {
  if (mark_bad(mflag)) {
   printf("\n Block %x is marked as defective\n",mflag);
  }
  else printf("\n Block %x is already defective\n",mflag);	
  return;
}

//###################################################
//# Removing a defective marker from the block
//###################################################
if (uflag) {
  if (unmark_bad(uflag)) {
     printf("\nBlock marker %x deleted\n",uflag);
  }
  else printf("\n Block %x is not defective\n",uflag);
  return;
}
}
