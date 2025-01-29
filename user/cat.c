#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

/*char buf[512];

void
cat(int fd)
{
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0) {
    if (write(1, buf, n) != n) {
      fprintf(2, "cat: write error\n");
      exit(1);
    }
  }
  if(n < 0){
    fprintf(2, "cat: read error\n");
    exit(1);
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    cat(0);
    exit(0);
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      fprintf(2, "cat: cannot open %s\n", argv[i]);
      exit(1);
    }
    cat(fd);
    close(fd);
  }
  exit(0);
}*/
#define MAX_FILENAME_LENGTH 128

int catsys(char *filename) {
    return syscall(SYS_catsys, filename);
}

void cat_file(char *filename) {
    int result = catsys(filename);  
    if (result < 0) {
        printf(2, "cat: error reading file %s\n", filename);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
      
        exit();
    }

    
    for (int i = 1; i < argc; i++) {
        cat_file(argv[i]);
    }

    exit();
}

