#include "include.h"

//%%%%%%%%% General variables %%%%%%%%%%%%%%%%55

unsigned int fixname=0;   //explicit file name indicator
unsigned int zeroflag=0;  //flag for skipping empty nvram entries
         int sysitem=-1;     //nvram partition number (-1 - all partitions)
char filename[50];        //output file name


//*******************************************
//* Checking partition number boundaries
//*******************************************
void verify_item(int item) {

if (item>0xffff) {
  printf("\n Incorrect cell number - %x\n",item);
  exit(1);
}
}

//*******************************************
//* Receiving the nvram partition from the modem
//*
//* 0 - not read
//* 1 - read
//*******************************************
int get_nvitem(int item, char* buf) {

unsigned char iobuf[140];
unsigned char cmd_rd[16]=  {0x26, 0,0};
unsigned int iolen;

*((unsigned short*)&cmd_rd[1])=item;
iolen=send_cmd_base(cmd_rd,3,iobuf,0);
if (iolen != 136) return 0;
memcpy(buf,iobuf+3,130);
return 1;
}

//*******************************************
//* Dump nvram partition
//*******************************************
void nvdump(int item) {
  
char buf[134];
int len;

if (!get_nvitem(item,buf)) {
  printf("\n! Cell %04x cannot be read\n",item);
  return;
}
if (zeroflag && (test_zero(buf,128) == 0)) {
  printf("\n! Cell %04x is empty\n",item);
  return;
}  
printf("\n *** NVRAM: Cell %04x attribute %04x\n--------------------------------------------------\n",
       item,*((unsigned short*)&buf[128]));

//cut off the trailing zeros
if (zeroflag) {
 for(len=127;len>=0;len--)
   if (buf[len]!=0) break;
 len++;  
}
else len=128;

dump(buf,len,0);
}

//*******************************************
//* Read all NVRAM entries
//*******************************************
void read_all_nvram_items() {
  
char buf[130];
unsigned int nv;
char filename[50];
FILE* out;
unsigned int start=0;
unsigned int end=0x10000;

//Create a directory nv to collect files
if (mkdir("nv",0777) != 0) 
  if (errno != EEXIST) {
    printf("\nCannot create directory nv/");
    return;
  }  

//setting the boundaries of readable records
if (sysitem != -1) {
  start=sysitem;
  end=start+1;
}  
  
printf("\n");  
for(nv=start;nv<end;nv++) {
  printf("\r NV %04x:  ok",nv); fflush(stdout);
  if (!get_nvitem(nv,buf)) continue;
  if (zeroflag && (test_zero(buf,128) == 0)) continue;
  sprintf(filename,"nv/%04x.bin",nv);
  out=fopen(filename,"w");
  fwrite(buf,1,130,out);
  fclose(out);
}  
}  

//*******************************************
//* Write nvram partition from buffer
//*******************************************
int write_item(int item, char*buf) {
  
unsigned char cmdwrite[200]={0x27,0,0};
unsigned char iobuf[200];
int iolen;
int i;

//trim the trailing zeros
for (i=124;i>0;i-=4) {
 if (*((unsigned int*)&buf[i]) != 0) break;
} 
i+=4;

*((unsigned short*)&cmdwrite[1])=item;
memcpy(cmdwrite+3,buf,i);
iolen=send_cmd_base(cmdwrite,i+3,iobuf,0);
if ((iolen != 136) || (iobuf[0] != 0x27)) {
  printf("\n --- Error writing cell %04x ---\n",item);
  dump(iobuf,iolen,0);
  printf("\n------------------------------------------------------\n");
  return 0;
}  
return 1;
}

//*******************************************
//* Write nvram partition from file
//*******************************************
void write_nvram() {
  
FILE* in;
char buf[135];

in=fopen(filename,"r");
if (in == 0) {
  printf("\nError opening file %s\n",filename);
  exit(1);
}
if (fread(buf,1,130,in) != 130) {
  printf("\nFile %s is too small\n",filename);
  exit(1);
}
fclose (in);
write_item(sysitem,buf);
}

//*******************************************
//* Write all nvram partitions
//*******************************************
void write_all_nvram() {

int i;
FILE* in;
char buf[135];

printf("\n");
for (i=0;i<0x10000;i++) {
  sprintf(filename,"nv/%04x.bin",i);
  in=fopen(filename,"r");
  if (in == 0) continue;
  printf("\r %04x: ",i);
  if (fread(buf,1,130,in) != 130) {
    printf("File %s is too small - skip\n",filename);
    fclose (in);
    continue;
  }
  fclose (in);
  if (write_item(i,buf)) printf(" OK"); fflush(stdout);
}
}  
  
