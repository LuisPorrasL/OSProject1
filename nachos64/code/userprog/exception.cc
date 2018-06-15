// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synch.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <iostream>

Semaphore* Console = new Semaphore("Console", 1);

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------


/*RETURN FROM SYSTEM CALL*/

void returnFromSystemCall() {

  int pc, npc;

  pc = machine->ReadRegister( PCReg );
  npc = machine->ReadRegister( NextPCReg );
  machine->WriteRegister( PrevPCReg, pc );        // PrevPC <- PC
  machine->WriteRegister( PCReg, npc );           // PC <- NextPC
  machine->WriteRegister( NextPCReg, npc + 4 );   // NextPC <- NextPC + 4

}// returnFromSystemCall


void Nachos_Halt() {                    // System call 0
  DEBUG('a', "Shutdown, initiated by user program.\n");
  interrupt->Halt();

}// Nachos_Halt


void Nachos_Open() {                    // System call 5
  /* System call definition described to user

  */
  // Read the name from the user memory, see 5 below
  // Use NachosOpenFilesTable class to create a relationship
  // between user file and unix file
  // Verify for errors
  //printf("Opening!\n");

  int r4 = machine->ReadRegister( 4 );
  char fileName [128] = {0};
  int c = 0, i = 0;
  do{
    machine->ReadMem(r4, 1, &c);
    r4++;
    fileName[i++] = c;
  }while(c != 0);

  int unixOpenFileId = open( fileName, O_RDWR );
  if( unixOpenFileId != -1 ){
    int nachosOpenFileId = currentThread->mytable->Open(unixOpenFileId);
    if(nachosOpenFileId != -1){
      ///currentThread->mytable->Print();
      machine->WriteRegister(2, nachosOpenFileId);
      returnFromSystemCall();		// Update the PC registers
      return;
    }
  }
  printf("Error: unable to open file\n");
  returnFromSystemCall();		// Update the PC registers
}// Nachos_Open

void Nachos_Read(){
  //printf("Reading!\n");
  int r4 =  machine->ReadRegister(4); // pointer to Nachos Mem
  int size = machine->ReadRegister(5); // byte to read
  OpenFileId fileId = machine->ReadRegister(6); // file to read
  char bufferReader[size] = {0}; // store unix result
  int readBytes = 0; // amount of read bytes

  // verify if file is one of standar output/input
  switch ( fileId ) {
    case ConsoleOutput:
    printf("%s\n", "Error, can not read from standard output");
    break;
    case ConsoleError:
    printf("%s\n", "Error, can not read from standard error");
    break;
    case ConsoleInput:
    fgets( bufferReader, size , stdin );
    readBytes = strlen( bufferReader );
    // write into Nachos mem
    for (int index = 0; index < readBytes; ++  index )
    {
      machine->WriteMem(r4, 1, bufferReader[index] );
      ++r4;
    }
    machine->WriteRegister(2, readBytes );
    break;
    default:
    if ( currentThread->mytable->isOpened( fileId ) ) // if file is still opened
    {
      //  read using Unix system call
      readBytes =  read( currentThread->mytable->getUnixHandle(fileId),
      (void *)bufferReader, size );
      // write into Nachos mem
      for (int index = 0; index < readBytes; ++  index )
      {
        machine->WriteMem(r4, 1, bufferReader[index] );
        ++r4;
      }
      // return amount of read readBytes
      machine->WriteRegister(2, readBytes );
    }else // otherwise no chars read
    {
      printf("Error: unable to read file\n");
      machine->WriteRegister(2,-1);
    }
    break;
  }
  returnFromSystemCall();
}// Nachos_Read

void Nachos_Write() {                   // System call 7

  /* System call definition described to user
  void Write(
  char *buffer,	// Register 4
  int size,	// Register 5
  OpenFileId id	// Register 6
);
*/
int r4 = machine->ReadRegister( 4 );
int size = machine->ReadRegister( 5 );	// Read size to write
char buffer[size+1] = {0};

int c = 0, i = 0;
do{
  machine->ReadMem(r4, 1, &c);
  r4++;
  buffer[i++] = c;
}while(i < size);

//printf("%s\n", buffer);

// buffer = Read data from address given by user;
OpenFileId id = machine->ReadRegister( 6 );	// Read file descriptor

// Need a semaphore to synchronize access to console
Console->P();
switch (id) {
  case  ConsoleInput:	// User could not write to standard input
  machine->WriteRegister( 2, -1 );
  size = 0;
  break;
  case  ConsoleOutput:
  printf( "%s", buffer );
  break;
  case ConsoleError:	// This trick permits to write integers to console
  printf( "%d\n", machine->ReadRegister( 4 ) );
  size = 1;
  break;
  default:	// All other opened files
  // Verify if the file is opened, if not return -1 in r2
  if(!currentThread->mytable->isOpened(id)){
    machine->WriteRegister( 2, -1 );
    size = 0;
    return;
  }
  // Get the unix handle from our table for open files
  int unixOpenFileId = currentThread->mytable->getUnixHandle(id);
  // Do the write to the already opened Unix file
  int charCounter = write(unixOpenFileId, buffer, size);
  // Return the number of chars written to user, via r2
  machine->WriteRegister( 2, charCounter );
  break;

}
// Update simulation stats, see details in Statistics class in machine/stats.cc
stats->numConsoleCharsWritten += size;
Console->V();
returnFromSystemCall();		// Update the PC registers

}// Nachos_Write

