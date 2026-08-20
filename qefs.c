#ifdef WIN32
#define _USE_32BIT_TIME_T
#endif
#include "include.h"
#include <time.h>
#include "efsio.h"

//%%%%%%%%% Common variables %%%%%%%%%%%%%%%%%

unsigned int fixname=0;   //explicit file name indicator
char filename[50];        //output file name

char* fbuf;  //buffer for file operations

int recurseflag=0;
int fullpathflag=0;

char iobuf[44096];
int iolen;

#ifdef WIN32
struct tm* localtime_r(const time_t *clock, struct tm *result) {
       if (!clock || !result) return NULL;
       memcpy(result,localtime(clock),sizeof(*result));
       return result;
}
#endif

//file list output modes:

enum {
  fl_tree,    //tree
  fl_ftree,   //tree with files
  fl_list,    //file listing
  fl_full,    //full file listing
  fl_mid      //listing of files in MC extfs format
};  

  
int tspace; //indentation for forming a file tree


//****************************************************
//* Read EFS dump (efs.mbn)
//****************************************************

void back_efs() {


FILE* out;
struct efs_factimage_rsp rsp;
rsp.stream_state=0;
rsp.info_cluster_sent=0;
rsp.cluster_map_seqno=0;
rsp.cluster_data_seqno=0;

strcpy(filename,"efs.mbn");
out=fopen(filename,"w");
if (out == 0) {
  printf("\nError opening output file %s\n",filename);
  return;
}  

if (efs_prep_factimage() != 0) {
  printf("\nError entering Factory Image mode, code %d\n",efs_get_errno());
  fclose(out);
  return;
}

if (efs_factimage_start() != 0) {
  printf("\nError starting EFS read, code %d\n",efs_get_errno());
  fclose(out);
  return;
}

printf("\n");

//main loop for getting efs.mbn
while(1) {
  printf("\r Read: sent=%i map=%i data=%i",rsp.info_cluster_sent,rsp.cluster_map_seqno,rsp.cluster_data_seqno);
  fflush(stdout);
  if (efs_factimage_read(rsp.stream_state, rsp.info_cluster_sent, rsp.cluster_map_seqno, 
                    rsp.cluster_data_seqno, &rsp) != 0) {  
    printf("\nReading error, code=%d\n",efs_get_errno());
    return;
  }
  if (rsp.stream_state == 0) break; //end of data stream
  fwrite(rsp.page,512,1,out);
}
//close EFS
efs_factimage_end();
fclose(out);
  
}

//***************************************************
//* Output the file name taking into account the tree indentation
//***************************************************
void printspace(char* name) {

int i;
printf("\n");
if (tspace != 0) for (i=0;i<tspace*3;i++) printf(" ");
printf("%s",name);
}

//*********************************************
//* Output file access attribute
//*********************************************
static char atrstr[15];

void fattr(int mode, char* str) {
  
memset(str,'-',3);
str[3]=0;
if ((mode&4) != 0) str[0]='r';
if ((mode&2) != 0) str[1]='w';
if ((mode&1) != 0) str[2]='x';
}

//*********************************************
char* cfattr(int mode) {

char str[5];
  
atrstr[0]=0;
fattr((mode>>6)&7,str);
strcat(atrstr,str);
fattr((mode>>3)&7,str);
strcat(atrstr,str);
fattr(mode&7,str);
strcat(atrstr,str);
return atrstr;
}

//****************************************************
//* Retrieving a symbolic description of a file attribute
//****************************************************
char* str_filetype(int attr,char* buf) {
  
strcpy(buf,"Unknown");
     if ((attr&S_IFMT) == S_IFIFO)  strcpy(buf,"fifo");
else if ((attr&S_IFMT) == S_IFCHR)  strcpy(buf,"Character device");
else if ((attr&S_IFMT) == S_IFDIR)  strcpy(buf,"Directory");
else if ((attr&S_IFMT) == S_IFBLK)  strcpy(buf,"Block device");
else if ((attr&S_IFMT) == S_IFREG)  strcpy(buf,"Regular file");
else if ((attr&S_IFMT) == S_IFLNK)  strcpy(buf,"Symlink");
else if ((attr&S_IFMT) == S_IFSOCK) strcpy(buf,"Socket");
else if ((attr&S_IFMT) == S_IFITM)  strcpy(buf,"Item File");
return buf;
}

