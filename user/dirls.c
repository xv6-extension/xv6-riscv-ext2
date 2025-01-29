#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/printcodes.h"

int
main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("enter directory\n");
    exit(0);
  }

  if (chdir(argv[1]) < 0)
  {
    printf("invalid directory\n");
    exit(0);
  }

  printinfo(CWDFILES, 0);
  exit(0);
}