void Nachos_Create(){
  ///printf("Creating!\n");
  int r4 = machine->ReadRegister(4); // read from register 4
  char fileName[256] = {0}; // need to store file name to unix create sc
  int c, i; // counter
  i = 0;

  do
  {
    machine->ReadMem( r4 , 1 , &c ); // read from nachos mem
    r4++;
    fileName[i++] = c;
  }while (c != 0 );

  int createResult = creat (fileName, O_CREAT | S_IRWXU ); // create with read write destroy authorization
  close(createResult);
  if (-1 == createResult )
  {
    printf("Error: unable to create new file\n");
  }
  returnFromSystemCall();		// Update the PC registers
}// Nachos_Create

void Nachos_Close(){
  /* Close the file, we're done reading and writing to it.
  void Close(OpenFileId id);*/
  //printf("Closing!\n");
  OpenFileId id = machine->ReadRegister( 4 );
  int unixOpenFileId = currentThread->mytable->getUnixHandle( id );
  int nachosResult = currentThread->mytable->Close(id);
  int unixResult = close( unixOpenFileId );
  if(nachosResult == -1 || unixResult == -1 ){
    printf("Error: unable to close file\n");
  }
  //currentThread->mytable->Print();
  returnFromSystemCall();		// Update the PC registers
}// Nachos_Close

void NachosForkThread( void * p ) { // for 64 bits version
  AddrSpace *space;
  long dir = (long) p;

  space = currentThread->space;
  space->InitRegisters();             // set the initial register values
  space->RestoreState();              // load page table register

  // Set the return address for this thread to the same as the main thread
  // This will lead this thread to call the exit system call and finish
  machine->WriteRegister( RetAddrReg, 4 );

  machine->WriteRegister( PCReg, dir );
  machine->WriteRegister( NextPCReg, dir + 4 );


  machine->Run();                     // jump to the user progam

  ASSERT(false);
}

void Nachos_Fork()
{
  DEBUG( 'u', "Entering Fork System call\n" );
  // We need to create a new kernel thread to execute the user thread
  Thread * newT = new Thread( "child to execute Fork code" );

  delete  newT->mySems;
  newT->mySems = currentThread->mySems;
  newT->mySems->addSem();


  // We need to share the Open File Table structure with this new child
  delete  newT->mytable;
  newT->mytable = currentThread->mytable;
  newT->mytable->addThread();

  // Child and father will also share the same address space, except for the stack
  // Text, init data and uninit data are shared, a new stack area must be created
  // for the new child
  // We suggest the use of a new constructor in AddrSpace class,
  // This new constructor will copy the shared segments (space variable) from currentThread, passed
  // as a parameter, and create a new stack for the new child
  newT->space = new AddrSpace( currentThread->space );

  // We (kernel)-Fork to a new method to execute the child code
  // Pass the user routine address, now in register 4, as a parameter
  // Note: in 64 bits register 4 need to be casted to (void *)
  newT->Fork( NachosForkThread, (void*)(long)(machine->ReadRegister( 4 ))); // ojo se elimino warning
  currentThread->Yield();
  returnFromSystemCall();	// This adjust the PrevPC, PC, and NextPC registers

  DEBUG( 'u', "Exiting Fork System call\n" );
}	// Nachos_Fork


void Nachos_SemCreate()
{
  long initValue = machine->ReadRegister( 4 );
  //printf("Valor inicial: %ld\n", initValue );
  Semaphore* sem = new Semaphore("Sem usuario", initValue);
  if ( sem != NULL )
  {
    //printf("Se crea sem\n");
    int nachosSemIdentfier = currentThread->mySems->registerSem( (long)sem );
    //printf("Su identificador es : %d\n", nachosSemIdentfier );
    machine->WriteRegister( 2, nachosSemIdentfier );
  }else
  {
    // return invalid id for sem
    machine->WriteRegister( 2, -1 );
  }
}// Nachos_SemCreate

