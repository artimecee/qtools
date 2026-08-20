#include "include.h"

//Write block size
#define wbsize 1024
//#define wbsize 1538
  
//partition table storage
struct  {
  char filename[50];
  char partname[16];
} ptable[30];

unsigned int npart=0;    //number of partitions in the table

unsigned int cfg0,cfg1; //saving controller configuration

//*****************************************************
//* Restoring controller configuration
//*****************************************************
void restore_reg() {
  
return;  
mempoke(nand_cfg0,cfg0);  
mempoke(nand_cfg1,cfg1);  
}


//***********************************8
//* Install secure mode
//***********************************8
int secure_mode() {
  
unsigned char iobuf[600];
unsigned char cmdbuf[]={0x17,1};
int iolen;

iolen=send_cmd(cmdbuf,2,iobuf);
if (iobuf[1] == 0x18) return 1;
show_errpacket("secure_mode()",iobuf,iolen);
return 0; //there was a mistake

}  


//*******************************************
//* Sending the partition table to the bootloader
//*******************************************

void send_ptable(char* ptraw, unsigned int len, unsigned int mode) {
  
unsigned char iobuf[600];
unsigned char cmdbuf[8192]={0x19,0};
int iolen;
  
memcpy(cmdbuf+2,ptraw,len);
//Firmware mode: 0 - update, 1 - full flashing
cmdbuf[1]=mode;
//printf("\n");
//dump(cmdbuf,len+2,0);

printf("\nSending the partition table...");
iolen=send_cmd(cmdbuf,len+2,iobuf);

if (iobuf[1] != 0x1a) {
  printf("error!");
  show_errpacket("send_ptable()",iobuf,iolen);
  if (iolen == 0) {
    printf("\nRequires bootloader in silent startup mode\n");
  }
  exit(1);
}  
if (iobuf[2] == 0) {
  printf("ok");
  return;
}  
printf("\nPartition tables do not match - a complete flashing of the modem is required (switch -f)\n");
exit(1);
}

//*******************************************
//* Sending the section header to the loader
//*******************************************
int send_head(char* name) {

unsigned char iobuf[600];
unsigned char cmdbuf[32]={0x1b,0x0e};
int iolen;
  
strcpy(cmdbuf+2,name);
iolen=send_cmd(cmdbuf,strlen(cmdbuf)+1,iobuf);
if (iobuf[1] == 0x1c) return 1;
show_errpacket("send_head()",iobuf,iolen);
return 0; //there was a mistake
}


//*******************************************
//@@@@@@@@@@@@ Head program
void main(int argc, char* argv[]) {
  
unsigned char iobuf[14048];
unsigned char scmd[13068]={0x7,0,0,0};
char ptabraw[4048];
FILE* part;
int ptflag=0;
int listmode=0;

char* fptr;
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
unsigned int i,opt,iolen;
unsigned int adr,len;
unsigned int fsize;
unsigned int forceflag=0;

while ((opt = getopt(argc, argv, "hp:s:w:mk:f")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\nThe utility is designed for writing partitions (according to a table) to a flash modem\n\
The following keys are valid:\n\n\
-p <tty> - specifies the name of the serial port device to communicate with the bootloader\n\
-k # - chipset code (-kl - get a list of codes)\n\
-s <file> - take a partition map from the specified file\n\
-s - - take partition map from file ptable/current-w.bin\n\
-f - complete flashing of the modem with changing the partition map\n\
-w file:part - write a section with the name part from the file file, the section name part is indicated without 0:\n\
-m - only view the firmware map without real recording\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;
    
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'w':
     //defining partitions for recording
     strcpy(iobuf,optarg);
     
     //highlight the file name
     fptr=strchr(iobuf,':');
     if (fptr == 0) {
       printf("\nError in the parameters of the -w key - partition name is not specified: %s\n",optarg);
       return;
     }
     *fptr=0; //filename delimiter
     strcpy(ptable[npart].filename,iobuf); //copy the file name
     fptr++;
     //copy the section name
     strcpy(ptable[npart].partname,"0:");
     strcat(ptable[npart].partname,fptr); 
     npart++;
     if (npart>19) {
       printf("\nToo many sections\n");
       return;
     }
     break;
     
   case 's':
       //load partition table from file
       if (optarg[0] == '-')   part=fopen("ptable/current-w.bin","rb");
       else part=fopen(optarg,"rb");
       if (part == 0) {
         printf("\nError opening partition table file\n");
         return;
       }	 
       fread(ptabraw,1024,1,part); //read partition table from file
       fclose(part);       
       ptflag=1; 
     break;
     
   case 'm':
     listmode=1;
     break;

   case 'f':
     forceflag=1;
     break;
     
   case '?':
   case ':':  
     return;
  }
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
hello(2);
//save the controller configuration
//cfg0=mempeek(nand_cfg0);
//cfg1=mempeek(nand_cfg1);



//firmware table output

printf("\n # --Section-- ------- File -----");     
for(i=0;i<npart;i++) {
    printf("\n%02u  %-14.14s  %s",i,ptable[i].partname,ptable[i].filename);
}
printf("\n");
if (listmode)  return; //switch -m - that's all.
  
printf("\n secure mode...");
if (!secure_mode()) {
  printf("\nError entering Secure mode\n");
//  restore_reg();
  return;
}
printf("ok");
qclose(0);  //####################################################
#ifndef WIN32
  usleep(50000);
#else
  Sleep(50);
#endif
//send the partition table
if (ptflag) send_ptable(ptabraw,16+28*npart,forceflag);

//-- main recording cycle - by sections: --
port_timeout(1000);
for(i=0;i<npart;i++) {
  part=fopen(ptable[i].filename,"rb");
  if (part == 0) {
    printf("\nPartition %u: error opening file %s\n",i,ptable[i].filename);
    return;
  }
  
  //get the partition size
  fseek(part,0,SEEK_END);
  fsize=ftell(part);
  rewind(part);

  printf("\nPartition entry %u (%s)",i,ptable[i].partname); fflush(stdout);
  //send the header
  if (!send_head(ptable[i].partname)) {
    printf("\n! The modem rejected the partition header\n");
    fclose(part);
    return;
  }  
  //cycle of writing sections of a section at 1K per command
  for(adr=0;;adr+=wbsize) {  
    //address
    scmd[0]=7;
    scmd[1]=(adr)&0xff;
    scmd[2]=(adr>>8)&0xff;
    scmd[3]=(adr>>16)&0xff;
    scmd[4]=(adr>>24)&0xff;
    memset(scmd+5,0xff,wbsize+1);   //fill the FF data buffer
    len=fread(scmd+5,1,wbsize,part);
    printf("\r Partition entry %u (%s): byte %u from %u (%i%%)",i,ptable[i].partname,adr,fsize,(adr+1)*100/fsize); fflush(stdout);
    iolen=send_cmd_base(scmd,len+5,iobuf,0);
    if ((iolen == 0) || (iobuf[1] != 8)) {
      show_errpacket("Data package",iobuf,iolen);
      printf("\n Error writing partition %u (%s): address:%06x\n",i,ptable[i].partname,adr);
      fclose(part);
      return;
    }
    if (feof(part)) break; //end of section and end of file
  }
  //The section has been transferred completely
  if (!qclose(1)) {
    printf("\nError closing data stream\n");
    fclose(part);
    return;
  }  
  printf("...recording completed");
#ifndef WIN32
  usleep(500000);
#else
  Sleep(500);
#endif
}
printf("\n");
fclose(part);
}


