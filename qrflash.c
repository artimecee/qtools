#include "include.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Read modem flash to file
//
//

//bedblock processing flags
enum {
  BAD_UNDEF,
  BAD_FILL,
  BAD_SKIP,
  BAD_IGNORE,
  BAD_DISABLE
};


//Readable data formats
enum {
  RF_AUTO,
  RF_STANDART,
  RF_LINUX,
  RF_YAFFS
};  

int bad_processing_flag=BAD_UNDEF;
unsigned char *blockbuf;

//Structure for storing a list of read errors

struct {
  int page;
  int sector;
  int errcode;
} errlist[1000];  

int errcount;
int qflag=0;

//********************************************************************************
//* Loading a block into a block buffer
//*
//* returns 0 if the block is defective, or 1 if it was read normally
//* cwsize - readable sector size (including OOB, if necessary)
//********************************************************************************
unsigned int load_block(int blk, int cwsize) {

int pg,sec;
int oldudsize,cfg0;
unsigned int cfgecctemp;
int status;

errcount=0;
if (bad_processing_flag == BAD_DISABLE) hardware_bad_off();
else if (bad_processing_flag != BAD_IGNORE) {
   if (check_block(blk)) {
    //bedblock detected
    memset(blockbuf,0xbb,cwsize*spp*ppb); //fill the block buffer with placeholder
    return 0; //return the badblock sign
  }
} 
//good block, or we don’t give a damn about bedblocks - read the block

//set udsize to the size of the readable area
cfg0=mempeek(nand_cfg0);
//oldudsize=get_udsize();
//set_udsize(cwsize);
//set_sparesize(0);

nand_reset();
if (cwsize>(sectorsize+4)) mempoke(nand_cmd,0x34); //reading data+ecc+spare without correction
else mempoke(nand_cmd,0x33);    //reading data with correction

bch_reset();
for(pg=0;pg<ppb;pg++) {
  setaddr(blk,pg);
  for (sec=0;sec<spp;sec++) {
   mempoke(nand_exec,0x1); 
   nandwait();
   status=check_ecc_status();
   if (status != 0) {
//     printf("\n--- blk %x  pg %i  sec  %i err %i---\n",blk,pg,sec,check_ecc_status());
     errlist[errcount].page=pg;
     errlist[errcount].sector=sec;
     errlist[errcount].errcode=status;
     errcount++;
   }
   
   memread(blockbuf+(pg*spp+sec)*cwsize,sector_buf, cwsize);
//   dump(blockbuf+(pg*spp+sec)*cwsize,cwsize,0);
  } 
}  
if (bad_processing_flag == BAD_DISABLE) hardware_bad_on();
//set_udsize(oldudsize);
mempoke(nand_cfg0,cfg0);
return 1; //fuck - block read
}
  
//***************************************
//* Read a block of data into an output file
//***************************************
unsigned int read_block(int block,int cwsize,FILE* out) {

unsigned int okflag=0;

okflag=load_block(block,cwsize);
if (okflag || (bad_processing_flag != BAD_SKIP)) {
  //the block was read or not read, but we skip bedblocks
   fwrite(blockbuf,1,cwsize*spp*ppb,out); //write it to a file
}
return !okflag;
} 

//********************************************************************************
//* Read data block for partitions with protected spare (516-byte sectors)
//* yaffsmode=0 - read data, 1 - read data and yaffs2 tag
//********************************************************************************
unsigned int read_block_ext(int block, FILE* out, int yaffsmode) {
unsigned int page,sec;
unsigned int okflag;
unsigned char* pgoffset;
unsigned char* udoffset;
unsigned char extbuf[2048]; //pseudo-OOB build buffer

okflag=load_block(block,sectorsize+4);
if (!okflag && (bad_processing_flag == BAD_SKIP)) return 1; //bedblock detected

//cycle through pages
for(page=0;page<ppb;page++)  {
  pgoffset=blockbuf+page*spp*(sectorsize+4); //offset to current page
  //by sector
  for(sec=0;sec<spp;sec++) {
   udoffset=pgoffset+sec*(sectorsize+4); //offset to current sector
//   printf("\n page %i  sector %i\n",page,sec);
   if (sec != (spp-1)) {
     //For non-last sectors
     fwrite(udoffset,1,sectorsize+4,out);    //Sector body + 4 bytes OBB
//     dump(udoffset,sectorsize+4,udoffset-blockbuf);
   }  
   else { 
     //for the last sector
     fwrite(udoffset,1,sectorsize-4*(spp-1),out);   //Sector body - tail oob
//     dump(udoffset,sectorsize-4*(spp-1),udoffset-blockbuf);
   }  
  }

//Reading yafs2 tag images
  if (yaffsmode) {
    memset(extbuf,0xff,oobsize);
    memcpy(extbuf,pgoffset+(sectorsize+4)*(spp-1)+(sectorsize-4*(spp-1)),16);
//    printf("\n page %i oob\n",page);
//    dump(pgoffset+(sectorsize+4)*(spp-1)+(sectorsize-4*(spp-1)),16,pgoffset+(sectorsize+4)*(spp-1)+(sectorsize-4*(spp-1))-blockbuf);
    fwrite(extbuf,1,oobsize,out);
  }  
}

return !okflag; 
} 