//**********************************************
//* Generation nvitem 226 with specified IMEI
//**********************************************
void write_imei(char* src) {

unsigned char binimei[15];
unsigned char imeibuf[0x84];
int i,j,f;  
char cbuf[7];
int csum;

memset(imeibuf,0,0x84);
if (strlen(src) != 15) {
  printf("\nInvalid IMEI length");
  return;
}

for (i=0;i<15;i++) {
  if ((src[i] >= '0') && (src[i] <='9')) {
    binimei[i] = src[i]-'0'; 
    continue;
  }  
  printf("\nInvalid character in IMEI string - %c\n",src[i]);
  return;
}

//Checking the IMEI checksum
j=0;
for (i=1;i<14;i+=2) cbuf[j++]=binimei[i]*2;
csum=0;
for (i=0;i<7;i++) {
  f=(int)cbuf[i]/10;
  csum = csum + f + (int)((cbuf[i]*10) - f*100)/10;
}  
for (i=0;i<13;i+=2) csum += binimei[i];
 
if ((((int)csum/10)*10) == csum) csum=0;
else csum=( (int)csum/10 + 1)*10 - csum;
if (binimei[14] != csum) {
  printf("\nIMEI has an incorrect checksum !\nCorrect IMEI =");
  for (i=0;i<14;i++) printf("%1i",binimei[i]);
  printf("%1i",csum);
  printf("\n Correct (y,n)?");
  i=getchar();
  if (i == 'y') binimei[14]=csum;
}  

//We form IMEI in Kvalkom format
imeibuf[0]=8;
imeibuf[1]=(binimei[0]<<4)|0xa;
j=2;
for (i=1;i<15;i+=2) {
  imeibuf[j++] = binimei[i] | (binimei[i+1] <<4);
}
write_item(0x226,imeibuf);
//dump(imeibuf,9,0);
}
  
  
//@@@@@@@@@@@@@ Main program @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2
void main(int argc, char* argv[]) {

unsigned int opt;
  
enum{
  MODE_BACK_NVRAM,
  MODE_READ_NVRAM,
  MODE_SHOW_NVRAM,
  MODE_WRITE_NVRAM,
  MODE_WRITE_ALL,
  MODE_WRITE_IMEI
}; 

int mode=-1;
char* imeiptr;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:o:b:r:w:j:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\nThe utility is designed to work with the nvram of the modem \n\
%s [switches] [parameter or file name]\n\
The following keys are valid:\n\n\
Keys that determine the operation being performed:\n\
-bn - dump nvram\n\
-ri[z] [item] - read all or only the specified nvram entries into separate files (z-skip empty entries)\n\
-rd[z] - dump the specified nvram partition (z-cut off trailing zeros)\n\n\
-wi item file - write the item section from the file file\n\
-wa - write all partitions in the nv/\n\ directory
-j imei - write the specified IMEI to nv226\n\
\n\
Modifier keys:\n\
-p <tty> - specifies the device name of the modem diagnostic port\n\
-o <file> - file name to save nvram\n\
\n",argv[0]);
    return;
    
   case 'o':
     strcpy(filename,optarg);
     fixname=1;
     break;
//----------------------------------------------	 
   //=== backup key group ==
   case 'b':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line");
       return;
     }  
     switch(*optarg) {
     
       case 'n':
         mode=MODE_BACK_NVRAM;
         break;
	 
       default:
	 printf("\n The value of the -b key is set incorrectly\n");
	 return;
      }
      break;

//----------------------------------------------	 
   //=== key group read ==
   case 'r':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line");
       return;
     }  
     switch(*optarg) {
       case 'i':
	 //rn - read one or all sections into a file
         mode=MODE_READ_NVRAM;
	 if (optarg[1] == 'z') zeroflag=1;
         break;

       case 'd':
         mode=MODE_SHOW_NVRAM;
	 if (optarg[1] == 'z') zeroflag=1;
         break;

       default:
	 printf("\n The value of the -r key is set incorrectly\n");
	 return;
      }
      break;
//----------------------------------------------	 
    //=== key group write ==
    case 'w':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line");
       return;
     }  
     switch(*optarg) {
       case 'i':
         mode=MODE_WRITE_NVRAM;
	 break;

       case 'a':
         mode=MODE_WRITE_ALL;
	 break;

       default:
	 printf("\n The value of the -w key is set incorrectly\n");
	 return;
      }
      break;
	 
//----------------------------------------------	 
//=== IMEI entry ==
    case 'j':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line");
       return;
     }  
     mode=MODE_WRITE_IMEI;
     imeiptr=optarg;
     break;
     
//------------- other keys --------------------
   case 'p':
    strcpy(devname,optarg);
    break;

   case '?':
   case ':':  
     return;
  }
}  

if (mode == -1) {
  printf("\nThe key of the operation being performed is not specified\n");
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


//////////////////

switch (mode) {
    
  case MODE_READ_NVRAM:
    if (optind<argc) {
      //the section number is indicated - select it
      sscanf(argv[optind],"%x",&sysitem);
      verify_item(sysitem);
    }  
    read_all_nvram_items();
    break;

  case MODE_SHOW_NVRAM:
    if (optind>=argc) {
      printf("\nNvram partition number not specified");
      break;
    }
    sscanf(argv[optind],"%x",&sysitem);
    verify_item(sysitem);
    nvdump(sysitem);
    break;

  case MODE_WRITE_NVRAM:
    if (optind != (argc-2)) {
       printf("\nInvalid number of parameters on the command line\n");
       exit(1);
    }   
    sscanf(argv[optind],"%x",&sysitem);
    verify_item(sysitem);
    strcpy(filename,argv[optind+1]);
    write_nvram();
    break;
 
  case MODE_WRITE_ALL:
    write_all_nvram();
    break;
   
  case MODE_WRITE_IMEI:
    write_imei(imeiptr);
    break;
    
  default:
    printf("\nThe key of the operation being performed is not specified\n");
    return;

}    
printf("\n");

}

