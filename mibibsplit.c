#include "include.h"

void main(int argc, char* argv[]) {
  
FILE* in;
FILE* out;
unsigned char buf[0x40000];
int i;
int npar;
unsigned int rflag=0,wflag=0,padr=0;
unsigned int sig1,sig2;

if (argc != 2) {
  printf("\nMIBIB file name not specified\n");
  return;
}
in=fopen(argv[1],"rb");
if (in == 0) {
  printf("\nError opening MIBIB file\n");
  return;
}  


//looking for the beginning of the partition table block
while (!feof(in)) {
 fread(&sig1,1,4,in);  
 fread(&sig2,1,4,in);  
 if ((sig1 == 0xfe569fac) && (sig2 == 0xcd7f127a)) break; //partition table block signature found
 fseek(in,504,SEEK_CUR);  //skip the rest of the sector
 padr+=512;
} 
if (feof(in)) {
   printf("\nPartition table block signature not found\n");
   fclose(in);
   return;
}
//now padr is the address of the section block inside the file
//read the entire MIBIB up to this address into the buffer - this will be SBL1 with a tail
fseek(in,0,SEEK_SET);
fread(buf,padr,1,in);  

//looking for the real tail of the section
for(i=padr-1;i>0;i--) {
  if (buf[i] != 0xff) break;
}
out=fopen("sbl1.bin","wb");
fwrite(buf,i+1,1,out);
fclose (out);
  
//Looking for partition table signatures
fseek(in,padr,SEEK_SET);
while(!feof(in)) {
 fread(&sig1,1,4,in);  
 fread(&sig2,1,4,in);  
 if ((!rflag) && (sig1 == 0x55ee73aa) && (sig2 == 0xe35ebddb)) {
   //reading table found
   fseek(in,-8,SEEK_CUR); //we move to the beginning of the signature
   fread(buf,2048,1,in);
   npar=*((unsigned int*)&buf[12]); //number of sections
   out=fopen("ptable-r.bin","wb");
   fwrite(buf,16+28*npar,1,out);
   fclose (out);
   rflag=1;  //we have a reading table
   continue; //skip further processing of the sector
 }
 
 if ((!wflag) && (sig1 == 0xaa7d1b9a) && (sig2 == 0x1f7d48bc)) {
   //record table found
   fseek(in,-8,SEEK_CUR); //we move to the beginning of the signature
   fread(buf,2048,1,in);
   npar=*((unsigned int*)&buf[12]); //number of sections
   out=fopen("ptable-w.bin","wb");
   fwrite(buf,16+28*npar,1,out);
   fclose (out);
   wflag=1;  //we have a reading table
   continue; //skip further processing of the sector
 }
 if (rflag && wflag)
 {
     fclose(in);
     return;   //both tables found
 }
 fseek(in,504,SEEK_CUR);  //skip the rest of the sector
}  
if (!rflag) printf("\nReading table not found\n");
if (!wflag) printf("\nRecord table not found\n");
    fclose(in);
}
