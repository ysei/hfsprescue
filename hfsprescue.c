/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 * I know, the program is not perfect, but it will do its job and restore
 * the most files and directories.
 *
 */

#define VERSION "0.1 20101130"

#define _FILE_OFFSET_BITS 64
#define __USE_FILE_OFFSET64

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdint.h>

#include <stdlib.h>
#include <string.h>

#include <signal.h>

char recordType[4][14]={ "Folder","File","FolderThread","FileThread" };

int fd;
__off64_t fofs;
unsigned char *fbuffer;

FILE *pflog, *pFolderTable;

char log[1024];
bool verboselog;

int directories=0;
int files=0;
float scanned=0;
int oldsum=-1;

struct HFSPlusExtentDescriptor
{
    uint32_t	startBlock;
    uint32_t	blockBlock;
};

struct HFSPlusForkData
{
    uint64_t	logicalSize;	// size in bytes
    uint32_t	clumpSize;	
    uint32_t	totalBlocks;	// number of all blocks
    
    HFSPlusExtentDescriptor extents[8];
};

struct HFSPlusVolumeHeader
{
    uint16_t	signature;
    uint16_t	version;
    uint32_t	attributes;
    uint32_t	lastMountedVersion;
    uint32_t	journalBlockInfo;
    
    uint32_t	createDate;
    uint32_t	modifyDate;
    uint32_t	backupDate;
    uint32_t	checkedDate;
    
    uint32_t	fileCount;
    uint32_t	folderCount;
    
    uint32_t	blockSize;
    uint32_t	totalBlocks;
    uint32_t	freeBlocks;
    
    uint32_t	nextAllocation;
    uint32_t	rsrcClumpSize;
    uint32_t	dataClumpSize;
    uint32_t	nextCataogID;
    
    uint32_t	writeCount;
    uint64_t	encodingsBitmap;
    
    uint32_t	finderInfo[8];
    
    HFSPlusForkData	allocationFile;
    HFSPlusForkData	extentsFile;
    HFSPlusForkData	catalogFile;
    HFSPlusForkData	attributesFile;
    HFSPlusForkData	startupFile;

};


struct HFSPlusBSDInfo
{
    
    uint32_t	a;
    uint32_t	b;
    
    uint8_t	c;
    uint8_t	d;
    
    uint16_t	e;
    
    uint32_t	f;
    uint32_t	g;
    uint32_t	h;
};



struct HFSPlusCatalogFolder
{
//    uint16_t	recordType;
    uint16_t 	flags;
    uint32_t	valence;
    uint32_t	folderID;
    uint32_t	createDate;
    uint32_t	contentModDate;
    uint32_t	attributeModDate;
    uint32_t	accessDate;
    uint32_t	backupDate;
    HFSPlusBSDInfo	permissions;
//xx    uint32_t	userInfo;
//	    finderInfo;
    uint32_t	textEncoding;
    uint32_t	reserved;

};



void swap16 (uint16_t *val)
{
    uint16_t o;
    uint16_t n;
    
    o=*val;

    n=(o & 0xff00) >> 8;
    n+=(o & 0x00ff) << 8;

    *val=n;    
}

void swap32 (uint32_t *val)
{
    uint32_t o;
    uint32_t n;
    
    o=*val;

    n=(o & 0xff000000) >> 24;
    n+=(o & 0x00ff0000) >> 8;
    n+=(o & 0x0000ff00) << 8;
    n+=(o & 0x000000ff) << 24;

    *val=n;    
}

void ConvertFilename (unsigned char *in, unsigned char *out, unsigned int len)
{
    unsigned char c;
    
    for (int i=0;i < len; i++)
    {
	c=in[i * 2 + 1];
	if ((c<' ') || (c>'z') || (c=='/') || (c=='\\')) c='_';
	
	out[i]=c;
    }
    out[len]=0x00;

}

void Log()
{
    if (verboselog)
	printf ("%s\n",log);

    fprintf (pflog, "%s\n", log);
}

void LogNoNL()
{
    if (verboselog)
	printf ("%s",log);

    fprintf (pflog, "%s", log);
}

void LogPrn()
{
    printf ("%s\n",log);
    fprintf (pflog, "%s\n", log);
}

void PrintInfo()
{
    if (oldsum == (int)scanned + directories + files)
	return;
	
    if (verboselog)
	printf ("\n");
	
    printf ("\r%0.2f%% scanned, %d directories found, %d files restored",
	    scanned,
	    directories,
	    files);

//	printf ("\n");

    oldsum = (int)scanned + directories + files;
}

