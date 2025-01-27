// On-disk file system format.
// Both the kernel and user programs use this header file.


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
  uint	s_first_ino; 		/* First non-reserved inode */
	ushort   s_inode_size; 		/* size of inode structure */
	ushort	s_block_group_nr; 	/* block group # of this superblock */
	uint	s_feature_compat; 	/* compatible feature set */
	uint	s_feature_incompat; 	/* incompatible feature set */
	uint	s_feature_ro_compat; 	/* readonly-compatible feature set */
	uchar	s_uuid[16];		/* 128-bit uuid for volume */
	char	s_volume_name[16]; 	/* volume name */
	char	s_last_mounted[64]; 	/* directory where last mounted */
	uint	s_algorithm_usage_bitmap;
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
  
		struct {
			ushort	l_i_frag;	/* Fragment number */
			ushort	l_i_fsize;	/* Fragment size */
			ushort	i_pad1;
			ushort	l_i_uid_high;	/* these 2 fields    */
			ushort	l_i_gid_high;	/* were reserved2[0] */
			uint	l_i_reserved2;
		} linux2;
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

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