void Nachos_SemWait()
{
  //printf("%s\n", "Nachos wait");
  int semId = machine->ReadRegister( 4 );
  //printf("id: %d\n", semId );
  long pointerToCast = currentThread->mySems->getNachosPointer( semId );
  //printf("Valor direccion %ld\n",pointerToCast);
  if ( pointerToCast !=  -1 )
  {
    //printf("%s\n","Hago wait" );
    Semaphore* sem = (Semaphore*)pointerToCast;
    sem->P(); // then wait
    machine->WriteRegister( 2, 0 );
  }else
  {
    printf("%s\n","NO hago wait" );
    machine->WriteRegister( 2, -1 );
  }
}// Nachos_SemWait

void Nachos_SemSignal()
{
  //printf("%s\n", "Nachos signal");
  int semId = machine->ReadRegister( 4 );
  //printf("id: %d\n", semId );
  long pointerToCast = currentThread->mySems->getNachosPointer( semId );
  //printf("Valor direccion %ld\n",pointerToCast);
  if ( pointerToCast != -1 )
  {
    Semaphore* sem = (Semaphore*)pointerToCast;
    //printf("%s%d\n","Hago signal valor semaforo: ", sem->getValue() );
    sem->V(); // then wait
    //printf("%s%d\n","Hago signal valor semaforo: ", sem->getValue() );
    machine->WriteRegister( 2, 0 );
  }else
  {
    printf("%s\n","No Hago signal" );
    machine->WriteRegister( 2, -1 );
  }
}// Nachos_SemSignal

void Nachos_SemDestroy()
{
  int semId = machine->ReadRegister( 4 );
  //printf("id: %d\n", semId );
  long sem = currentThread->mySems->unRegisterSem( semId );
  //printf("Valor direccion %ld\n",sem);
  if ( sem != -1 )
  {
    Semaphore* s = (Semaphore*) sem;
    s->Destroy();
    machine->WriteRegister( 2, 0 );
  }else
  {
    // sem isnt destroied.
    machine->WriteRegister( 2, -1 );
  }
}// Nachos_SemDestroy

struct joinS
{
  long threadId;
  std::string fileName;
  Semaphore* s;
  inline joinS():threadId(-1),fileName(),s(NULL){}
};

joinS** execFiles = new joinS*[128];
BitMap* execFilesMap = new BitMap(128);

void Nachos_Exit(){
  /* This user program is done (status = 0 means exited normally). */
  //void Exit(int status);
  Thread* nextThread;
  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  DEBUG('t', "Finishing thread \"%s\"\n", currentThread->getName());

  //printf("currentThread: %ld\n", (long)currentThread);

  for(int ind = 0; ind < 128; ++ind){
    if(execFilesMap->Test(ind)){
      //printf("I'm a EXEC thread %ld\n", execFiles[ind]->threadId);
      if((long)currentThread == execFiles[ind]->threadId){
        if(execFiles[ind]->s != NULL){
          //printf("Someone is wainting for me: %ld\n", execFiles[ind]->threadId);
          execFiles[ind]->s->V();
        }
        else{
          //printf("No one is wainting for me: %ld\n", execFiles[ind]->threadId);
          delete execFiles[ind];
          execFilesMap->Clear(ind);
        }
      }
    }
  }

  nextThread = scheduler->FindNextToRun();
  if (nextThread != NULL) {
    scheduler->Run(nextThread);
  }else
  {
    currentThread->Finish();
  }
  interrupt->SetLevel(oldLevel);
  //returnFromSystemCall();
}//Nachos_Exit

void NachosExecThread( void* id)
{
  joinS* info = execFiles[(long)id];

  OpenFile *executable = fileSystem->Open(info->fileName.c_str());
  AddrSpace *space;

  if (executable == NULL) {
    printf("Unable to open file %s\n",info->fileName.c_str());
    return;
  }
  space = new AddrSpace( executable );
  delete currentThread->space; // i dont need may space anymore
  currentThread->space = space;

  delete executable;			        // close file
  space->InitRegisters();		// set the initial register values
  space->RestoreState();		// load page table register
  machine->Run();			// jump to the user progam
  printf("\t\t\t\t\tError\n");
  ASSERT(false);			// machine->Run never returns;
}

