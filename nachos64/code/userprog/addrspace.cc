// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace( AddrSpace* other)
{
	numPages = other->numPages;
	pageTable = new TranslationEntry[ numPages ];

	// iterar menos las 8 paginas de pila
	long dataAndCodePages = numPages - 8;
	long index;
	for (index = 0; index < dataAndCodePages; ++ index )
	{
		pageTable[index].virtualPage =  other->pageTable[index].virtualPage;
		pageTable[index].physicalPage = other->pageTable[index].physicalPage;
		pageTable[index].valid = other->pageTable[index].valid;
		pageTable[index].use = other->pageTable[index].use;
		pageTable[index].dirty = other->pageTable[index].dirty;
		pageTable[index].readOnly = other->pageTable[index].readOnly;
	}
	// 8 paginas para la pila
	/*
	printf("%s\n", "\nUsed pages: " );
	MemBitMap->Print();
	printf("\n");
	*/
	for (index = dataAndCodePages; index < numPages ; ++ index )
	{
		pageTable[index].virtualPage =  index;	// for now, virtual page # = phys page #
		pageTable[index].physicalPage = MemBitMap->Find();
		pageTable[index].valid = true;
		pageTable[index].use = false;
		pageTable[index].dirty = false;
		pageTable[index].readOnly = false;
	}
}

AddrSpace::AddrSpace(OpenFile *executable)
{
	/*
	printf("%s\n", "\nUsed pages: " );
	MemBitMap->Print();
	printf("\n");
	*/

	NoffHeader noffH;
	unsigned int i, size;

	executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
	if ((noffH.noffMagic != NOFFMAGIC) &&
	(WordToHost(noffH.noffMagic) == NOFFMAGIC))
	SwapHeader(&noffH);
	ASSERT(noffH.noffMagic == NOFFMAGIC);

	// how big is address space?
	size = noffH.code.size + noffH.initData.size + noffH.uninitData.size
	+ UserStackSize;	// we need to increase the size
	// to leave room for the stack
	numPages = divRoundUp(size, PageSize);
	size = numPages * PageSize;

	ASSERT(numPages <= NumPhysPages);		// check we're not trying
	// to run anything too big --
	// at least until we have
	// virtual memory

	DEBUG('a', "Initializing address space, num pages %d, size %d\n",
	numPages, size);
	// first, set up the translation
	pageTable = new TranslationEntry[numPages];
	for (i = 0; i < numPages; i++) {
		pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
		pageTable[i].physicalPage = MemBitMap->Find();
		pageTable[i].valid = true;
		pageTable[i].use = false;
		pageTable[i].dirty = false;
		pageTable[i].readOnly = false;  // if the code segment was entirely on
		// a separate page, we could set its
		// pages to be read-only
	}

	// zero out the entire address space, to zero the unitialized data segment
	// and the stack segment
	//bzero(machine->mainMemory, size);

	// then, copy in the code and data segments into memory

	/* Para el segmento de codigo*/

	int x = noffH.code.inFileAddr;
	int y = noffH.initData.inFileAddr;
	int index;
	int codeNumPages = divRoundUp(noffH.code.size, numPages);
	int segmentNumPages = divRoundUp(noffH.initData.size, numPages);

	DEBUG('a', "Initializing code segment, at 0x%x, size %d, numero de paginas %d\n",
	noffH.code.virtualAddr, noffH.code.size, codeNumPages);

	for (index = 0; index < codeNumPages; ++ index )
	{
		executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *128 ] ),
		PageSize, x );
		x+=128;
	}

	if (noffH.initData.size > 0) {
		DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
		noffH.initData.virtualAddr, noffH.initData.size);
		for (index = codeNumPages; index < segmentNumPages; ++ index )
		{
			executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *128 ] ),
			PageSize, y );
			y+=128;
		}
	}

	// then, copy in the code and data segments into memory
	/*
	if (noffH.code.size > 0) {
	DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
	noffH.code.virtualAddr, noffH.code.size);
	executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
	noffH.code.size, noffH.code.inFileAddr);
}


if (noffH.initData.size > 0) {
DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
noffH.initData.virtualAddr, noffH.initData.size);
executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
noffH.initData.size, noffH.initData.inFileAddr);
}
*/
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
	delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
	int i;

	for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

	// Initial program counter -- must be location of "Start"
	machine->WriteRegister(PCReg, 0);

	// Need to also tell MIPS where next instruction is, because
	// of branch delay possibility
	machine->WriteRegister(NextPCReg, 4);

	// Set the stack register to the end of the address space, where we
	// allocated the stack; but subtract off a bit, to make sure we don't
	// accidentally reference off the end!
	machine->WriteRegister(StackReg, numPages * PageSize - 16);
	DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState()
{}

	//----------------------------------------------------------------------
	// AddrSpace::RestoreState
	// 	On a context switch, restore the machine state so that
	//	this address space can run.
	//
	//      For now, tell the machine where to find the page table.
	//----------------------------------------------------------------------

	void AddrSpace::RestoreState()
	{
		machine->pageTable = pageTable;
		machine->pageTableSize = numPages;
	}
