#include "include.h"

//send prefix flag 7E
int prefixflag=1;
//HDLC mode flag
int hdlcflag=1; 


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Interactive shell for entering commands into the bootloader
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


//*******************************************************************
//* Sending a command buffer to the modem and saving the result
//*******************************************************************
void iocmd(char* cmdbuf, int cmdlen) {

unsigned char iobuf[2048];
unsigned int iolen;
  
if (hdlcflag) {
  //HDLC mode command
  iolen=send_cmd_base(cmdbuf,cmdlen,iobuf,prefixflag);
  if (iobuf[1] == 0x0e) {
    show_errpacket ("[ERR] ", iobuf, iolen);
    return;
  }  
}
else  {
  write(siofd,cmdbuf,cmdlen);
  iolen=read(siofd,iobuf,1024);
}  
  
if (iolen != 0) {
  printf("\n ---- answer --- \n");
  dump(iobuf,iolen,0);
}  
printf("\n");
}

//*******************************************************************
//* Search for characters other than space in a string
//*
//* zmode - 
//* 0 - look for the first non-space
//* 1 - search for space first, then non-space
//*******************************************************************
char* find_token(char* line, int zmode) {
  
int i=0;

if (zmode) { 
  //looking for a space
  for (i=0; line[i] != ' ' ; i++) {
   if ((line[i] == '\r') || (line [i] == '\n') || (line [i] == 0)) return 0; //logical end of line
  } 
}

for (; line[i] != 0 ; i++) {
  if ((line[i] == '\r') || (line [i] == '\n')) return 0; //logical end of line
  if (line[i] != ' ') return line+i;
}
return 0;
}

//*******************************************************************
//* Parse the entered command sequence and run it
//*******************************************************************
void ascii_cmd(char* line) {

char* sptr;
unsigned char cmdbuf[2048];
int bcnt=0;
int i;

sptr=find_token(line,0); //parsing command bytes separated by spaces
if (sptr == 0) return; //empty command line

do {
  if (*sptr == '\"') {
    //ascii text input mode
    sptr++;
    while(*sptr != '\"') { // before quote
     if ((*sptr == 0) || (*sptr == '\n') || (*sptr == '\r')) {
       printf("\nThe closing quote is missing\n");
       return;
     }  
     cmdbuf[bcnt++]=*sptr++;
    }  
  }
  else {
    // hex byte input mode
    sscanf(sptr,"%x",&i);
    cmdbuf[bcnt++]=i;
  } 
} while ((sptr=find_token(sptr,1)) != 0);
//printf("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
//dump(cmdbuf,bcnt,0);
iocmd(cmdbuf,bcnt);
}

//********************************************************************************
//* Sending the contents of a file to the modem as a command 
//********************************************************************************
void binary_cmd(char* line) {
  
unsigned char cmdbuf[2048];
FILE* fcmd;
unsigned int i;

char* sptr;
sptr=strtok(line," "); // select the file name
if (sptr == 0) {
  printf("File name not specified\n");
  return;
}  
fcmd=fopen(sptr,"r");
if (fcmd == 0) {
  printf("Error opening file %s\n",sptr);
  return;
}
fseek(fcmd,0,SEEK_END);
i=ftell(fcmd);
if (i>1024) {
  printf("The file is too large - %u bytes\n",i);
  fclose(fcmd);
  return;
}
rewind(fcmd);
fread(cmdbuf,i,1,fcmd);
fclose(fcmd);
iocmd(cmdbuf,i);
}


//********************************************************************************
//* Switching HDLC mode
//********************************************************************************
void hdlcswitch(char* line) {

char* sptr;
unsigned int mode;
sptr=strtok(line," "); // select the parameter

if (sptr != 0) { // mode is specified - set it
  sscanf(sptr,"%u",&mode);
  hdlcflag=mode?1:0;
}
printf(" HDLC %s\n",hdlcflag?"On":"Off");
}