//****************************************************
//* Retrieving a one-character description of a file attribute
//****************************************************
char chr_filetype(int attr) {
  
     if ((attr&S_IFMT) == S_IFIFO)   return 'p';
else if ((attr&S_IFMT) == S_IFCHR)   return 'c';
else if ((attr&S_IFMT) == S_IFDIR)   return 'd';
else if ((attr&S_IFMT) == S_IFBLK)   return 'b';
else if ((attr&S_IFMT) == S_IFREG)   return '-';
else if ((attr&S_IFMT) == S_IFLNK)   return 'l';
else if ((attr&S_IFMT) == S_IFSOCK)  return 's';
else if ((attr&S_IFMT) == S_IFITM)   return 'i';
return '-';
}

//****************************************************
//* Convert date to ascii string
//*Format:
//* 0 - normal
//* 1 - for Midnight Commander
//****************************************************
char* time_to_ascii(int32 time, int format) {
  
time_t xtime;      //the same time, only time_t bit depth
struct tm lt;      //structure for storing converted date
static char timestr[100];

xtime=time;
if (localtime_r(&xtime,&lt) != 0)  {
 if (format == 0) strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&xtime));
 else             strftime(timestr,100,"%m-%d-%y %H:%M",localtime(&xtime));
}
else strcpy(timestr,"---------------");
return timestr;
}

//****************************************************
//* Display detailed information about a regular file
//****************************************************
void show_efs_filestat(char* filename, struct efs_filestat* fi) {
  
char sfbuf[50]; //buffer for storing file type description

printf("\nFile name: %s",filename);
printf("\n Size: %i bytes",fi->size);
printf("\nFile type: %s",str_filetype(fi->mode,sfbuf));
printf("\nReference count: %d",fi->nlink);
printf("\nAccess attributes: %s",cfattr(fi->mode));
printf("\nCreation date: %s",time_to_ascii(fi->ctime,0));
printf("\nModification date: %s",time_to_ascii(fi->mtime,0));
printf("\nDate of last access: %s\n",time_to_ascii(fi->atime,0));
}


//****************************************************
//* Display the file tree of the specified directory
//* lmode - fl_* output mode:
//* fl_tree - tree
//* fl_ftree, - tree with files

//* fname - initial path, default /
//****************************************************
void show_tree (int lmode, char* fname) {
  
struct efs_dirent dentry; //directory item handle
unsigned char dirname[100];	
int dirp=0;  //pointer to open directory

int i,nfile;
char targetname[200];

if (strlen(fname) == 0) strcpy(dirname,"/"); //By default we open the root directory
else strcpy(dirname,fname);

// chdir
dirp=efs_opendir(dirname);
if (dirp == 0) {
  printf("\n ! Access to directory %s is denied, errno=%i\n",dirname,efs_get_errno());
  return;
}

//Directory Entry Fetch Loop
for(nfile=1;;nfile++) {
 //select the next entry
 if (efs_readdir(dirp, nfile, &dentry) == -1) continue; //when there is an error reading the next structure
 if (dentry.name[0] == 0) break;   //end of list

 //Determining the full file name
   strcpy(targetname,dirname);
//   strcat(targetname,"/");
   strcat(targetname,dentry.name); //skip the initial "/"
   if(dentry.entry_type == 1) strcat (targetname,"/"); //this is a directory
 
   if ((lmode == fl_tree) && (dentry.entry_type != 1)) continue; //skip regular files in directory tree mode

   if (fullpathflag) printspace(targetname);
   else {
     for(i=strlen(targetname)-2;i>=0;i--) {
       if (targetname[i] == '/') break;
     } 
     i++;
     printspace(targetname+i);
   }  
   if (dentry.entry_type == 1) {
     //this entry is a directory - we process the nested subdirectory
     tspace++;
     efs_closedir(dirp);
     show_tree(lmode,targetname); 
     dirp=efs_opendir(dirname);
     tspace--;
   }  
 }
efs_closedir(dirp); 
}

