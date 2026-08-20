#include "include.h"

//--------------------------------------------------------------------------------
//* Terminal program for working with modem command ports
//--------------------------------------------------------------------------------

unsigned int hexflag=0;         //hex flag
unsigned int wrapperlen=0;      //line size (0 - no line wrapping)
unsigned int waittime=1;        //response waiting time
unsigned int monitorflag=0;     //monitor mode
unsigned int autoflag=1;        //AT auto-adding mode
char outcmd[500];
char ibuf[6000];

//*****************************************************
//* Receive and receive modem response
//*****************************************************
void read_responce() {

int i;
int dlen=0;  //full response length in buffer
int rlen;    //length of the received part of the response
int clen;    //the length of the response fragment is the width of the terminal line

//cycle of receiving parts of the response and assembling the response into a single buffer

do {
//usleep(waittime*100); // delay waiting for a response - not needed, termios handles it itself
  rlen=read(siofd,ibuf+dlen,5000);   //command response
  if ((dlen+rlen) >= 5000) break; //buffer overflow
  dlen+=rlen;
} while (rlen != 0);
  
  
if (dlen == 0) return; //no answer

//The answer is received - display it on the screen
if (hexflag) {
  printf("\n");rlen=-1;
  dump(ibuf,dlen,0);
  printf("\n");
}
else {
  ibuf[dlen]=0; //end of line
  printf("\n");
  if (wrapperlen == 0) puts(ibuf);
  else {
    clen=wrapperlen;
    for(i=0;i<dlen;i+=wrapperlen) {
       if ((dlen-i) < wrapperlen) clen=dlen-i; //last line length
       fwrite(ibuf+i,1,clen,stdout);
       printf("\n");
       fflush(stdout);
    }
  }
}  
fflush(stdout);
}

//*****************************************************
//* Sending a command to the modem and receiving the result *
//*****************************************************
void process_command(char* cmdline) {

outcmd[0]=0;

//auto-adding AT prefix
if ( autoflag &&
    (((cmdline[0] != 'a') && (cmdline[0] != 'A')) ||
    ((cmdline[1] != 't') && (cmdline[1] != 'T') && (cmdline[1] != '/'))) 
   )  strcpy(outcmd,"AT");
strcat(outcmd,cmdline);
strcat(outcmd,"\r");   //add CR to the end of the line

//sending a command
ttyflush();  //clearing the output buffer
write(siofd,outcmd,strlen(outcmd));  //sending a command
// 
read_responce();
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc,char* argv[]) {
  
#ifndef WIN32
char* line;
char oldcmdline[200]="";
#else
char line[200];
#endif
char scmdline[200]={0};
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
int opt;

while ((opt = getopt(argc, argv, "p:xw:c:hd:ma")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\nTerminal program for entering AT commands into the modem\n\n\
The following keys are valid:\n\n\
-p <tty> - specifies the serial port device name\n\
-d <time> - sets the waiting time for the modem response in ms\n\
-x - displays the modem response as a HEX dump\n\
-w <len> - line length in long string wrapping mode (0 - no wrapping)\n\
-m - port monitor mode\n\
-a - disable auto-adding of AT letters at the beginning of the command\n\
-c \"<command>\" - runs the specified command and exits\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'c':
     strcpy(scmdline,optarg);
     break;

   case 'd':
     sscanf(optarg,"%i",&waittime);
     break;
     
   case 'w':
     sscanf(optarg,"%i",&wrapperlen);
     break;

   case 'x':
     hexflag=1;
     break;
     
   case 'a':
     autoflag=0;
     break;
     
   case 'm':
     monitorflag=1;
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

//setting port timeout
port_timeout(waittime);

//monitor mode
if (monitorflag) 
  for (;;) read_responce();

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
 if (strlen(line) == 0) continue; //empty command
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
