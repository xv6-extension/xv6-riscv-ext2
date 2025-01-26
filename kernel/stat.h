#define EXT2_T_DIR     0x4000   // Directory
#define EXT2_T_FILE    0x8000   // File
#define EXT2_T_DEVICE  0x6000   // Device
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device


struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  ushort type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};

//P changed the values