//****************************************************
//* List the files in the specified directory
//* lmode - output mode fl_*
//* fl_list - short listing of files
//* fl_full - full file listing
//* fl_mid - full listing of files in midnight commander format
//* fname - initial path, default /
//****************************************************
void show_files (int lmode, char* fname) {
  
struct efs_dirent dentry; //directory item handle
char dnlist[200][100]; //directory list
unsigned short ndir=0;
unsigned char dirname[100];	
int dirp=0;  //pointer to open directory

int i,nfile;
char ftype;
char targetname[200];

if (strlen(fname) == 0) strcpy(dirname,"/"); //By default we open the root directory
else strcpy(dirname,fname);

// opendir
dirp=efs_opendir(dirname);
if (dirp == 0) {
  if (lmode != fl_mid) printf("\n ! Access to directory %s is denied, errno=%i\n",dirname,efs_get_errno());
//printf("\n ! Access to directory %s is denied\n",dirname);
  return;
}
if (lmode == fl_full) printf("\n *** Directory %s ***\n",dirname);
//file search
for(nfile=1;;nfile++) {
 //select the next entry
 if (efs_readdir(dirp, nfile, &dentry) == -1) {
   continue; //when there is an error reading the next structure
 }  
 if (dentry.name[0] == 0) {
   break;   //end of list
 }  
 ftype=chr_filetype(dentry.mode);
 if ((dentry.entry_type) == 1) { 
   //Generating a list of subdirectories
   strcpy(dnlist[ndir++],dentry.name);
 }  

 //Determining the full file name
   strcpy(targetname,dirname);
//   strcat(targetname,"/");
   strcat(targetname,dentry.name); //skip the initial "/"
   if(ftype == 'd') strcat (targetname,"/");
 
 
 //simple file list mode
 if (lmode == fl_list) {
   printf("\n%s",targetname);
   if ((ftype == 'd') && (recurseflag == 1)) { 
     show_files(lmode,targetname);
   } 
   continue;
 }
 
 //full file list mode
if (lmode == fl_full) 
  printf ("%c%s %9i %s %s\n",
      ftype,
      cfattr(dentry.mode),
      dentry.size,
      time_to_ascii(dentry.mtime,0),
      dentry.name);

//midnight commander mode
  
if (lmode == fl_mid) {
  if (ftype == 'i') ftype='-';
  printf("%c%s",
      ftype,                          // attr
      cfattr(dentry.mode));           // mode

  printf(" %d root root",ftype == 'd'?2:1);     // nlink, owner
  printf(" %9d %s %s/%s\n", 
	 dentry.size,                   // size
      time_to_ascii(dentry.mtime,1),      // date
      dirname,	 
      dentry.name);                     // name
}
}
//this directory has been processed - we process nested subdirectories in full view mode

efs_closedir(dirp);  
if (lmode == fl_full) printf("\n * Files: %i\n",nfile);
if (((lmode == fl_full) && recurseflag) || (lmode == fl_mid)) {
   for(i=0;i<ndir;i++) {
    strcpy(targetname,dirname);
    strcat(targetname,"/");
    strcat(targetname,dnlist[i]);
    show_files(lmode,targetname);
   }
}  

}

  
  
//**************************************************   
//* Reading a file into a buffer
//**************************************************   
unsigned int readfile(char* filename) {	

struct efs_filestat fi;
int i,blk;
int fd;

efs_close(1);
switch (efs_stat(filename,&fi)) {
   case 0:
     printf("\nObject %s not found\n",filename);
     return 0;
 
   case 2: //catalog
     printf("\nObject %s is a directory\n",filename);
     return 0;
}    
if (fi.size == 0) {
  printf("\nFile %s does not contain data\n",filename);
  return 0;
}
fbuf=malloc(fi.size);
fd=efs_open(filename,O_RDONLY);
if (fd == -1) {
  printf("\nError opening file %s",filename);
  return 0;
}

blk=512;
for (i=0;i<(fi.size);i+=512) {
 if ((i+512) > fi.size) blk=fi.size-i;
 if (efs_read(fd, fbuf+i, blk, i)<=0) 
   return 0; //reading error
}
efs_close(fd);
return fi.size;
}

/////////////////////////////////////////////////////////////////
//**************************************************   
//* Write file
//**************************************************   
unsigned int write_file(char* file, char* path) {	

struct efs_filestat fi;
int i,blk;
FILE* in;
long filesize;
int fd;

efs_close(1);

//prepare the output file name

strcpy(filename,path);

//check the existence of the file

switch (efs_stat(filename,&fi)) {
   case 1:
     printf("\nObject %s already exists\n",filename);
     return 0;
 
   case 2: //catalog
     strcat(filename,"/");
     strcat(filename,file);
}    

//read file into buffer
in=fopen(file,"r");
if (in == 0) {
  printf("\nerror opening file %s",file);
  return 0;
}  
fseek(in,0,SEEK_END);
filesize=ftell(in);
fbuf=malloc(filesize);
fseek(in,0,SEEK_SET);
fread(fbuf,1,filesize,in);
fclose(in);

fd=efs_open(filename,O_CREAT);
if (fd == -1) {
  printf("\nError opening file %s for writing",filename);
  return 0;
}

blk=512;
for (i=0;i<(filesize);i+=512) {
 if ((i+512) > filesize) {
   blk=filesize-i;
 }
 efs_write(fd, fbuf+i, blk, i);
 usleep(3000);
}
free(fbuf);
efs_close(fd);
return 1;
}


