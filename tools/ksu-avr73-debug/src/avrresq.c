#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
int main(int c,char**v){sleep(1); if(c>1) abort(); exit(3);}
