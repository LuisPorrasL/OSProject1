#include "syscall.h"

void hijo(int);
int id;
int main(){
	id = SemCreate(0);
	Fork(hijo);

	SemWait(id);
	Write("padre\n", 6, 1);
	SemDestroy(id);
	Exit(0);
}


void hijo(int dummy){
	Write( "hijo\n", 6, 1 );
	SemSignal(id);
}
