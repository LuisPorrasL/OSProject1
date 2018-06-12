#include "syscall.h"

void hijo(int);
int main(){
	Fork(hijo);

	Exit(0);
}

void hijo(int dummy){
	Write("Yeah!", 5, 1);
}