//*************************************************************
//* Read data block for non-file Linux partitions
//*************************************************************
unsigned int read_block_resequence(int block, FILE* out) {
 return read_block_ext(block,out,0);
} 

//*************************************************************
//* Reading data block for yaffs2 file partitions
//*************************************************************
unsigned int read_block_yaffs(int block, FILE* out) {
 return read_block_ext(block,out,1);
} 

//****************************************
//* Displays a list of reading errors
//****************************************
void show_errlist() {
  
int i;  
  
if (qflag || (errcount == 0)) return; //there were no errors
for (i=0;i<errcount;i++) {
  if (errlist[i].errcode == -1) printf("\n!   Page %d sector %d: uncorrectable read error",
                                   errlist[i].page,errlist[i].sector);
  else                          printf("\n!   Page %d sector %d: bit adjusted: %d",
                                   errlist[i].page,errlist[i].sector,errlist[i].errcode);
}
printf("\n");
}

//*****************************
//* reading raw flash
//*****************************
void read_raw(int start,int len,int cwsize,FILE* out, unsigned int rflag) {
  
int block;  
unsigned int badflag;

printf("\n Read blocks %08x - %08x",start,start+len-1);
printf("\n Data format: %u+%i\n",sectorsize,cwsize-sectorsize);
//main cycle
//by block
for (block=start;block<(start+len);block++) {
  printf("\r Block: %08x",block); fflush(stdout);
  switch (rflag) {
    case RF_AUTO:
    case RF_STANDART:
       badflag=read_block(block,cwsize,out);
       break;
      
    case RF_LINUX:   
       badflag=read_block_resequence(block,out); 
       break;
       
    case RF_YAFFS:
       badflag=read_block_yaffs(block,out); 
       break;
  }  
  show_errlist(); 
  if (badflag != 0) printf(" - Badblock\n");   
} 
printf("\n"); 
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {
  
unsigned char filename[300]={0};
unsigned int i,block,filepos,lastpos;
unsigned char c;
unsigned int start=0,len=0,opt;
unsigned int partlist[60]; //list of partitions allowed for reading
unsigned int cwsize;  //size of the data portion read from the sector buffer at a time
FILE* out;
int partflag=0;  //0 - raw flash, 1 - working with partition table
int eccflag=0;  //1 - disable ECC, 0 - enable
int partnumber=-1; //flag for selecting a partition for reading, -1 - all partitions, 1 - by list
int rflag=RF_AUTO;     //partition format: 0 - auto, 1 - standard, 2 - Linux-Chinese, 3 - yaffs2
int listmode=0;    //1- partition map output
int truncflag=0;  //1 - cut off all FF from the end of the section
int xflag=0;      // 
unsigned int badflag;

int forced_oobsize=-1;

//Partition table source. By default - MIBIB partition on the flash drive
char ptable_file[200]="@";

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

memset(partlist,0,sizeof(partlist)); //clearing the list of partitions allowed for reading

while ((opt = getopt(argc, argv, "hp:b:l:o:xs:ef:mtk:r:z:u:q")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\nThe utility is designed to read a flash memory image through a modified bootloader\n\
 The following keys are valid:\n\n\
-p <tty> - serial port for communicating with the bootloader\n\
-e - disable ECC correction when reading\n\
-x - read full sector - data+oob (without key - only data)\n\n\
-k # - chipset code (-kl - get a list of codes)\n\
-z # - OOB size per page, in bytes (overrides the autodetected size)\n\
-q - disable displaying a list of reading errors\n\
-u <x> - determines the mode of processing defective blocks:\n\
   -uf - fill the image of the defective block in the output file with byte 0xbb\n\
   -us - skip bad blocks when reading\n\
   -ui - ignore defective block marker (read as read)\n\
   -ux - disable hardware control of bad blocks\n");
printf("\n * For raw reading mode and checking for bad blocks:\n\
-b <blk> - starting number of the block to be read (default 0)\n\
-l <num> - number of readable blocks, 0 - until the end of the flash drive\n\
-o <file> - output file name (default qflash.bin) (read only)\n\n\
 * For partition reading mode:\n\n\
-s <file> - take partition map from file\n\
-s @ - take partition map from flash (default for -f and -m)\n\
-s - - take partition map from file ptable/current-r.bin\n\
-m - display the full partition map and exit\n\
-f n - read only section number n (can be specified several times to form a list of sections)\n\
-f * - read all sections\n\
-t - cut off all FFs after the last significant byte of the section\n\
-r <x> - data format:\n\
    -ra - (by default and only for partition mode) auto-detection of format by partition attribute\n\
    -rs - standard format (512-byte blocks)\n\
    -rl - Linux format (516-byte blocks, except the last one on the page)\n\
    -ry - yaffs2 format (page+tag)\n\
	");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;
    
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'e':
     eccflag=1;
     break;
    
   case 'o':
    strcpy(filename,optarg);
    break;
    
   case 'b':
     sscanf(optarg,"%x",&start);
     break;

   case 'l':
     sscanf(optarg,"%x",&len);
     break;

   case 'z':
     sscanf(optarg,"%u",&forced_oobsize);
     break;

   case 'u':
     if (bad_processing_flag != BAD_UNDEF) {
       printf("\nDuplicate key u - error\n");
       return;
     }  
     switch(*optarg) {
       case 'f':
	 bad_processing_flag=BAD_FILL;
	 break; 
       case 'i':
	 bad_processing_flag=BAD_IGNORE;
	 break; 
       case 's':
	 bad_processing_flag=BAD_SKIP;
	 break; 
       case 'x':
	 bad_processing_flag=BAD_DISABLE;
	 break;
       default:
	 printf("\nInvalid key value u\n");
	 return;
     } 
     break;
	 

   case 'r':
     switch(*optarg) {
       case 'a':
	 rflag=RF_AUTO;   //auto
	 break;     
       case 's':
	 rflag=RF_STANDART;   //standard
	 break;
       case 'l':
	 rflag=RF_LINUX;   //Linux
	 break;
       case 'y':
         rflag=RF_YAFFS;
         break;	 
       default:
	 printf("\nInvalid key value r\n");
	 return;
     } 
     break;
     
   case 'x':
     xflag=1;
     break;
     
   case 's':
     partflag=1;
     strcpy(ptable_file,optarg);
     break;
     
   case 'm':
     partflag=1;  //force partition mode
     listmode=1;
     break;
     
   case 'f':
     partflag=1; //force partition mode
     if (optarg[0] == '*') {
       //all sections
       partnumber=-1;
       break;
     }
     //building a list of sections
     partnumber=1;
     sscanf(optarg,"%u",&i);
     partlist[i]=1;
     break;
     
   case 't':
     truncflag=1;
     break;

   case 'q':
     qflag=1;
     break;
     
   case '?':
   case ':':  
     return;
  }
}  

//Checking for keyless start
if ((start == 0) && (len == 0) && !xflag && !partflag) {
  printf("\nNo operating mode key is specified\n");
  return;
}  

//Define the default value of the -u key
if (bad_processing_flag==BAD_UNDEF) {
  if (partflag == 0) bad_processing_flag=BAD_FILL; //to read a range of blocks
  else bad_processing_flag=BAD_SKIP;               //to read sections
}  

if ((truncflag == 1)&&(xflag == 1)) {
  printf("\nThe -t and -x switches are incompatible\n");
  return;
}  


//Configuring port and flash drive parameters
//In the mode of outputting a partition map from a file, this entire setting is skipped
//----------------------------------------------------------------------------
if (! (listmode && ptable_file[0] != '@')) {

#ifdef WIN32
 if (*devname == '\0') {
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
//load the flash drive parameters
hello(0);
//allocate memory for a block buffer
blockbuf=(unsigned char*)malloc(300000);

if (forced_oobsize != -1) {
  printf("\n! Using OOB size %d instead of %d\n",forced_oobsize,oobsize);
  oobsize=forced_oobsize;
}  

cwsize=sectorsize;
if (xflag) cwsize+=oobsize/spp; //increase the size of the codeword by the size of the OOB portion for each sector
mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe|eccflag); // ECC on/off
mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe|eccflag); // ECC on/off

mempoke(nand_cmd,1); //Reset all controller operations
mempoke(nand_exec,0x1);
nandwait();
  
//set the command code
mempoke(nand_cmd,0x34); //reading data+ecc+spare

//clearing the sector buffer
for(i=0;i<cwsize;i+=4) mempoke(sector_buf+i,0xffffffff);
}


//###################################################
//Raw flash reading mode
//###################################################

if (partflag == 0) {

if (len == 0) len=maxblock-start; //to the end of the flash drive
  if (filename[0] == 0) {
    switch(rflag) {
      case RF_AUTO:
      case RF_STANDART:
	strcpy(filename,"qrflash.bin");
	break;
      case RF_LINUX:
        strcpy(filename,"qrflash.oob");
        break;
      case RF_YAFFS:
        strcpy(filename,"qrflash.yaffs");
        break;
    }
  } 
  out=fopen(filename,"wb");
  if (out == 0) {
    printf("\nError opening output file %s",filename);
    return;
  }  
  read_raw(start,len,cwsize,out,rflag);
  fclose(out);
  return;
}  


//###################################################
//Partition table read mode
//###################################################

//load the partition table
if (!load_ptable(ptable_file)) { 
    printf("\nPartition table not found. Let's finish the job.\n");
    return;
}

//check if the table has loaded
if (!validpart) {
   printf("\nPartition table not found or damaged\n");
   return;
}

//Partition table view mode
if (listmode) {
  list_ptable();
  return;
}  

if ((partnumber != -1) && (partnumber>=fptable.numparts)) {
  printf("\nInvalid partition number: %i, total partitions %u\n",partnumber,fptable.numparts);
  return;
}  

print_ptable_head();

//Main cycle - section by section
for(i=0;i<fptable.numparts;i++) {

  //Reading the section
 
  //If the mode for all sections is not set, check whether this particular section is selected
  if ((partnumber == -1) || (partlist[i]==1)) { 
  //Displaying a description of the section
  show_part(i);
  //form the file name depending on the format
  if (rflag == RF_YAFFS) sprintf(filename,"%02u-%s.yaffs2",i,part_name(i)); 
  else if (cwsize == sectorsize) sprintf(filename,"%02u-%s.bin",i,part_name(i)); 
  else                   sprintf(filename,"%02u-%s.oob",i,part_name(i));  
  //replace the colon with a minus in the file name
  if (filename[4] == ':') filename[4]='-';
  //open the output file
  out=fopen(filename,"wb");  
  if (out == 0) {
	  printf("\nError opening output file %s\n",filename);
	  return;
  }
  //Loop through partition blocks
  for(block=part_start(i); block < (part_start(i)+part_len(i)); block++) {
          printf("\r * R: block %06x [start+%03x] (%i%%)",block,block-part_start(i),(block-part_start(i)+1)*100/part_len(i)); 
	  fflush(stdout);
	  
    //Actually reading the block
  switch (rflag) {
    case RF_AUTO: //auto format selection
      if ((fptable.part[i].attr2 != 1)||(cwsize>(sectorsize+4))) 
         //raw reading or reading uncooked sections
         badflag=read_block(block,cwsize,out);
      else 
	 //reading Chinese Linux partitions
	 badflag=read_block_resequence(block,out);
      break;
	       
    case RF_STANDART: //standard format
      badflag=read_block(block,cwsize,out);
      break;
	      
    case RF_LINUX: //Chinese Linux format
      badflag=read_block_resequence(block,out);
      break;
	      
    case RF_YAFFS: //image of file partitions
      badflag=read_block_yaffs(block,out);
      break;
  }
  //display a list of found errors
  show_errlist(); 
  if (badflag != 0) {
      printf("- defective block");
      if (bad_processing_flag == BAD_SKIP) printf (", skip");
      if (bad_processing_flag == BAD_IGNORE) printf (", read as is");
      if (bad_processing_flag == BAD_FILL) printf (", mark in the output file");
      printf("\n");
  }  
}    //end of block cycle

  fclose(out);
//Trimming all FF tail
  if (truncflag) {
      out=fopen(filename,"r+b");  //re-open the output file
      fseek(out,0,SEEK_SET);  //rewind the file to the beginning
      lastpos=0;
      for(filepos=0;;filepos++) {
	c=fgetc(out);
	if (feof(out)) break;
	if (c != 0xff) lastpos=filepos;  //found significant byte
      }
#ifndef WIN32
       ftruncate(fileno(out),lastpos+1);   //crop the file
#else
       _chsize(_fileno(out),lastpos+1);   //crop the file
#endif
      fclose(out);
  }	
 }  //checking partition selection
}   //cycle by sections

//restoring the ECC
mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe); // ECC on BCH
mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe); // ECC on R-S

printf("\n"); 
    
} 
