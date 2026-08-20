#include "include.h"

//****************************************
//*Loading via Sahara
//****************************************
int dload_sahara() {

FILE* in;
char infilename[200]="loaders/";
unsigned char sendbuf[131072];
unsigned char replybuf[128];
unsigned int iolen,offset,len,donestat,imgid;
unsigned char helloreply[60]={
 02, 00, 00, 00, 48, 00, 00, 00, 02, 00, 00, 00, 01, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00
}; 
unsigned char donemes[8]={5,0,0,0,8,0,0,0};

printf("\nWe are waiting for the Hello packet from the device...\n");
port_timeout(100); //we will wait 10 seconds for the Hello packet
iolen=read(siofd,replybuf,48);  //read Hello
if ((iolen != 48)||(replybuf[0] != 1)) {
	sendbuf[0]=0x3a; //can be any number
	write(siofd,sendbuf,1); //initiate the sending of the Hello packet
	iolen=read(siofd,replybuf,48);  //try to read Hello again
	if ((iolen != 48)||(replybuf[0] != 1)) { //Now that's it - there's nothing more to wait for
		printf("\nHello packet was not received from the device\n");
		dump(replybuf,iolen,0);
		return 1;
	}
}

//Received Hello
ttyflush();  //clearing the receive buffer
port_timeout(10); //now packet exchange will go faster - timeout 1 s
write(siofd,helloreply,48);   //send Hello Response with mode switching
iolen=read(siofd,replybuf,20); //reply packet
  if (iolen == 0) {
    printf("\nNo response from the device\n");
    return 1;
  }  
//replybuf should contain a request for the first block of the loader
imgid=*((unsigned int*)&replybuf[8]); //image id
printf("\nImage ID to download: %08x\n",imgid);
switch (imgid) {

	case 0x07:
	  strcat(infilename,get_nprg());
	break;

	case 0x0d:
	  strcat(infilename,get_enprg());
	break;

	default:
          printf("\nUnknown identifier - no such image!\n");
	return 1;
}
printf("\n Loading %s...\n",infilename); fflush(stdout);
in=fopen(infilename,"rb");
if (in == 0) {
  printf("\nError opening input file %s\n",infilename);
  return 1;
}

//Main bootloader code transfer loop
printf("\n Transfer the bootloader to the device...\n");
while(replybuf[0] != 4) { //EOIT message
 if (replybuf[0] != 3) { //Read Data message
    printf("\nPackage with invalid code - interrupt the download!");
    dump(replybuf,iolen,0);
    fclose(in);
    return 1;
 }
  //select the parameters of the file fragment
  offset=*((unsigned int*)&replybuf[12]);
  len=*((unsigned int*)&replybuf[16]);
//printf("\n address=%08x length=%08x",offset,len);
  fseek(in,offset,SEEK_SET);
  fread(sendbuf,1,len,in);
  //send a block of data to Sahara
  write(siofd,sendbuf,len);
  //we get the answer
  iolen=read(siofd,replybuf,20);      //reply packet
  if (iolen == 0) {
    printf("\nNo response from the device\n");
    fclose(in);
    return 1;
  }  
}
//received EOIT, end of loading
write(siofd,donemes,8);   //send package Done
iolen=read(siofd,replybuf,12); //We are expecting Done Response
if (iolen == 0) {
  printf("\nNo response from the device\n");
  fclose(in);
  return 1;
} 
//we get the status
donestat=*((unsigned int*)&replybuf[12]); 
if (donestat == 0) {
  printf("\nLoader transferred successfully\n");
} else {
  printf("\nLoader transfer error\n");
}
fclose(in);

return donestat;

}