void Nachos_Exec(){
  /* Run the executable, stored in the Nachos file "name", and return the
  * address space identifier
  */
  //SpaceId Exec(char *name);

  DEBUG( 't', "Entering EXEC System call\n" );
  long r4 = machine->ReadRegister( 4 ); // read from register 4
  char name[256] = {0}; // need to store file name to unix create sc
  int c, i; // counter
  i = 0;

  do
  {
    machine->ReadMem( r4 , 1 , &c ); // read from nachos mem
    r4++;
    name[i++] = c;
  }while (c != 0 );
  std::string s = name;
  joinS* newE = new joinS();

  // We need to create a new kernel thread to execute the user thread
  Thread * newT = new Thread( "HILO EXEC" );
  long fileToExec = execFilesMap->Find();
  if(fileToExec == -1){
    machine->WriteRegister(2, fileToExec );
    return;
  }

  newE->threadId = (long) newT;
  newE->fileName = s;
  execFiles[fileToExec] = newE;

  newT->Fork( NachosExecThread, (void*) fileToExec ); // ojo se elimino warning
  machine->WriteRegister(2, fileToExec );
  returnFromSystemCall();	// This adjust the PrevPC, PC, and NextPC registers

  DEBUG( 't', "Exiting EXEC System call\n" );
}// Nachos_Exec

void Nachos_Join()
{
  /* Only return once the the user program "id" has finished.
  * Return the exit status.
  */
  //int Join(SpaceId id);

  DEBUG( 't', "Entering JOIN System call\n" );

  //First I need to read the SpaceID of the thread I must wait for
  long id = machine->ReadRegister( 4 ); // read from register 4
  //I need to make sure it is a valid thread
  if(execFilesMap->Test(id)){
    //I need to create a Semaphore asosieted with this SpaceID
    Semaphore* joinSem = new Semaphore("JOIN Semaphore", 0);
    execFiles[id]->s = joinSem;
    joinSem->P();
    //printf("%ld: %s\n", execFiles[id]->threadId, "just give me a sigh");
    delete execFiles[id];
    execFilesMap->Clear(id);
    machine->WriteRegister(2, 0);
  }
  else{ //invalid SpaceID
    //printf("%s\n", "Error JOIN: ¡Invalid SpaceID!");
    machine->WriteRegister(2, -1 );
  }
  returnFromSystemCall();	// This adjust the PrevPC, PC, and NextPC registers
}// Nachos_Join

void ExceptionHandler(ExceptionType which)
{
  int type = machine->ReadRegister(2);

  switch ( which ) {

    case SyscallException:
    switch ( type )
    {
      case SC_Halt:                 //System call # 0
      Nachos_Halt();
      break;
      case SC_Exit:                 //System call # 1
      Nachos_Exit();
      break;
      case SC_Exec:                 //System call # 2
      Nachos_Exec();
      break;
      case SC_Join:                 //System call # 3
      Nachos_Join();
      break;
      case SC_Create:               //System call # 4
      Nachos_Create();
      break;
      case SC_Open:                 //System call # 5
      Nachos_Open();
      break;
      case SC_Read:                 //System call # 6
      Nachos_Read();
      break;
      case SC_Write:                //System call # 7
      Nachos_Write();
      break;
      case SC_Close:                //System call # 8
      Nachos_Close();
      break;
      case SC_Fork:                 //System call # 9
      Nachos_Fork();
      break;
      case SC_Yield:                //System call # 10
      currentThread->Yield();
      returnFromSystemCall();
      break;
      case SC_SemCreate:            //System call # 11
      Nachos_SemCreate();
      returnFromSystemCall();
      break;
      case SC_SemDestroy:           //System call # 12
      Nachos_SemDestroy();
      returnFromSystemCall();
      break;
      case SC_SemSignal:            //System call # 13
      Nachos_SemSignal();
      returnFromSystemCall();
      break;
      case SC_SemWait:              //System call # 14
      Nachos_SemWait();
      returnFromSystemCall();
      break;
      default:
      printf("Unexpected syscall exception %d\n", type );
      ASSERT(false);
      break;
    }
    break;
    case PageFaultException:
    printf("\nPageFaultException\n");
    ASSERT(false);
    break;
    case ReadOnlyException:
    printf("\nReadOnlyException\n");
    ASSERT(false);
    break;
    case BusErrorException:
    printf("\nBusErrorException\n");
    ASSERT(false);
    break;
    case AddressErrorException:
    printf("\nAddressErrorException\n");
    ASSERT(false);
    break;
    case OverflowException:
    printf("\nOverflowException\n");
    ASSERT(false);
    break;
    case IllegalInstrException:
    printf("\nIllegalInstrException\n");
    ASSERT(false);
    break;
    case NumExceptionTypes:
    printf("\nNumExceptionTypes\n");
    ASSERT(false);
    break;
    default:
    printf("\nUnexpected exception %d\n", which );
    ASSERT(false);
    break;
  }
}