void GetFile (char *filename,
	      unsigned long long int filesize,
	      unsigned char *buffer,
	      unsigned int ofs,
	      unsigned int blocksize,
	      unsigned int parentID	      
	      )
{

    unsigned long int block,num,ext;
    
    
    int i;
    unsigned int write;
    char fileout[8192];
    char dir[8192];
    int fnr=0;

    FILE *pfout;
    
    if (filesize > 1024*1024*1024) 
    {
	sprintf (log, "Skipping file %s, has to be implemented.", filename);
	Log();
	fprintf (stderr, "\n%s\n",log);
	return;    
    }



    sprintf (dir, "restored/%d", parentID);
    mkdir (dir, 7660);
    
    sprintf (fileout, "restored/%d/%s", parentID, filename);
    

    pfout=fopen (fileout, "w");
    if (!pfout)
    {
	printf ("error create file %s\n", filename);
	exit (EXIT_FAILURE);
    }
    


    for (ext = 0; ext < 8; ext++)
    {

	sprintf (log,"hd: ofs %d | ", ofs);
	LogNoNL();
	
        block=(buffer[ofs] << 24) + 
    	        (buffer[ofs + 1] << 16) + 
		(buffer[ofs + 2] << 8) + 
		(buffer[ofs + 3]);
	ofs+=4;
	num=(buffer[ofs] << 24) + 
	        (buffer[ofs + 1] << 16) + 
		(buffer[ofs + 2] << 8) + 
		(buffer[ofs + 3]);
	ofs+=4;

	sprintf (log, "block %d/0x%x num %d ofs %d", block, block, num, ofs);
	Log();
	//printf ("%x %d %d\n",block*blocksize,num,ofs);
	if ((num > 0) && (block > 10))
	{
//pos=0xaa4c7000;

	    __off64_t g;
//g=0x22aa4c7000;
	    g=(__off64_t)block*(__off64_t)blocksize;


	    if (num > 10000) 
	    {
		sprintf (log, "Skipping file part2 %s, has to be implemented.", filename);
		Log();
		fprintf (stderr, "\n%s\n",log);

		fclose (pfout);
		return;    
	    }

	    for (i = 0; i < num; i++)
	    {    
		pread64 (fd,fbuffer,blocksize,g);
		g+=(__off64_t)blocksize;
		
		if (filesize>blocksize)
		    write=blocksize;
		else write=filesize;

		if (write > 0)
		    fwrite (fbuffer,write,1,pfout);


		if (filesize<blocksize) 
		{
		
		    fclose (pfout);
		    files++;
		    return;
		}

		filesize-=(unsigned long long)blocksize;
	    }
	}
    }
    fclose (pfout);
    files++;

}	      
	      


