#include "include.h"

void main(int argc, char* argv[]) {
  
FILE* in;
unsigned int fl,i,j,daddr,ncmd,tablefound=0,hwidfound=0;
unsigned char buf[1024*512];
unsigned char hwidstr[7]="HW_ID1";
unsigned char hwid[17];
unsigned int header[7];
unsigned int baseaddr=0;
unsigned int codeoffset=0x28;

if (argv[1] == NULL) {
  printf("\nFile not specified\n");
  return;
}
in = fopen(argv[1],"rb");
if (in == NULL) {
  printf("\n Cannot open file %s\n",argv[1]);
  return;
}

fl = fread(&buf,1,sizeof(buf),in);
fclose(in);
printf("\n** %s: %u bytes\n",argv[1],fl);

//Analyzing the bootloader header
memcpy(header,buf,28);
if (header[1] == 0x73d71034) {
  //new ENPRG header format - loaders
  baseaddr=header[6]-header[5];
  codeoffset=0x50;
}
else if (header[1] == 3) {
  //format for NPRG and old ENPRG
  baseaddr=header[3]-0x28;
}
else printf("\n Undefined file header - most likely this is not a loader");
if (baseaddr != 0) { 
  printf("\nDownload address: %08x",baseaddr);
  printf("\n Code start address: %08x",baseaddr+codeoffset);
}

for (i=0;i<fl;i++) {
 if (hwidfound == 0) {
	if (((memcmp(buf+i,hwidstr,6)) == 0) && (buf[i-17] == 0x30)) { //look for the "HW_ID" line in the certificate
		memcpy(hwid,buf+i-17,16); //found, save the value
		hwid[16]=0;
		hwidfound=1; //and we're not looking anymore
	}
 }
 if ((i >= 0x1000) && (i%4 == 0) && (tablefound == 0)) { //the table is aligned to a word boundary and has not yet been processed
	daddr=*((unsigned int*)&buf[i]); //stub address
	if ((daddr<0x10000) || (daddr>0xffff0000)) continue;
	if ( (daddr != *((unsigned int*)&buf[i-4])) && 
	  (daddr == *((unsigned int*)&buf[i+8])) && 
	  (daddr == *((unsigned int*)&buf[i+16])) && 
	  (daddr == *((unsigned int*)&buf[i+24])) && 
	  (daddr == *((unsigned int*)&buf[i+32])) && 
	  (daddr == *((unsigned int*)&buf[i+40])) &&
	  (daddr == *((unsigned int*)&buf[i+48])) &&
	  (daddr == *((unsigned int*)&buf[i+56])) && 
	  (daddr == *((unsigned int*)&buf[i+64])) &&
	  (daddr == *((unsigned int*)&buf[i+72])) &&
	  (daddr == *((unsigned int*)&buf[i+80])) &&
	  (daddr == *((unsigned int*)&buf[i+88]))
	) {
		//table found
		tablefound=1;
		//cycle of selecting commands from the table
		ncmd=0;
//		printf("\n--f--");
		for (j=0;j<0x100;j++) {
			if (*((unsigned int*)&buf[i-4+4*j]) != daddr) {
			if ((*((unsigned int*)&buf[i-4+4*j])&0xfffc0000) != (daddr&0xfffc0000)) break; //this is not an address, the end of the table
			ncmd++;
			printf("\n CMD %02x = %08x",j+1,*((unsigned int*)&buf[i-4+4*j]));
			}
		}  
		if (ncmd != 0) {
			printf("\n CMD table offset: %x",i-4);
			printf("\n Invalid CMD handler: %08x",daddr);
		} else 	printf ("\n No CMD table found. Image is probably not a *PRG.");  
	}
 }
}
 if (hwidfound == 1) {
	printf("\n\n HW_ID = 0x%.16s",hwid);
	printf("\n MSM_ID = 0x%.8s\n OEM_ID = 0x%.4s\n MODEL_ID = 0x%.4s\n",hwid,hwid+8,hwid+12);
} else printf("\n\n Unsigned code or no HW_ID value in Subject field\n");

printf("\n");
}
