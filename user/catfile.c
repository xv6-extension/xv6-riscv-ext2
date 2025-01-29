#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/printcodes.h"

int
main(int argc, char *argv[])
{
  printinfo(CATFILE, (uint64)argv[1]);
  exit(0);
}