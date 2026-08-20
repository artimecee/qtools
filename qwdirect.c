#include "include.h"


//**********************************************************
//* Configuring the chipset for the Linux format of the flash drive partition
//**********************************************************
void set_linux_format() {
  
unsigned int sparnum, cfgecctemp;

if (nand_ecc_cfg != 0xffff) cfgecctemp=mempeek(nand_ecc_cfg);
else cfgecctemp=0;
sparnum = 6-((((cfgecctemp>>4)&3)?(((cfgecctemp>>4)&3)+1)*4:4)>>1);
//For ECC-R-S
if (! (is_chipset("MDM9x25") || is_chipset("MDM9x3x") || is_chipset("MDM9x4x"))) set_blocksize(516,1,10); //data - 516, spare - 1 byte, ecc - 10
//For ECC - BCH
else {
	set_udsize(516); //data - 516, spare - 2 or 4 bytes
	set_sparesize(sparnum);
}
}  
  

//*******************************************
//@@@@@@@@@@@@ Head program
void main(int argc, char* argv[]) {
  

			     
unsigned char datacmd[1024]; //sector buffer
			     
unsigned char srcbuf[8192]; //page buffer
unsigned char membuf[1024]; //verification buffer
unsigned char databuf[8192], oobuf[8192]; //data sector and OOB buffers
unsigned int fsize;
FILE* in;
int vflag=0;
int cflag=0;
unsigned int flen=0;
#ifndef WIN32
char devname[]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif
unsigned int cfg0bak,cfg1bak,cfgeccbak,cfgecctemp;
unsigned int i,opt;
unsigned int block,page,sector;
unsigned int startblock=0;
unsigned int bsize;
unsigned int fileoffset=0;
int badflag;
int uxflag=0, ucflag=0, usflag=0, umflag=0, ubflag=0;
int wmode=0; //recording mode
int readlen;

#define w_standart 0
#define w_linux    1
#define w_yaffs    2
#define w_image    3
#define w_linout   4

while ((opt = getopt(argc, argv, "hp:k:b:f:vc:z:l:o:u:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\nThe utility is designed to write a raw flash image through controller registers\n\
The following keys are valid:\n\n\
-p <tty> - specifies the name of the serial port device to communicate with the bootloader\n\
-k # - chipset code (-kl - get a list of codes)\n\
-b # - starting block number for recording \n\
-f <x> - select the recording format:\n\
        -fs (default) - writes only data sectors\n\
        -fl - writes only data sectors in Linux format\n\
        -fy - writing yaffs2 images\n\
	-fi - recording the raw image data+OOB, as is, without recalculating the ECC\n\
	-fo - at the input - only data, on the flash drive - Linux format\n");
printf("\
-z # - OOB size per page, in bytes (overrides the autodetected size)\n\
-l # - number of blocks to be written, by default - until the end of the input file\n\
-o # - offset in blocks in the source file to the beginning of the recorded section\n\
-ux - disable hardware control of bad blocks\n\
-us - ignore signs of bad blocks noted in the input file\n\
-uc - simulate defective blocks of the input file\n\
-um - check the correspondence between defective blocks of the file and the flash drive\n\
-ub - do not check the defectiveness of flash drive blocks before writing (DANGER!)\n\
-v - check the written data after recording\n\
-c n - only erase n blocks, starting from the initial one.\n\
\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;

   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'c':
     sscanf(optarg,"%x",&cflag);
     if (cflag == 0) {
       printf("\n Incorrect key argument -c");
       return;
     }  
     break;
     
   case 'b':
     sscanf(optarg,"%x",&startblock);
     break;
     
   case 'z':
     sscanf(optarg,"%u",&oobsize);
     break;
     
   case 'l':
     sscanf(optarg,"%x",&flen);
     break;
     
   case 'o':
     sscanf(optarg,"%x",&fileoffset);
     break;
     
   case 'v':
     vflag=1;
     break;
     
   case 'f':
     switch(*optarg) {
       case 's':
        wmode=w_standart;
	break;
	
       case 'l':
        wmode=w_linux;
	break;
	
       case 'y':
        wmode=w_yaffs;
	break;
	
       case 'i':
        wmode=w_image;
	break;

       case 'o':
        wmode=w_linout;
	break;
	
       default:
	printf("\n Incorrect value of the -f key\n");
	return;
     }
     break;
     
   case 'u':  
     switch (*optarg) {
       case 'x':
         uxflag=1;
	 break;
	 
       case 's':
         usflag=1;
	 break;
	 
       case 'c':
         ucflag=1;
	 break;
	 
       case 'm':
         umflag=1;
	 break;
	 
       case 'b':
         ubflag=1;
	 break;
	 
       default:
	printf("\nInvalid key value -u\n");
	return;
     }
     break;
     
   case '?':
   case ':':  
     return;
  }
}  

if (uxflag+usflag+ucflag+umflag > 1) {
  printf("\nThe switches -ux, -us, -uc, -um are incompatible with each other\n");
  return;
}  

if (uxflag+ubflag > 1) {
  printf("\nThe -ux and -ub switches are incompatible with each other\n");
  return;
}  

if (uxflag && (wmode != w_image)) {
  printf("\n The -ux switch is valid only in -fi mode\n");
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

if (!cflag) { 
 in=fopen(argv[optind],"rb");
 if (in == 0) {
   printf("\nError opening input file\n");
   return;
 }
 
}
else if (optind < argc) {//in erase mode, no input file is needed
  printf("\nWith the -c switch the input file is invalid\n");
  return;
}

hello(0);


if ((wmode == w_standart)||(wmode == w_linux)) oobsize=0; //for non-OOB input files
oobsize/=spp;   //now oobsize is the OOB size per sector

//Resetting the nand controller
nand_reset();

//Saving controller register values
cfg0bak=mempeek(nand_cfg0);
cfg1bak=mempeek(nand_cfg1);
cfgeccbak=mempeek(nand_ecc_cfg);

//-------------------------------------------
//erase mode
//-------------------------------------------
if (cflag) {
  if ((startblock+cflag) > maxblock) cflag=maxblock-startblock;
  printf("\n");
  for (block=startblock;block<(startblock+cflag);block++) {
    printf("\r Erase block %03x",block); 
    if (!ubflag) 
      if (check_block(block)) {
	printf("- badblock, erasing is prohibited\n");
	continue; 
      }	
    block_erase(block);
  }  
  printf("\n");
  return;
}

//ECC on-off
if (wmode != w_image) {
  mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe); 
  mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe); 
}
else {
  mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)|1); 
  mempoke(nand_cfg1,mempeek(nand_cfg1)|1); 
}
  