void scanBuffer (unsigned char *buffer, unsigned int ofs,unsigned int blocksize)
{

    unsigned int keyLen;
    unsigned int type;
    unsigned int filenameLen;
    unsigned char filename[256];
    char dir[256];
    unsigned int parentID;
    unsigned int folderID;
    unsigned long long int filesize;
    int found;
    int ofs2;

    unsigned int extent[8];
    

    while (ofs < 4000)
    {    


	found=0;
	while ((ofs < 4000) && (!found))
	{
//	printf ("ofs %04x\n",ofs);
	    keyLen=(buffer[ofs] << 8) + buffer[ofs + 1];
	    filenameLen=(buffer[ofs+2+4] << 8)+ (buffer[ofs + 1 +2+4]);
	    ofs2=ofs+keyLen+2;
	    type=(buffer[ofs2] << 8) + buffer[ofs2 + 1];

//	printf ("ofs %04x  %d  %d %x\n",ofs,keyLen,filenameLen,type);

	    if ((keyLen == filenameLen * 2 + 6) && (keyLen>1) && (type>0) && (type<5)
	       && (filenameLen>2) && (filenameLen<256))
	    {
		//printf ("found %04x   %04x\n", ofs, ofs2);
		found=1;
	    }
	    else  ofs++;
	
	}

	
	if (found)
	{

	    int oblock=fofs >> 9;
	    sprintf (log, "block %d, ofs 0x%x/%d | ", oblock, ofs, ofs);
	    LogNoNL();
	    
	    keyLen=(buffer[ofs] << 8) + buffer[ofs + 1];
	    ofs+=2;
    
	    parentID=(buffer[ofs] << 24) + 
		    (buffer[ofs + 1] << 16) + 
		    (buffer[ofs + 2] << 8) + 
		    (buffer[ofs + 3]);

    
	    /*    printf ("#ofs: %x  keyLen: %x  ",
		    0,//      vh->catalogFile.extents[0].startBlock * vh->blockSize + 
		    //  startOfs +
	    	    //3*4 + 2,
	    	    keyLen);
		printf ("parentID: %x/%d \n",parentID ,parentID);
	    */
    
	    ofs+=4;
    
	    filenameLen=(buffer[ofs] << 8)+ (buffer[ofs + 1]);
	    ofs+=2;
    
	    //	printf ("ofs %04x\n",ofs);
	    ConvertFilename (buffer + ofs, filename, filenameLen);
	    sprintf (log, "%s | ", filename);
	    LogNoNL();

	    ofs+=filenameLen * 2;

	    type=(buffer[ofs] << 8) + buffer[ofs + 1];
	    ofs+=2;

	    sprintf (log, "RecordType: 0x%04x, %s | ParentID: %d ",
			type, recordType[type - 1], parentID);
	    LogNoNL();
	    
	    if (type==1)
	    {		

		folderID=(buffer[ofs+2+4] << 24) + 
		        (buffer[ofs + 1 +2+4] << 16) + 
			(buffer[ofs + 2 +2+4] << 8) + 
			(buffer[ofs + 3 +2+4]);

    		sprintf (log, "\nFolderinfo: %d|%d|%s", 
    			    folderID, parentID, filename);
    		Log();
    		
    		sprintf (dir,"restored/%d", folderID);
    		mkdir (dir,7660);

    		fprintf (pFolderTable, "%d|%d|%s\n", 
    			    folderID, parentID, filename);
	    
		directories++;
	    } 
	    else if ((type == 2) && (filenameLen > 2))
	    {
		ofs2 = ofs + 6 * 16 - 8 - 2;

		filesize=0;
		for (int i = 0; i < 8; i++)
		{
		    filesize<<=8;
		    filesize+=buffer[ofs2];
		    ofs2++;
		}
		
		sprintf (log, " | Filesize: %d", filesize);
		Log();
		
		if (ofs2 > 4000) 
		{
		    sprintf (log, "buffer error %d, has to be fixed\n", ofs2);
		    Log();
		}
		else
		    if (filesize > 0) 
			GetFile ((char *)filename,filesize,buffer,ofs2+8,blocksize,parentID);
	    } 
	    else
	    {
		log[0]=0x00;
		Log();
	    }
	    
	    //PrintInfo();
	}

	PrintInfo();
    }

    //PrintInfo();

}

void sigproc (int sig)
{
    signal (SIGINT, sigproc);
    
    if (pflog)
	fclose (pflog);

    if (pFolderTable)
	fclose (pFolderTable);

    printf ("\n\n");
    exit (EXIT_FAILURE);
}

struct _dir_restore
{
    unsigned int folderID;
    unsigned int parentID;
    char *filename;
    bool check;
};

void ExpandDirInfo (char *info, 
		    unsigned int *folderID,
		    unsigned int *parentID,
		    char *filename
		    
		    )
{
    unsigned int i,p,t;
    char txt[1024];
    
    p=0;
    t=0;
    for (i = 0; info[i] != 0; i++)
    {
	if ((info[i] !='\r') && (info[i] != '\n'))
	{
	    if (info[i] != '|')
	    {
		txt[p] = info[i];
		p++;	
	    } 
	    else 
	    {
		txt[p] = 0;
		switch (t)
		{
		    case 0:
			*folderID = atoi (txt);
			break;

		    case 1:
			*parentID = atoi (txt);
			break;	    
		}
		t++;
		p = 0;
	    }
	}
    
    }
    txt[p] = 0;
    strcpy (filename,txt);

}