//******************************************************
//* Parsing the contents of the CFG0 register
//******************************************************
void decode_cfg0() {
  
unsigned int cfg0=mempeek(nand_cfg0);
printf("\n **** Configuration register 0 *****");
printf("\n * NUM_ADDR_CYCLES              = %x",(cfg0>>27)&7);
printf("\n * SPARE_SIZE_BYTES             = %x",(cfg0>>23)&0xf);
printf("\n * ECC_PARITY_SIZE_BYTES        = %x",(cfg0>>19)&0xf);
printf("\n * UD_SIZE_BYTES                = %x",(cfg0>>9)&0x3ff);
printf("\n * CW_PER_PAGE                  = %x",((cfg0>>6)&7) | ((cfg0>>2)&8));
printf("\n * DISABLE_STATUS_AFTER_WRITE   = %x",(cfg0>>4)&1);
printf("\n * BUSY_TIMEOUT_ERROR_SELECT    = %x",(cfg0)&7);
}

//******************************************************
//* Parsing the contents of the CFG1 register
//******************************************************
void decode_cfg1() {
  
unsigned int cfg1=mempeek(nand_cfg1);
printf("\n **** Configuration register 1 *****");
printf("\n * ECC_MODE                      = %x",(cfg1>>28)&3);
printf("\n * ENABLE_BCH_ECC                = %x",(cfg1>>27)&1);
printf("\n * DISABLE_ECC_RESET_AFTER_OPDONE= %x",(cfg1>>25)&1);
printf("\n * ECC_DECODER_CGC_EN            = %x",(cfg1>>24)&1);
printf("\n * ECC_ENCODER_CGC_EN            = %x",(cfg1>>23)&1);
printf("\n * WR_RD_BSY_GAP                 = %x",(cfg1>>17)&0x3f);
printf("\n * BAD_BLOCK_IN_SPARE_AREA       = %x",(cfg1>>16)&1);
printf("\n * BAD_BLOCK_BYTE_NUM            = %x",(cfg1>>6)&0x3ff);
printf("\n * CS_ACTIVE_BSY                 = %x",(cfg1>>5)&1);
printf("\n * NAND_RECOVERY_CYCLES          = %x",(cfg1>>2)&7);
printf("\n * WIDE_FLASH                    = %x",(cfg1>>1)&1);
printf("\n * ECC_DISABLE                   = %x",(cfg1)&1);
}


//****************************************************************8
//* command processing
//****************************************************************8

