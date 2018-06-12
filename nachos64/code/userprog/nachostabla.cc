#include "nachostabla.h"

NachosOpenFilesTable::NachosOpenFilesTable()
{
	this->openFiles = new int[ MAX_FILES ];
	this->openFiles[0] = 0; //std::in
	this->openFiles[1] = 1; //std::out
	this->openFiles[2] = 2; //std::cerr

	for (int x = 3; x < MAX_FILES; ++x)
		 this->openFiles[x] = 0;
 
	this->openFilesMap = new BitMap( MAX_FILES );
	this->openFilesMap->Mark(0);
	this->openFilesMap->Mark(1);
	this->openFilesMap->Mark(2);
}


NachosOpenFilesTable::~NachosOpenFilesTable()
{
    if(usage == 1){
        delete[] openFiles;
        delete openFilesMap;
    }
}

bool NachosOpenFilesTable::isOpened( int NachosHandle ){
    if(NachosHandle >= 0 && NachosHandle < MAX_FILES){
        return openFilesMap->Test(NachosHandle);
    }
    return false;
}

int NachosOpenFilesTable:: Open( int UnixHandle )
{	
	int freeFile = this->openFilesMap->Find();
	if (freeFile != -1)
	{
		this->openFiles[ freeFile ] = UnixHandle;
	} 		
	return freeFile;
}

int NachosOpenFilesTable::Close( int NachosHandle )
{
    if(isOpened(NachosHandle)){
	    this->openFilesMap->Clear( NachosHandle );
	    this->openFiles[ NachosHandle ] = 0;
        return	 0;
    }
    return -1;
}

int NachosOpenFilesTable::getUnixHandle( int NachosHandle ){
    if(isOpened(NachosHandle)){
        return openFiles[NachosHandle];
    }
    return -1;
}

void NachosOpenFilesTable::addThread(){
    ++usage;
}

void NachosOpenFilesTable::delThread(){
    --usage;
}

void NachosOpenFilesTable::Print(){
    for(int i = 0; i < MAX_FILES; ++i){
        printf("Nachos handle: %d, Unix handle: %d\n", i, openFiles[i]);
    }
    printf("\n");
}