void GenerateRenameScript()
{
    FILE *pf, *pfout;
    char info[1024];
    
    _dir_restore *pDirs;
    _dir_restore *pDirsBase=NULL;

    char *pFilenamesBase=NULL;
    
    unsigned int dirCount=0;
    unsigned int i,m;
    unsigned int folderID;
    unsigned int parentID;
    char filename[1024];
    bool checkAgain;
    bool last;
    long size=0, pos;
        
    
    pf = fopen ("foldertable.txt", "r");
    if (!pf)
    {
	fprintf (stderr, "Unable to open foldertable.txt\n");
	exit (EXIT_FAILURE);    
    }
    
    pfout = fopen ("restored/dirrestore.sh", "w");
    if (!pfout)
    {
	fprintf (stderr, "Unable to create restored/dirrestore.sh\n");
	exit (EXIT_FAILURE);    
    }
    
    printf ("Loading ID's...\n");

    pFilenamesBase = (char *)malloc (size+1);

    while (!feof (pf))
    {
	if (fgets (info, sizeof(info), pf))
	{
	    
	    dirCount++;
	    pDirsBase = (_dir_restore *)realloc (pDirsBase, dirCount * sizeof (_dir_restore));

	    pDirs = pDirsBase + dirCount - 1;
	    
	    ExpandDirInfo (info, &folderID, &parentID, filename);
	    
	    pDirs->check = true;
	    pDirs->folderID=folderID;
	    pDirs->parentID=parentID;
	    
	    pos = size;
	    size+= strlen (filename) + 1;
	    pFilenamesBase = (char *)realloc (pFilenamesBase, size * sizeof(char));
	    pDirs->filename=pFilenamesBase + pos;

	    strcpy (pDirs->filename, filename);
	    

	
	}    
    }

    printf ("Creating directory restore script...\n");
    
    fprintf (pfout, "#!/bin/sh\n\n");

    fprintf (pfout, "cd restored\n");
    fprintf (pfout, "echo Restoring directory structure...\n");

    checkAgain = true;
    
    while (checkAgain)
    {
	checkAgain = false;
	
	for (m = 1; m < dirCount; m++)
	{
    	    pDirs = pDirsBase + m;
    	
    	    if (pDirs->check)
    	    {
    	
		parentID = pDirs->folderID;
		last=true;	// assume its the deepest directory
	
		for (i = 1; (i < dirCount) && last; i++)
		{
    
    		    pDirs = pDirsBase + i;
    		    if (pDirs->check)
    		    {
    			checkAgain=true;

    			if (pDirs->parentID == parentID)
    			    last=false; // it has a child -> its not deepest
    		    }	    
		}
	
		if (last) // its the deepest directory -> move to parent
		{
    		    pDirs = pDirsBase + m;
    		    pDirs->check = false;
    		    fprintf (pfout, "mv \"%d\" \"%d/%s\" >& /dev/null\n", 
    				    pDirs->folderID, 
    				    pDirs->parentID,
    				    pDirs->filename);
		}
	    }    
	}
    }



    fprintf (pfout, "mv 2 root\n");
    fprintf (pfout, "echo\n");
    fprintf (pfout, "echo Finished. Your files are in the restored directory.\n");
    fprintf (pfout, "echo\n");
    fclose (pfout);
    fclose (pf);
    
    printf ("\nScript created.\n\n");
    printf ("Run now 'hfsrestore -s' or 'sh restored/dirrestore.sh'\n\n");

}

void RunScript()
{
    FILE *pf;
    
    pf=fopen ("restored/dirrestore.sh","r");
    if (!pf)
    {
	printf ("Unable to open the file restored/dirrestore.sh\n\n");
	printf ("Run 'hfsprescue -h' for help.\n\n");
	exit (EXIT_FAILURE);    
    }
    fclose (pf);
    
    system ("sh restored/dirrestore.sh");

}

void PrintHelp()
{    
    printf ("hfsprescue scans a damaged image file or partition that is formatted with\n");
    printf ("HFS+. You can restore your files and directories, even when it's not possible \n");
    printf ("to mount it with your operating system. Your files and directories will be\n");
    printf ("stored in the directory './restored' in your current directory. The HFS+ file\n");
    printf ("or partition will not be changed. So you need enough space to copy out the\n");
    printf ("files from the HFS+ file system. Important infos will be logged to\n");
    printf ("'hfsprescue.log'. The directory structure will be stored in 'foldertable.txt'\n");
    printf ("and is used to restore the directory structure and directory names.\n\n");
    printf ("This is the first version. Maybe, you will be not able to restore all files\n");
    printf ("and directories, but you should get the most back. However, its possible to\n");
    printf ("make the program better and rescue all files.\n\n");
    printf ("You have to complete 3 steps to restore your files:\n");
    printf (" 1) run 'hfsprescue [device node|image file]' this restores your files\n");
    printf (" 2) run 'hfsprescue -d' to create the script to restore the directory structure\n");
    printf (" 3) run 'hfsprescue -s' to restore the directory structure and directory names\n");
    printf ("\n");
    printf ("Example: hfsprescue /dev/sdb2\n");
    printf ("         hfsprescue -v /dev/sdb2   verbose infos on the screen\n");
}

