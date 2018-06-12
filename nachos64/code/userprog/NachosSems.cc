#include "NachosSems.h"

NachosSems::NachosSems()
{
  usage = 0;
  semaphores = new long[MAX_SEMS];
  openSemsMap = new BitMap(MAX_SEMS);
  // initialize pointer in NULL
  for ( int x = 0; x < MAX_SEMS; ++x )
  {
    semaphores[ x ] = -1;
  }
}

NachosSems::~NachosSems()
{
  if( usage == 1 ){
    //delete openSemsMap;
    //delete[] semaphores;
  }
}

void NachosSems::print()
{
  for (int x= 0; x < MAX_SEMS; ++x)
    printf("Valor en el vector de info: %ld\n", semaphores[x] );

}

int NachosSems::registerSem( long s )
{
  int freeSemSpace = this->openSemsMap->Find();

  if (-1 != freeSemSpace )
  {
    printf("Valor libre para registrar sem: %d\n", freeSemSpace );
    semaphores[ freeSemSpace ] = s;
  }
  print();
  return freeSemSpace;
}

long NachosSems::unRegisterSem( int id )
{
    if ( semaphores[ id ] != -1 )
    {
      this->openSemsMap->Clear( id );
      long copy = semaphores[ id ];
      semaphores[ id ] = -1;
      return copy;
    }
    return -1;
}
long NachosSems::getNachosPointer( int id )
{
  print();
  return semaphores[ id ];
}
void NachosSems::addSem()
{
  ++usage;
}
void NachosSems::delSem()
{
  --usage;
}
