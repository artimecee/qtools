#include "include.h"

//Setting flash controller parameters
//

void main(int argc, char* argv[]) {
  
#ifndef WIN32
char devname[20]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif

//local settings for installation
int lud=-1, lecc=-1, lspare=-1, lbad=-1;
int sflag=0;
int opt;
int badloc;

while ((opt = getopt(argc, argv, "hp:s:u:e:d:")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\nThe utility is designed to set NAND controller parameters\n\n\
The following keys are valid:\n\n\
-p <tty> - specifies the name of the serial port device to communicate with the bootloader\n\
-s nnn - set the size of the spare field per sector\n\
-u nnn - set the size of the sector data field\n\
-e nnn - set the ECC field size per sector\n\
-d [L]xxx- set the bad block marker to byte xxx (hex), L=U(user) or S(spare)\n\");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 's':
     sscanf(optarg,"%d",&lspare);
     sflag=1;
     break;

   case 'u':
     sscanf(optarg,"%d",&lud);
     sflag=1;
     break;

   case 'e':
     sscanf(optarg,"%d",&lecc);
     sflag=1;
     break;
    
   case 'd':
     parse_badblock_arg(optarg, &lbad, &badloc);
     sflag=1;
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

if (!sflag) {
 hello(1);
 return;
}

hello(0);

if (lspare != -1) set_sparesize(lspare);
if (lud != -1) set_udsize(lud);
if (lecc != -1) set_eccsize(lecc);
if (lbad != -1) set_badmark_pos (lbad, badloc);
} 