int main (int argc, char *argv[])
{
    HFSPlusVolumeHeader *vh;
    
    FILE *pf;
    uint32_t	startOfs;

    unsigned char cKeyLen[2];
    int keyLen;
    
    uint32_t 	*parentID;
    int type;

//    unsigned filenameLen;
    char filename[4096];
    
    struct stat sts;
    
    
    unsigned char *buffer;


    printf ("hfsprescue " VERSION " by Elmar Hanlhofer http://www.plop.at\n\n");


    if ((argc == 1) || (strcmp (argv[1],"-h") == 0))
    {
	PrintHelp();
	exit (EXIT_SUCCESS);    
    }


    if (strcmp (argv[1],"-d") == 0)
    {    
	GenerateRenameScript();
	exit(0);
    }

    if (strcmp (argv[1],"-s") == 0)
    {    
	RunScript();
	exit(0);
    }

    if (strcmp (argv[1],"-v") == 0)
    {
	verboselog = true;
	if (argc != 3)
	{
	    fprintf (stderr, "input error");
	    exit (EXIT_FAILURE);
	}
	strcpy (filename,argv[2]);    
    }
    else {    
	verboselog=false;
	strcpy (filename,argv[1]);        
    }



    pflog=fopen ("hfsprescue.log","w");
    if (!pflog)
    {
	printf ("error creating file hfsprescue.log\n");
	return EXIT_FAILURE;
    }
    else
	fprintf (pflog, "hfsprescue " VERSION " by Elmar Hanlhofer http://www.plop.at\n\n");

    signal (SIGINT, sigproc);	// trap CTRL-C

    
    vh=(HFSPlusVolumeHeader *)malloc (sizeof (HFSPlusVolumeHeader));
    parentID=(uint32_t *)malloc (sizeof (uint32_t));
    
    
    fd=open64 (filename,O_RDONLY);
    if (!fd)
    {
	fprintf (stderr, "fopen error: %s\n", filename);
	exit (EXIT_FAILURE);
    }
    
    stat (filename, &sts);

    mkdir ("restored",7660);
    mkdir ("restored/2",7660);	// root dir
    
    pread64 (fd,vh,sizeof (HFSPlusVolumeHeader),1024);
    
    startOfs=4096;

    swap32 (&vh->fileCount);
    swap32 (&vh->folderCount);
    swap32 (&vh->blockSize);
    swap32 (&vh->totalBlocks);
    swap32 (&vh->allocationFile.extents[0].startBlock);
    swap32 (&vh->extentsFile.extents[0].startBlock);
    swap32 (&vh->catalogFile.extents[0].startBlock);
//vh[0].fileCount=3;
    sprintf (log, "Signature: %02x",vh->signature);
    LogPrn();

    sprintf (log, "FileCount: %d",vh->fileCount);
    LogPrn();

    sprintf (log, "DirCount: %d",vh->folderCount);
    LogPrn();

    sprintf (log, "BlockSize: %d",vh->blockSize);
    LogPrn();

    sprintf (log, "TotalBlocks: %d",vh->totalBlocks);
    LogPrn();

    sprintf (log, "AllocationFile");
    LogPrn();

    sprintf (log, "StartBlock: %d",vh->allocationFile.extents[0].startBlock);
    LogPrn();

    sprintf (log, "ExtentsFile");
    LogPrn();

    sprintf (log, "StartBlock: %d",vh->extentsFile.extents[0].startBlock);
    LogPrn();

    sprintf (log, "CatalogFile");
    LogPrn();

    sprintf (log, "StartBlock: %d",vh->catalogFile.extents[0].startBlock);
    LogPrn();



    sprintf (log,"");
    LogPrn();
    
    pFolderTable=fopen ("foldertable.txt","w");
    if (!pFolderTable)
    {
	printf ("error creating file foldertable.txt\n");
	return EXIT_FAILURE;
    }
    fprintf (pFolderTable,"0|0|dummy entry\n");


    fofs=(__off64_t)startOfs;
    fofs+=(__off64_t)vh->catalogFile.extents[0].startBlock * (__off64_t)vh->blockSize;

    buffer=(unsigned char *)malloc (vh->blockSize);
    fbuffer=(unsigned char *)malloc (vh->blockSize);


    int ofs = 3 * 4 + 2;
    fofs = 10;
    //for (int i=0;i<1000000;i++)
    while (1)
    {
	if (!pread64 (fd,buffer, vh->blockSize,fofs))
	    break;

	if (buffer[0] != 0xff)    
	    scanBuffer (buffer, ofs, vh->blockSize);

	fofs+=(__off64_t)vh->blockSize;
	scanned =  (float)fofs / (float)sts.st_size * 100;

	PrintInfo();
    }


    printf ("\n\n");    
    printf ("Now run 'hfsprescue -d' to create the directory structure restore script\n\n");
    
    free (fbuffer);

    fclose (pflog);
    fclose (pFolderTable);
    close (fd);

    return EXIT_SUCCESS;
}
