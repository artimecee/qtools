//
//Procedures for working with a flash drive partition table
//
#include "include.h"


//flash drive partition table storage
struct flash_partition_table fptable;
int validpart=0; //table validity


//*************************************
//* reading partition table from flash
//*************************************
int load_ptable_flash() {

unsigned int udsize=512;
unsigned int blk;
unsigned char buf[4096];

if (get_udflag()) udsize=516;

for (blk=0;blk<12;blk++) {
  //We are looking for a block with maps
  flash_read(blk, 0, 0);     //Page 0 sector 0 - MIBIB block header
  memread(buf,sector_buf, udsize);
  //checking the MIBIB header signature
  if (memcmp(buf,"\xac\x9f\x56\xfe\x7a\x12\x7f\xcd",8) != 0) continue; //signature not found - look further

  //MiBIB block found - page 1 contains the system partition table
  //load this page into our buffer
  flash_read(blk, 1, 0);     //Page 1 sector 0 - system partition table
  memread(buf,sector_buf, udsize);
  mempoke(nand_exec,1);     //sector 1 - continuation of the table
  nandwait();
  memread(buf+udsize,sector_buf, udsize);

  //copy the table image into the structure
  memcpy(&fptable,buf,sizeof(fptable));
  //checking the system table signature
  if ((fptable.magic1 != FLASH_PART_MAGIC1) || (fptable.magic2 != FLASH_PART_MAGIC2)) continue;
    //found a table
    validpart=1;
    //adjust the length of the last section
    if ((maxblock != 0) && (fptable.part[fptable.numparts-1].len == 0xffffffff)) 
        fptable.part[fptable.numparts-1].len=maxblock-fptable.part[fptable.numparts-1].offset; //if the length is FFFF, then there is a growing partition
    return 1; //That's it - the table has been found, there is nothing more to do here
}  
validpart=0;
return 0;  
}

//***************************************************
//* Loading partition table from external file
//*
//* File name consisting of one minus sign '-' -
//* loading from file ptable/current-r.bin
//***************************************************
int load_ptable_file(char* name) {

char filename[200];
unsigned char buf[4096];
FILE* pf;

if (name[0] == '-') strcpy(filename,"ptable/current-r.bin");
else strncpy(filename,name,199);
  
pf=fopen(filename,"rb");
if (pf == 0) {
   printf("\n! Error opening partition table file %s\n",filename);
   return 0;
} 
fread(buf,1024,1,pf); //read partition table from file
fclose(pf);
//copy the table image into the structure
memcpy(&fptable,buf,sizeof(fptable));
validpart=1;
return 1;
}

//*****************************************************
//* Universal partition table loading procedure
//*
//* Possible name options:
//*
//* @ - loading table from flash drive
//* - - loading from file ptable/current-r.bin
//* name - loading from the specified file
//*
//*****************************************************
int load_ptable(char* name) {

if (name[0]== '@') return load_ptable_flash();
else return load_ptable_file(name);
}
  
//***************************************************
//* Display the partition table header
//***************************************************
void print_ptable_head() {

  printf("\n # start size A0 A1 A2 F# format ------ Name------");     
  printf("\n============================================================\n");
}


//***************************************************
//* Display information about a section by its number
//***************************************************
int show_part(int pn) {
  
if (!validpart) return 0; //the table has not yet been loaded
if (pn>=fptable.numparts) return 0; //wrong section number
printf("\r%02u  %6x",
       pn,
       fptable.part[pn].offset);

if (fptable.part[pn].len != 0xffffffff)
  printf("  %6.6x   ",fptable.part[pn].len);
else printf("  ------   ");

printf("%02x %02x %02x %02x   %s   %.16s\n",
       fptable.part[pn].attr1,
       fptable.part[pn].attr2,
       fptable.part[pn].attr3,
       fptable.part[pn].which_flash,
       (fptable.part[pn].attr2==1)?"LNX":"STD",
       fptable.part[pn].name);

return 1;  
}


//*************************************
//* Displaying the partition table on the screen
//*************************************
void list_ptable() {
  
int i;

if (!validpart) return; //the table has not yet been loaded
print_ptable_head();
for (i=0;i<fptable.numparts;i++) show_part(i);
printf("============================================================");
printf("\nPartition table version: %i\n",fptable.version);
}

//*************************************
//Getting a name by section number
//*************************************
char* part_name(int pn) {
  
return fptable.part[pn].name;
}

//*********************************************
//Getting the starting block by section number
//*********************************************
int part_start(int pn) {
  
return fptable.part[pn].offset;
}

//*********************************************
//Getting partition length by partition number
//*********************************************
int part_len(int pn) {
  
return fptable.part[pn].len;
}

//************************************************************
//Getting the number of the section that contains the specified block
//************************************************************
int block_to_part(int block) {

int i;
for(i=0;i<fptable.numparts;i++) {
  if ((block>=part_start(i)) && (block < (part_start(i)+part_len(i)))) 
     return i;
}
//partition not found
return -1; 
}