//***************************************
//* View file on screen
//*
//* mode=0 - view as text
//* 1 - view as a dump
//***************************************
void list_file(char* filename,int mode) {
  
unsigned int flen;

flen=readfile(filename);
if (flen == 0) {
  printf("Error reading file %s",filename);
  return;
}  
if (!mode) fwrite(fbuf,flen,1,stdout);
else dump(fbuf,flen,0);
free(fbuf);
}

//******************************************************
//* Copy a single file from EFS to the current directory
//******************************************************
void get_file(char* name, char* dst) {
  
unsigned int flen;
char* fnpos;
FILE* out;
struct stat fs;
char filename[200];

flen=readfile(name);
if (flen == 0) {
  printf("Error reading file %s",filename);
  return;
}  

//extract the file name from the full path
fnpos=strrchr(name,'/');
if (fnpos == 0) fnpos=name;
else fnpos++;

if (dst == 0) {
  //no output directory or file specified
  strcpy(filename,fnpos);
}
else {
  //output directory or file specified
  if ((stat(dst,&fs) == 0) && S_ISDIR(fs.st_mode)) {
    //the output file is a directory
    strcpy(filename,dst);
    strcat(filename,"/");
    strcat(filename,fnpos);
  }
  //output file does not exist or is a regular file
  else strcpy(filename,dst);
}      
out=fopen(filename,"w");
if (out == 0) {
  printf("Error opening output file\n");
  exit(1);
}  
fwrite(fbuf,1,flen,out);
fclose(out);
}



//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@ Head program
void main(int argc, char* argv[]) {

unsigned int opt;
int i;
struct efs_filestat fi;
char filename[100];
  
enum{
  MODE_BACK_EFS,
  MODE_FILELIST,
  MODE_TYPE,
  MODE_GETFILE,
  MODE_WRITEFILE,
  MODE_DELFILE,
  MODE_MKDIR
}; 


enum {
  T_TEXT,
  T_DUMP
};  

enum {
  G_FILE,
  G_ALL,
  G_DIR
};  

int mode=-1;
int lmode=-1;
int tmode=-1;
int gmode=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:o:ab:g:l:rt:w:e:fm:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\nThe utility is designed to work with the efs partition \n\
%s [switches] [path or file name] [output file name]\n\
The following keys are valid:\n\n\
* Keys that determine the operation being performed:\n\
-be - efs dump\n\n\
-ld - show EFS directory tree (without regular files)\n\
-lt - show the EFS directory and file tree\n\
-ll - show a short list of files in the specified directory\n\
-lf - show a complete list of files in the specified directory\n (For all -l switches, you can specify the initial path to the directory)\n\n\
-tt - view the file in text form\n\
-td - view the file as a dump\n\n\
-gf file [dst] - reads the specified file from EFS to the dst file or to the current directory\n\
-wf file path - writes the specified file to the specified path\n\
-ef file - deletes the specified file\n\
-ed dir - deletes the specified directory\n\
-md dir - creates a directory with the specified access rights\n\n\
* Modifier keys:\n\
-r - processing of all subdirectories when listing listing\n\
-f - output the full path to each directory when browsing the tree\n\
-p <tty> - specifies the device name of the modem diagnostic port\n\
-a - use alternative EFS\n\
-o <file> - file name to save efs\n\
\n",argv[0]);
    return;
    
   case 'o':
     strcpy(filename,optarg);
     fixname=1;
     break;
   //=== backup key group ==
   case 'b':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line\n");
       return;
     }  
     switch(*optarg) {
       case 'e':
         mode=MODE_BACK_EFS;
         break;
	 
       default:
	 printf("\n The value of the -b key is set incorrectly\n");
	 return;
      }
      break;

   //List of files
   case 'l':   
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line\n");
       return;
     }  
     mode=MODE_FILELIST;
     switch(*optarg) {
       case 'd':
         lmode=fl_tree;
         break;
       case 't':
         lmode=fl_ftree;
         break;
       case 'l':
         lmode=fl_list;
         break;
       case 'f':
         lmode=fl_full;
         break;
       case 'm':
         lmode=fl_mid;
         break;
       default:
	 printf("\n The value of the -l key is set incorrectly\n");
	 return;
     }  
     break;

   //=== file view key group

    case 't':   
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line\n");
       return;
     }  
     mode=MODE_TYPE;
     switch(*optarg) {
       case 't':
         tmode=T_TEXT;
         break;
       
       case 'd':
         tmode=T_DUMP;
         break;
       
       default:
	 printf("\n The value of the -t key is set incorrectly\n");
	 return;
      }
     break; 

  //=== file retrieval key group (get) ==
   case 'g':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line\n");
       return;
     }  
     mode=MODE_GETFILE;
     switch(*optarg) {
       case 'f':
	 gmode=G_FILE;
	 break;
	 
       default:
	 printf("\n The value of the -g key is set incorrectly\n");
	 return;
      }
      break;
      
  //=== file write key group (write) ==
   case 'w':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line\n");
       return;
     }  
     mode=MODE_WRITEFILE;
     switch(*optarg) {
       case 'f':
	 gmode=G_FILE;
	 break;
	 
       default:
	 printf("\n The value of the -g key is set incorrectly\n");
	 return;
      }
      break;      

  //=== file deletion key group (erase) ==
   case 'e':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line\n");
       return;
     }  
     mode=MODE_DELFILE;
     switch(*optarg) {
       case 'f':
	 gmode=G_FILE;
	 break;
	 
       case 'd':
	 gmode=G_DIR;
	 break;
	 
       default:
	 printf("\n The value of the -g key is set incorrectly\n");
	 return;
      }
      break;      

  //==== Directory creation key ====
   case 'm':
     if (mode != -1) {
       printf("\nMore than 1 operating mode key is specified on the command line\n");
       return;
     }  
     mode=MODE_MKDIR;
     if (*optarg != 'd') {
       printf("\nInvalid key m%c",*optarg);
       return;
     }
     break;
   
 //===================== Auxiliary keys ====================
      
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'a':
     set_altflag(1);
     break;
     
   case 'r':
     recurseflag=1;
     break;
     
   case 'f':
     fullpathflag=1;
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

