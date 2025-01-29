#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/printcodes.h"

int
main(int argc, char *argv[])
{
  printinfo(ROOT_FILES, 0);
  exit(0);
}