//Determining the file size
if (wmode == w_linout) bsize=pagesize*ppb; //for this mode the file does not contain OOB data, but OOB writing is required
else bsize=(pagesize+oobsize*spp)*ppb;  //size in bytes of a full flash drive block, taking into account OOB
fileoffset*=bsize; //convert the offset from blocks to bytes
fseek(in,0,SEEK_END);
i=ftell(in);
if (i<=fileoffset) {
  printf("\n Offset %i is outside the file boundary\n",fileoffset/bsize);
  return;
}
i-=fileoffset; //cut off the size of the skipped area from the file length
fseek(in,fileoffset,SEEK_SET); //stand at the beginning of the recording section
fsize=i/bsize; //size in blocks
if ((i%bsize) != 0) fsize++; //round up to the block border

if (flen == 0) flen=fsize;
else if (flen>fsize) {
  printf("\nThe specified length %u exceeds the file size %u\n",flen,fsize);
  return;
} 
  
printf("\nRecording from file %s, starting block %03x, size %03x\nRecording mode:",argv[optind],startblock,flen);


switch (wmode) {
  case w_standart:
    printf("data only, standard format\n");
    break;
    
  case w_linux: 
    printf("data only, Linux input format\n");
    break;
    
  case w_image: 
    printf("raw image without ECC calculation\n");
	printf("Data format: %u+%u\n",sectorsize,oobsize); 
    break;
    
  case w_yaffs: 
    printf("image yaffs2\n");
    set_linux_format();
    break;

  case w_linout: 
     printf("Linux format on a flash drive\n");
    set_linux_format();
    break;
}   
    