//Closing all open directory handles

for(i=1;i<10;i++) efs_closedir(i);

//Launching the necessary procedures depending on the operating mode

switch (mode) {

//============================================================================  
//EFS dump
  case MODE_BACK_EFS:
    back_efs();
    break;

//============================================================================  
//browsing the catalog
  case MODE_FILELIST:
    tspace=0;
    //the path is not specified - we work with the root directory
    if (optind == argc)    strcpy(filename,"/");
    //the path is indicated
    else strcpy(filename,argv[optind]);
    //Checking the presence of the file and whether it is a directory
    i=efs_stat(filename,&fi);
    switch (i) {
      case 0:
        printf("\nObject %s not found\n",filename);
        break;
    
      case 1: //regular file
        show_efs_filestat(filename,&fi);
        break;
	
      case 2: //catalog
        if ((lmode == fl_tree) || (lmode == fl_ftree)) show_tree(lmode,filename);
	else show_files(lmode,filename);
	break;
	
      case -1: //error
	printf("\nObject %s is unavailable, code %d",filename,efs_get_errno());
	break;
    }    
    break;

//============================================================================  
//View files
  case MODE_TYPE:
    if (optind == argc) {
      printf("\n File name not specified");
      break;
    }  
    list_file(argv[optind],tmode);
    break;

//============================================================================  
//Extracting a file
  case MODE_GETFILE:
     if (optind < (argc-2)) {
      printf("\nNot enough parameters on the command line");
      break;
    }  
    if (optind == (argc-1)) get_file(argv[optind],0);
    else get_file(argv[optind],argv[optind+1]);	
    break;
    
//============================================================================  
//Write a file
  case MODE_WRITEFILE:
    if (optind != (argc-2)) {
      printf("\nNot enough parameters on the command line");
      break;
    }  
    write_file(argv[optind],argv[optind+1]);
    break;

//============================================================================  
//Deleting a file
  case MODE_DELFILE:
    if (optind == argc) {
      printf("\n File name not specified");
      break;
    }  
    switch (gmode) {
      case G_FILE:
        efs_unlink(argv[optind]);
	break;

      case G_DIR:
        efs_rmdir(argv[optind]);
	break;
    }	
    break;

//============================================================================  
//Creating a directory
  case MODE_MKDIR:
    if (optind == argc) {
      printf("\nNot enough parameters on the command line");
      break;
    }  
    if (efs_mkdir(argv[optind],7) != 0) {
      printf("\nError creating directory %s, code %d",argv[optind],efs_get_errno());
    }  
    break;
    
//============================================================================  
  default:
    printf("\nThe key of the operation being performed is not specified\n");
    return;

}    
if (lmode != fl_mid) printf("\n");

}

