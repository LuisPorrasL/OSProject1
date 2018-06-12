#ifndef NACHOSSEMS_H
#define NACHOSSEMS_H

//#include "synch.h"
#include "bitmap.h"

#define MAX_SEMS 6

class NachosSems
{
public:
  NachosSems();
  ~NachosSems();

  int registerSem( long s ); // Register the Nachos sem pointer
  long unRegisterSem( int id );      // Unregister the fNachos sem pointer
  long getNachosPointer( int id );
  void addSem();		// If a user thread is using this table, add it
  void delSem();		// If a user thread is using this table, delete it
  void print();

  private:
  BitMap * openSemsMap;	// A bitmap to control our vector
  long* semaphores;		// A vector with user created semaphores
  int usage;			// How many threads are using this table

};

#endif