port_timeout(1000);

//block loop
if ((startblock+flen) > maxblock) flen=maxblock-startblock;
for(block=startblock;block<(startblock+flen);block++) {
  //check, if necessary, whether the block is defective
  badflag=0;
  if (!uxflag && !ubflag)  badflag=check_block(block);
  //target block is defective
  if (badflag) {
//    printf("\n %x - badflag\n",block);
    //skip the defective block and move on
    if (!umflag && !ubflag) {
      flen++;   //we move the end boundary of the input file - we missed the block, the data is moved apart
      printf("\n Block %x is defective - skip\n",block);
      continue;
    }  
  }  
  //erase the block
  if (!badflag || ubflag) {
    block_erase(block);
  }  
              
  bch_reset();

  //cycle through pages
  for(page=0;page<ppb;page++) {

    memset(oobuf,0xff,sizeof(oobuf));
    memset(srcbuf,0xff,pagesize); //fill the FF buffer to read partial pages
    //read the entire page dump
    if (wmode == w_linout) readlen=fread(srcbuf,1,pagesize,in);
    else readlen=fread(srcbuf,1,pagesize+(spp*oobsize),in);
    if (readlen == 0) goto endpage;  //0 - all data from the file has been read
 
    //srcbuf has been read - check if there is a bedblock there
    if (test_badpattern(srcbuf)) {
      //there really is a bad block
      if (!usflag) {
	if (page == 0) printf("\nA sign of a defective block was detected in the input dump - skip it\n");
	continue;  //-us - skip this block, page by page
      }
      if (ucflag) {
	//creating a bedblock
	mark_bad(block);
	if (page == 0) printf("\r Block %x is marked as defective according to the input file!\n",block);
	continue;
      }
      if (umflag && !badflag) {
	//the input bedblock does not match the bedblock on the flash drive
	printf("\n Block %x: no defect found on flash, exit!\n",block);
	return;
      }
      if (umflag && badflag && page == 0) printf("\r Block %x - defects match, continue recording\n",block);
    }
    else if (umflag && badflag) {
	printf("\n Block %x: an unexpected defect was detected in the flash, shut down!\n",block);
	return;
    }
      
    //parsing the dump by buffers
    switch (wmode) {
      case w_standart:
      case w_linux:
      case w_image:
      //for all modes except yaffs and linout - input file format 512+obb
      for (i=0;i<spp;i++) {
		memcpy(databuf+sectorsize*i,srcbuf+(sectorsize+oobsize)*i,sectorsize);
		if (oobsize != 0) memcpy(oobuf+oobsize*i,srcbuf+(sectorsize+oobsize)*i+sectorsize,oobsize);
     }  
	break;
	 
      case w_yaffs:
      //for yaffs mode - input file format pagesize+obb
		memcpy(databuf,srcbuf,sectorsize*spp);
		memcpy(oobuf,srcbuf+sectorsize*spp,oobsize*spp);
      break;

      case w_linout:
      //for this mode - the input file contains only pagesize data
		memcpy(databuf,srcbuf,pagesize);
	break;
    }
    
    //set the address of the flash drive
    printf("\r Block: %04x Page: %02x",block,page); fflush(stdout);
    setaddr(block,page);

    //set the write command code
    switch (wmode) {
	case w_standart:
	mempoke(nand_cmd,0x36); //page program - write only the body of the block
    break;

	case w_linux:
	case w_yaffs:
	case w_linout:
        mempoke(nand_cmd,0x39); //write data+spare, ECC is calculated by the controller
    break;
	 
	case w_image:
        mempoke(nand_cmd,0x39); //write data+spare+ecc, all data from the buffer goes directly to the flash drive
    break;
    }

    //cycle by sector
    for(sector=0;sector<spp;sector++) {
      memset(datacmd,0xff,sectorsize+64); //fill the sector buffer FF - default values
      
      //fill the sector buffer with data
      switch (wmode) {
        case w_linux:
	//Linux (Chinese perverse) version of the data layout, recording without OOB
          if (sector < (spp-1))  
	 //first n sectors
             memcpy(datacmd,databuf+sector*(sectorsize+4),sectorsize+4); 
          else 
	 //last sector
             memcpy(datacmd,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); //data of the last sector - shorten
	  break;
	  
        case w_standart:
	 //standard format - 512 byte sectors only, no OOB
          memcpy(datacmd,databuf+sector*sectorsize,sectorsize); 
	  break;
	  
	case w_image:
	 //raw image - data+oob, ECC is not calculated
          memcpy(datacmd,databuf+sector*sectorsize,sectorsize);       // data
          memcpy(datacmd+sectorsize,oobuf+sector*oobsize,oobsize);    // oob
	  break;

	case w_yaffs:
	 //yaffs image - write only data in 516-byte blocks
	 //and the yaffs tag at the end of the last block
	 //the input file has the format page+oob, but the tag is located at position 0 OOB
          if (sector < (spp-1))  
	 //first n sectors
             memcpy(datacmd,databuf+sector*(sectorsize+4),sectorsize+4); 
          else  {
	 //last sector
             memcpy(datacmd,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); //data of the last sector - shorten
             memcpy(datacmd+sectorsize-4*(spp-1),oobuf,16 );    //add the yaffs tag to it
		  }
	  break;

	case w_linout:
	 //write only data in 516-byte blocks
          if (sector < (spp-1))  
	 //first n sectors
             memcpy(datacmd,databuf+sector*(sectorsize+4),sectorsize+4); 
          else  {
	 //last sector
             memcpy(datacmd,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); //data of the last sector - shorten
		  }
	  break;

      }
      //send the sector to the sector buffer
	  if (!memwrite(sector_buf,datacmd,sectorsize+oobsize)) {
		printf("\nSector buffer transfer error\n");
		return;
      }	
      //if necessary, disable bedblock control
      if (uxflag) hardware_bad_off();
      //execute the write command and wait for it to complete
      mempoke(nand_exec,0x1);
      nandwait(); 
      //turn back bedblock control
      if (uxflag) hardware_bad_on();
     }  //end of sector recording cycle
     if (!vflag) continue;   //no verification required
    //data verification
     printf("\r");
     setaddr(block,page);
     mempoke(nand_cmd,0x34); //reading data+ecc+spare
     
     //verification cycle by sector
     for(sector=0;sector<spp;sector++) {
      //we read the next sector
      mempoke(nand_exec,0x1);
      nandwait();
      
      //read sector buffer
      memread(membuf,sector_buf,sectorsize+oobsize);
      switch (wmode) {
        case w_linux:
 	//verification in Linux format
	  if (sector != (spp-1)) {
	    //all sectors except the last one
	    for (i=0;i<sectorsize+4;i++) 
	      if (membuf[i] != databuf[sector*(sectorsize+4)+i])
                 printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[sector*(sectorsize+4)+i]); 
	  }  
	  else {
	      //last sector
	    for (i=0;i<sectorsize-4*(spp-1);i++) 
	      if (membuf[i] != databuf[(spp-1)*(sectorsize+4)+i])
                 printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[(spp-1)*(sectorsize+4)+i]); 
	  }    
	  break; 
	  
		 case w_standart:
	     case w_image:  
         case w_yaffs:  
	     case w_linout: //not working yet!
          //verification in standard format
	  for (i=0;i<sectorsize;i++) 
	      if (membuf[i] != databuf[sector*sectorsize+i])
                 printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[sector*sectorsize+i]); 
	  break;   
      }  // switch(wmode)
    }  //end of sector verification cycle
  }  //end of page loop
} //end of block cycle
endpage:  
mempoke(nand_cfg0,cfg0bak);
mempoke(nand_cfg1,cfg1bak);
mempoke(nand_ecc_cfg,cfgeccbak);
printf("\n");
}

