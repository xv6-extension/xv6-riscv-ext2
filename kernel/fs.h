// On-disk file system format.
// Both the kernel and user programs use this header file.
#define INODESIZE 128
#define EXT2_INDIRECT                   (BSIZE / sizeof(uint))
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)
#define EXT2_IDIRECT 0
#define EXT2_NDIRECT 12
#define EXT2_IINDIRECT (EXT2_NDIRECT)
#define EXT2_NINDIRECT (BSIZE / sizeof(uint))
#define EXT2_IDINDIRECT (EXT2_NDIRECT + 1)
#define EXT2_NDINDIRECT (EXT2_NINDIRECT * EXT2_NINDIRECT)
#define EXT2_ITINDIRECT (EXT2_IDINDIRECT + 1)
#define EXT2_NTINDIRECT (EXT2_NINDIRECT * EXT2_NDINDIRECT)
#define EXT2_MAXFILE (EXT2_NDIRECT + EXT2_NINDIRECT + EXT2_NDINDIRECT + EXT2_NTINDIRECT)
#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size
#define EXT2_ROOTINO 2
// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct ext2_superblock {
  uint ninodes;      // Number of inodes.
  uint nblocks;      // Number of data blocks
  uint s_r_blocks_count;
  uint s_free_blocks_count;
  uint s_free_inodes_count;
  uint s_first_data_block;
  uint s_log_block_size;
  uint s_log_frag_size;
  uint s_blocks_per_group;
  uint s_frags_per_group;
  uint s_inodes_per_group;
  uint s_mtime;
  uint s_wtime;
  ushort s_mnt_count;
  ushort s_max_mnt_count;
  ushort magic;       // Must be FSMAGIC
  ushort s_state;
  ushort s_errors;
  ushort s_minor_rev_level;
  uint s_lastcheck;
  uint s_checkinterval;
  uint s_creator_os;
  uint s_rev_level;
  ushort s_def_resuid;
  ushort s_def_reguid;
  uint size;         // Size of file system image (blocks)
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};
struct group_desc
{
	uint	block_bitmap;		/* Blocks bitmap block */
	uint	inode_bitmap;		/* Inodes bitmap block */
	uint	inode_table;		/* Inodes table block */
	ushort	free_blocks_count;	/* Free blocks count */
	ushort	free_inodes_count;	/* Free inodes count */
	ushort	used_dirs_count;	/* Directories count */
	ushort	pad;
	uint	reserved[3];
};
struct superblock {
  uint magic;
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define EXT2_FSMAGIC 0xEF53
#define FSMAGIC 0x10203040


#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct ext2_dinode {
 	ushort	i_mode;		/* File mode */
	ushort	i_uid;		/* Low 16 bits of Owner Uid */
	uint	i_size;		/* Size in bytes */
	uint	i_atime;	/* Access time */
	uint	i_ctime;	/* Creation time */
	uint	i_mtime;	/* Modification time */
	uint	i_dtime;	/* Deletion Time */
	ushort	i_gid;		/* Low 16 bits of Group Id */
	ushort	i_links_count;	/* Links count */
	uint	i_blocks;	/* Blocks count */
	uint	i_flags;	
  uint i_osd1;
  uint i_block[15];
  uint	i_generation;	/* File version (for NFS) */
	uint	i_file_acl;	/* File ACL */
	uint	i_dir_acl;	/* Directory ACL */
	uint	i_faddr;
};
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))
#define EXT2_IPB           (BSIZE / sizeof(struct ext2_dinode))
// Block containing inode i
#define EXT2_IBLOCK(i, sb)     ((i-1)/sb.s_inodes_per_group)
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14
#define EXT2_DIRSIZ 255
#define EXT2_NAME_LEN 255

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
struct ext2_dirent {
	uint	inode;			/* Inode number */
	ushort	rec_len;		/* Directory entry length */
	uchar	name_len;		/* Name length */
	uchar	file_type;
	char	name[EXT2_NAME_LEN];	/* File name */
};

void
print_root_files(void);

void
print_cwd_files(void);

void
print_cat_file(char *);