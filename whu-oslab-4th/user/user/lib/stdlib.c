#include "def.h"
#include "panic.h"
#include "output.h"
#include "func.h"

void panic(char *m)
{
    puts(m);
    exit(-100);
}