void process_command(char* cmdline) {

int adr,len=128,data;
char* sptr;
char membuf[4096];
int block,page,sect;


switch (cmdline[0]) {
  
  //help
  case 'h':
    printf("\nThe following commands are available:\n\n\
c nn nn nn nn.... - formation and launch of a command packet from the listed bytes\n\
@file - run a command package from the specified file\n\
d adr [len] - view the system address space dump\n\
m adr word ... - write words to the specified address\n\r block page sect - read a flash drive block into the sector buffer \n\
s - view the sector buffer dump of the NAND controller\n\n - view the contents of the NAND controller registers\n\
k - parsing the contents of configuration registers\n\
i [s] - launching the HELLO procedure, s - without configuration settings\n\
f [n] - enable(1)/disable(0)/view HDLC mode status\n\
x - exit the program\n\
\n");
    break;
  // processing the command packet
  case 'c':  
   ascii_cmd(cmdline+1);
   break;
 
  case '@':  
   binary_cmd(cmdline+1);
   break;

  case 'f':
    hdlcswitch(cmdline+1);
    break;
    
  // bootloader activation 
  case 'i':
    sptr=strtok(cmdline+1," "); //address
    if ((sptr == 0) || (sptr[0] == 0x0a)) {
      hello(1);
      break;
    }
    if (sptr[0] != 's') {
      printf("\nInvalid parameter in command i");
      break;
    }
    hello(2);
    break;
    
  // memory dump
  case 'd':  
   sptr=strtok(cmdline+1," "); //address
   if (sptr == 0) {printf("\nAddress not specified"); return;}
   sscanf(sptr,"%x",&adr);
   sptr=strtok(0," "); // length
   if (sptr != 0) sscanf(sptr,"%x",&len);
   if (memread(membuf,adr,len)) dump(membuf,len,adr); 
   break;

  case 'm':
   sptr=strtok(cmdline+1," "); //address
   if (sptr == 0) {printf("\nAddress not specified"); return;}
   sscanf(sptr,"%x",&adr);
   while((sptr=strtok(0," ")) != 0) { // data
     sscanf(sptr,"%x",&data);
     if (!mempoke(adr,data)) printf("\nThe command returned an error, adr=%08x data=%08x\n",adr,data);
     adr+=4;
   }
   break;

  case 'r':
   hello(0);
   sptr=strtok(cmdline+1," "); // block
   if (sptr == 0) {printf("\n Block # not specified"); return;}
   sscanf(sptr,"%x",&block);

   sptr=strtok(0," ");        // page
   if (sptr == 0) {printf("\nNo page # specified"); return;}
   sscanf(sptr,"%x",&page);
   if (page>63)  {printf("\nPage # too big"); return;}
   
   sptr=strtok(0," ");        // sector
   if (sptr == 0) {printf("\nSector # not specified"); return;}
   sscanf(sptr,"%x",&sect);
   if (sect>spp-1)  {printf("\nSector # too large"); return;}
   
   if (!flash_read(block,page,sect)) printf("\n    *** badblock ***\n");
   memread(membuf,sector_buf,0x23c);
   dump(membuf,0x23c,0); 
   break;
   
   
  case 's': 
   hello(0);
   memread(membuf,sector_buf,0x23c);
   dump(membuf,0x23c,0); 
   break;

  case 'n':
   hello(0);
   if (is_chipset("MDM9x4x")) {
   memread(membuf,nand_cmd,0x1c);
   memread(membuf+0x20,nand_cmd+0x20,0x0c);
   memread(membuf+0x40,nand_cmd+0x40,0x0c);
//   memread(membuf+0x64,nand_cmd+0x64,4);
//   memread(membuf+0x70,nand_cmd+0x70,0x1c);
//   memread(membuf+0xa0,nand_cmd+0xa0,0x10);
//   memread(membuf+0xd0,nand_cmd+0xd0,0x10);
   memread(membuf+0xe8,nand_cmd+0xe8,0x0c);
   } else memread(membuf,nand_cmd,0x100);

   printf("\n* 000 NAND_FLASH_CMD         = %08x",*((unsigned int*)&membuf[0]));
   printf("\n* 004 NAND_ADDR0             = %08x",*((unsigned int*)&membuf[4]));
   printf("\n* 008 NAND_ADDR1             = %08x",*((unsigned int*)&membuf[8]));
   printf("\n* 00c NAND_CHIP_SELECT       = %08x",*((unsigned int*)&membuf[0xc]));
   printf("\n* 010 NANDC_EXEC_CMD         = %08x",*((unsigned int*)&membuf[0x10]));
   printf("\n* 014 NAND_FLASH_STATUS      = %08x",*((unsigned int*)&membuf[0x14]));
   printf("\n* 018 NANDC_BUFFER_STATUS    = %08x",*((unsigned int*)&membuf[0x18]));
   printf("\n* 020 NAND_DEV0_CFG0         = %08x",*((unsigned int*)&membuf[0x20]));
   printf("\n* 024 NAND_DEV0_CFG1         = %08x",*((unsigned int*)&membuf[0x24]));
   printf("\n* 028 NAND_DEV0_ECC_CFG      = %08x",*((unsigned int*)&membuf[0x28]));
   printf("\n* 040 NAND_FLASH_READ_ID     = %08x",*((unsigned int*)&membuf[0x40]));
   printf("\n* 044 NAND_FLASH_READ_STATUS = %08x",*((unsigned int*)&membuf[0x44]));
   printf("\n* 048 NAND_FLASH_READ_ID2    = %08x",*((unsigned int*)&membuf[0x48]));
   if (!(is_chipset("MDM9x4x"))) {
		printf("\n* 064 FLASH_MACRO1_REG       = %08x",*((unsigned int*)&membuf[0x64]));
		printf("\n* 070 FLASH_XFR_STEP1        = %08x",*((unsigned int*)&membuf[0x70]));
		printf("\n* 074 FLASH_XFR_STEP2        = %08x",*((unsigned int*)&membuf[0x74]));
		printf("\n* 078 FLASH_XFR_STEP3        = %08x",*((unsigned int*)&membuf[0x78]));
		printf("\n* 07c FLASH_XFR_STEP4        = %08x",*((unsigned int*)&membuf[0x7c]));
		printf("\n* 080 FLASH_XFR_STEP5        = %08x",*((unsigned int*)&membuf[0x80]));
		printf("\n* 084 FLASH_XFR_STEP6        = %08x",*((unsigned int*)&membuf[0x84]));
		printf("\n* 088 FLASH_XFR_STEP7        = %08x",*((unsigned int*)&membuf[0x88]));
		printf("\n* 0a0 FLASH_DEV_CMD0         = %08x",*((unsigned int*)&membuf[0xa0]));
		printf("\n* 0a4 FLASH_DEV_CMD1         = %08x",*((unsigned int*)&membuf[0xa4]));
		printf("\n* 0a8 FLASH_DEV_CMD2         = %08x",*((unsigned int*)&membuf[0xa8]));
		printf("\n* 0ac FLASH_DEV_CMD_VLD      = %08x",*((unsigned int*)&membuf[0xac]));
		printf("\n* 0d0 FLASH_DEV_CMD3         = %08x",*((unsigned int*)&membuf[0xd0]));
		printf("\n* 0d4 FLASH_DEV_CMD4         = %08x",*((unsigned int*)&membuf[0xd4]));
		printf("\n* 0d8 FLASH_DEV_CMD5         = %08x",*((unsigned int*)&membuf[0xd8]));
		printf("\n* 0dc FLASH_DEV_CMD6         = %08x",*((unsigned int*)&membuf[0xdc]));
   }
   printf("\n* 0e8 NAND_ERASED_CW_DET_CFG = %08x",*((unsigned int*)&membuf[0xe8]));
   printf("\n* 0ec NAND_ERASED_CW_DET_ST  = %08x",*((unsigned int*)&membuf[0xec]));
   printf("\n* 0f0 EBI2_ECC_BUF_CFG       = %08x\n",*((unsigned int*)&membuf[0xf0]));
   break;
  case 'x': 
   exit(0);
   break;

  case 'k':
    hello(0);
    decode_cfg0();
    printf("\n");
    decode_cfg1();
    printf("\n");
    break;
   
  default:
    printf("\nUndefined command\n");
    break;
}   
}    
    
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc,char* argv[]) {
  
#ifndef WIN32
char* line;
char oldcmdline[1024]="";
#else
char line[1024];
#endif
char scmdline[1024]={0};
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
int opt,helloflag=0;

while ((opt = getopt(argc, argv, "p:ic:hek:f")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\nInteractive shell for entering commands into the bootloader\n\n\
The following keys are valid:\n\n\
-p <tty> - specifies the name of the serial port device to communicate with the bootloader\n\
-i - runs the HELLO procedure to initialize the bootloader\n\
-e - disables passing the 7E prefix before the command\n\
-f - disables HDLC formatting of command packets\n\
-k # - chipset code (-kl - get a list of codes)\n\
-c \"<command>\" - runs the specified command and exits\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'f':
     hdlcflag=0;
     break;
    
   case 'k':
    define_chipset(optarg);
    break;
    
   case 'i':
     helloflag=1;
     break;
     
   case 'e':
     prefixflag=0;
     break;
     
   case 'c':
     strcpy(scmdline,optarg);
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
if (helloflag) hello(1);
printf("\n");

//run the command from the -C switch, if available
if (strlen(scmdline) != 0) {
  process_command(scmdline);
  return;
}
 
//Main command loop
#ifndef WIN32
 //loading command history
 read_history("qcommand.history");
 write_history("qcommand.history");
#endif 

for(;;)  {
#ifndef WIN32
 line=readline(">");
#else
 printf(">");
 fgets(line, sizeof(line), stdin);
#endif
 if (line == 0) {
    printf("\n");
    return;
 }   
 if (strlen(line) <1) continue; //command too short
#ifndef WIN32
 if (strcmp(line,oldcmdline) != 0) {
   add_history(line); //buffer it for history
   append_history(1,"qcommand.history");
   strcpy(oldcmdline,line);
 }  
#endif
 process_command(line);
#ifndef WIN32
 free(line);
#endif
} 
}
