// skipped: 
//in ilock function what is ad?
//what is S_IDIR
// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// there should be one superblock per disk device, but we run with
// only one device
struct ext2_superblock sb; 

static uint
ext2fs_free_block(char *bitmap)
{
  int i, j, mask;
  for(i = 0; i < sb.s_blocks_per_group * 8; i++)
  {
    for(j = 0; j < 8; j++)
    {
      mask = 1 << (7 - j);
      if ((bitmap[i] & mask) == 0)
      {
        bitmap[i] |= mask;
        return i * 8 + j;
      }
    }
  }
  return -1;
}
// Read the super block.
static void
readsb(int dev, struct ext2_superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
  printf("number of inodes is: %u\n",sb->ninodes);
  printf("number of blocks is: %u\n",sb->nblocks);
}


// Init fs
void
fsinit(int dev) {
  readsb(dev, &sb);
  if(sb.magic != EXT2_FSMAGIC)
    panic("invalid file system");
 // initlog(dev, &sb);
}

// Zero a block.
/*static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  //log_write(bp);
  brelse(bp);
}*/
// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  //log_write(bp);
  bwrite(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
// returns 0 if out of disk space.
/*static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
       // log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}*/
static uint
balloc(uint dev)
{
  int  fbit, zbno;
  struct group_desc gdesc;
  struct buf *bp1, *bp2;

  bp1 = bread(dev, 2);
  memmove(&gdesc, bp1->data, sizeof(gdesc));
  brelse(bp1);
  bp2 = bread(dev, gdesc.block_bitmap);

  fbit = ext2fs_free_block((char *)bp2->data);
  if (fbit > -1)
  {
    zbno = gdesc.block_bitmap + fbit;
    bwrite(bp2);
    bzero(dev, zbno);
    brelse(bp2);
    return zbno;
  }
  brelse(bp2);
  panic("ext2_balloc: out of blocks\n");
}

// Free a disk block.
/*static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
 // log_write(bp);
  brelse(bp);
}*/
// Free a disk block.
static void
bfree(int dev, uint b)
{
  int  mask;
  struct group_desc bgdesc;
  struct buf *bp1, *bp2;

  bp1 = bread(dev, 2);
  memmove(&bgdesc, bp1->data, sizeof(bgdesc));
  bp2 = bread(dev, bgdesc.block_bitmap);
  b -= bgdesc.block_bitmap;
  mask = 1 << (b % 8);

  if ((bp2->data[b / 8] & mask) == 0)
    panic("ext2fs_bfree: block already free\n");
  bp2->data[b / 8] = bp2->data[b / 8] & ~mask;
  bwrite(bp2);
  brelse(bp2);
  brelse(bp1);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at block
// sb.inodestart. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a table of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The in-memory
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in table: an entry in the inode table
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a table entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   table entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays in the table and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The itable.lock spin-lock protects the allocation of itable
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold itable.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

void
iinit()
{
  int i = 0;
  
  initlock(&itable.lock, "itable");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

static struct inode* iget(uint dev, uint inum);

// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode,
// or NULL if there is no free inode.



/*struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + ((inum-1) % sb.s_inodes_per_group);
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
    //  log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  printf("ialloc: no inodes\n");
  return 0;
}*/
struct inode* ialloc(uint dev, short type)
{
    int fbit, bno, iindex, inum;
  struct buf *bp1, *bp2, *bp3;
  struct ext2_dinode *din;
  struct group_desc bgdesc;
    bp1 = bread(dev, 2);
    memmove(&bgdesc, bp1->data, sizeof(bgdesc));
    brelse(bp1);

    bp2 = bread(dev, bgdesc.inode_bitmap);
    fbit = ext2fs_free_block((char *)bp2->data);
    if (fbit == -1){
      brelse(bp2);
      panic("ext2_ialloc: no inodes");
    }

    bno = bgdesc.inode_table + fbit / (BSIZE / sizeof(struct ext2_dinode));    
    iindex = fbit % (BSIZE / sizeof(struct ext2_dinode));
    bp3 = bread(dev, bno);
    din = (struct ext2_dinode *)bp3->data + iindex;
    memset(din, 0, sizeof(*din));
    if (type == T_DIR)      //folder?
      din->i_mode = EXT2_T_DIR;
    else if (type == T_FILE)
      din->i_mode = EXT2_T_FILE;
    bwrite(bp3);
    bwrite(bp2);
    brelse(bp3);
    brelse(bp2);

    inum =  fbit + 1;    //index of inode in the disk
    return iget(dev, inum);
}


// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
void
iupdate(struct inode *ip)
{
  printf("called iupdate\n");
  struct buf *bp;
  struct ext2_dinode din;
  struct group_desc gdes;
  int bno, iindex;
  uint *ad;
  bp = bread(ip->dev, 2);
  memmove(&gdes, bp->data, sizeof(gdes));
  brelse(bp);
  bno = gdes.inode_table + (ip->inum - 1) / (BSIZE/INODESIZE);
  iindex = (ip->inum - 1) % (BSIZE/INODESIZE);
  bp = bread(ip->dev, bno);
  memmove(&din, bp->data + iindex * INODESIZE, sizeof(din));
  if (ip->type == T_DIR)
    din.i_mode = EXT2_T_DIR;
  if (ip->type == T_FILE)
    din.i_mode = EXT2_T_FILE;
  din.i_links_count = ip->nlink;
  din.i_size = ip->size;
  din.i_dtime = 0;
  din.i_faddr = 0;
  din.i_file_acl = 0;
  din.i_flags = 0;
  din.i_generation = 0;
  din.i_gid = 0;
  din.i_mtime = 0;
  din.i_uid = 0;
  din.i_atime = 0;

  ad = ip->addrs;
  memmove(din.i_block, ad, sizeof(ad));
  memmove(bp->data + (iindex * INODESIZE), &din, sizeof(din));
  bwrite(bp);
  brelse(bp);
}
/*void
iupdate(struct inode *ip)
{
  printf("called iupdate\n");
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  //log_write(bp);
  brelse(bp);
}*/

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
  // printf("called iget for inum %d\n", inum);
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // Is the inode already in the table?
  empty = 0;
  for(ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&itable.lock);

  return ip;

}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
rootilock(struct inode *ip)
{
  struct buf *bp;
  struct group_desc group;
  struct ext2_dinode din;
  int bno;
  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  // acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, 2);
    memmove(&group,bp->data,sizeof(group));
    brelse(bp);
    bno=group.inode_table;
    bp = bread(ip->dev,bno);
    memmove(&din,bp->data+INODESIZE,sizeof(din));
    brelse(bp);
    // printf("inode in ilock: %d %d\n", ip->inum, din.i_mode);
    if ( GET_FILE_MODE(din.i_mode) == EXT2_T_DIR|| din.i_mode == T_DIR)
        ip->type = T_DIR;
    else
        ip->type = T_FILE;
    ip->major = 0;
    ip->minor = 0;
    ip->nlink = din.i_links_count;
    ip->size = din.i_size;
    memmove(ip->addrs, din.i_block, sizeof(ip->addrs));
    ip->valid = 1;
    if(ip->type == 0)
        panic("ilock: no type");
  }
}

void
ilock(struct inode *ip)
{
  struct buf *bp, *bp1;
  struct group_desc group;
  struct ext2_dinode din;
  int offset;
  int bno;
  if(ip == 0 || ip->ref < 1)
    panic("ilock");
  
  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    offset= ip->inum - 1;
    bp = bread(ip->dev, 2);
    memmove(&group,bp->data,sizeof(group));
    brelse(bp);
    bno=group.inode_table + offset / (BSIZE/INODESIZE);  
    bp1 = bread(ip->dev,bno);
    memmove(&din,bp1->data+(offset % (BSIZE/INODESIZE))*INODESIZE,sizeof(din));
    brelse(bp1);
    // printf("inode in ilock: %d %d\n", ip->inum, din.i_mode);
    if ( GET_FILE_MODE(din.i_mode) == EXT2_T_DIR|| din.i_mode == T_DIR)
        ip->type = T_DIR;
    else
        ip->type = T_FILE;
    ip->major = 0;
    ip->minor = 0;  
    ip->nlink = din.i_links_count;
    ip->size = din.i_size;
    memmove(ip->addrs, din.i_block, sizeof(ip->addrs));
    ip->valid = 1;
    if(ip->type == 0)
        panic("ilock: no type");
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  // printf("called iunlock for ip with inum %d\n", ip->inum);
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");
  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
/*void
iput(struct inode *ip)
{
  //struct ext2fs_addrs *ad;
  //acquiresleep(&ip->lock);
  printf("called iput");
  acquire(&itable.lock);
 // ad = (struct ext2fs_addrs *)ip->addrs;
  if(ip->valid && ip->nlink == 0){
    //acquire(&icache.lock);
    acquiresleep(&ip->lock);
    int r = ip->ref;
    //release(&icache.lock);
    release(&itable.lock);
    if(r == 1){
      // inode has no links and no other references: truncate and free.
     
      //ext2fs_ifree(ip);
      //ext2fs_itrunc(ip);
      itrunc(ip);
      ip->type = 0;
      ip->valid = 0;
      iupdate(ip);
      //ip->addrs = 0;
      releasesleep(&ip->lock);
      acquire(&itable.lock);
    }
  }
  releasesleep(&ip->lock);

  //acquire(&icache.lock);
  ip->ref--;
  if (ip->ref == 0){
    ad->busy = 0;
    ip->addrs = 0;
  }
  release(&itable.lock);
  //release(&icache.lock);

  return;
}*/
void
iput(struct inode *ip)
{
  acquire(&itable.lock);

  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // inode has no links and no other references: truncate and free.
    // ip->ref == 1 means no other process can have ip locked,
    // so this acquiresleep() won't block (or deadlock).
    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);
    acquire(&itable.lock);
  }

  ip->ref--;
  release(&itable.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  // printf("iunlockputing ip with inum %u\n", ip->inum);
  iunlock(ip);
  iput(ip);
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// returns 0 if out of disk space.
/*static uint
bmap(struct inode *ip, uint bn)
{
  printf("called bmap\n");
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr){
        a[bn] = addr;
       // log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}*/
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a, *b, *c;
  struct buf *bp, *bp1, *bp2;
  uint *ad = ip->addrs;

  if (bn < EXT2_NDIRECT){
    if ((addr = ad[bn]) == 0)
      ad[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= EXT2_NDIRECT;
  if (bn < EXT2_NINDIRECT){
    if ((addr = ad[EXT2_IINDIRECT]) == 0)
      ad[EXT2_IINDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0)
      a[bn] = addr = balloc(ip->dev);
    brelse(bp);
    return addr;
  }
  bn -= EXT2_NINDIRECT;

  if (bn < EXT2_NDINDIRECT){
    if ((addr = ad[EXT2_IDINDIRECT]) == 0)
      ad[EXT2_IDINDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn / EXT2_NINDIRECT]) == 0)
      a[bn / EXT2_NINDIRECT] = addr = balloc(ip->dev);
    bp1 = bread(ip->dev, addr);
    b = (uint *)bp1->data;
    if ((addr = b[bn % EXT2_NINDIRECT]) == 0)
      b[bn % EXT2_NINDIRECT] = addr = balloc(ip->dev);
    brelse(bp);
    brelse(bp1);
    return addr;
  }
  bn -= EXT2_NDINDIRECT;

  if (bn < EXT2_NTINDIRECT){
    if ((addr = ad[EXT2_ITINDIRECT]) == 0)
      ad[EXT2_ITINDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn / EXT2_NDINDIRECT]) == 0)
      a[bn / EXT2_NDINDIRECT] = addr = balloc(ip->dev);
    bp1 = bread(ip->dev, addr);
    b = (uint *)bp1->data;
    bn %= EXT2_NDINDIRECT;
    if ((addr = b[bn / EXT2_NINDIRECT]) == 0)
      b[bn / EXT2_NINDIRECT] = addr = balloc(ip->dev);
    bp2 = bread(ip->dev, addr);
    c = (uint *)bp2->data;
    if ((addr = c[bn % EXT2_NINDIRECT]) == 0)
      c[bn % EXT2_NINDIRECT] = addr = balloc(ip->dev);
    brelse(bp);
    brelse(bp1);
    brelse(bp2);
    return addr;
  }
  panic("ext2_bmap: block number out of range\n");
}
/*static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a , *b , *c;
  struct buf *bp, *bp1, *bp2;

  if (bn < EXT2_NDIRECT){
    if ((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= EXT2_NDIRECT;
  if (bn < EXT2_NINDIRECT){
    if ((addr = ip->addrs[EXT2_IINDIRECT]) == 0)
      ip->addrs[EXT2_IINDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0)
      a[bn] = addr = balloc(ip->dev);
    brelse(bp);
    return addr;
  }
  bn -= EXT2_NIDIRECT;

  if (bn < EXT2_NDINDIRECT){
    if ((addr = ip->addrs[EXT2_IDINDIRECT]) == 0)
      ip->addrs[EXT2_IDINDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn / EXT2_NIDIRECT]) == 0)
      a[bn / EXT2_NIDIRECT] = addr = balloc(ip->dev);
    bp1 = bread(ip->dev, addr);
    b = (uint *)bp1->data;
    if ((addr = b[bn / EXT2_NIDIRECT]) == 0)
      b[bn / EXT2_NIDIRECT] = addr = balloc(ip->dev);
    brelse(bp);
    brelse(bp1);
    return addr;
  }
  bn -= EXT2_NDINDIRECT;

  if (bn < EXT2_NTINDIRECT){
    if ((addr = ip->addrs[EXT2_ITINDIRECT]) == 0)
      ip->addrs[EXT2_ITINDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn / EXT2_NIDIRECT]) == 0)
      a[bn / EXT2_NIDIRECT] = addr = balloc(ip->dev);
    bp1 = bread(ip->dev, addr);
    b = (uint *)bp1->data;
    if ((addr = b[bn / EXT2_NIDIRECT]) == 0)
      b[bn / EXT2_NIDIRECT] = addr = balloc(ip->dev);
    bp2 = bread(ip->dev, addr);
    c = (uint *)bp2->data;
    if ((addr = c[bn / EXT2_NIDIRECT]) == 0)
      c[bn / EXT2_NIDIRECT] = addr = balloc(ip->dev);
    brelse(bp);
    brelse(bp1);
    brelse(bp2);
    return addr;
  }
  panic("ext2_bmap: block number out of range\n");
}
*/


// Truncate inode (discard contents).
// Caller must hold ip->lock.
/*void
itrunc(struct inode *ip)
{
  printf("called itrunc\n");
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}*/
void
itrunc(struct inode *ip)
{

  printf("called itrunc\n");
  int i,j,k;
  struct buf *bp1, *bp2 , *bp3;
  uint *a , *b , *c;
  //direct
  for (int i = 0; i < EXT2_NDIR_BLOCKS; i++){
    if (ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);    //check free function
      ip->addrs[i] = 0;
    }
  }
  //indirect
   if (ip->addrs[EXT2_IND_BLOCK]){
    bp1 = bread(ip->dev, ip->addrs[EXT2_IND_BLOCK]);
    a = (uint *)bp1->data;
    for (i = 0; i < EXT2_INDIRECT; i++){
      if(a[i]){
        bfree(ip->dev, a[i]);
        a[i] = 0;
      }
    }
    brelse(bp1);
    bfree(ip->dev, ip->addrs[EXT2_IND_BLOCK]);
    ip->addrs[EXT2_IND_BLOCK] = 0;
  }

  // for double indirect blocks
  if (ip->addrs[EXT2_DIND_BLOCK]){
    bp1 = bread(ip->dev, ip->addrs[EXT2_DIND_BLOCK]);
    a = (uint *)bp1->data;
    for (i = 0; i < EXT2_INDIRECT; i++){
      if(a[i]){
        bp2 = bread(ip->dev, a[i]);
	b = (uint *)bp2->data;
	for (j = 0; j < EXT2_INDIRECT; j++){
          if(b[j]){
	    bfree(ip->dev, b[j]);
            b[j] = 0;
	  }
	}
	brelse(bp2);
	bfree(ip->dev, a[i]);
	a[i] = 0;
      }
    }
    brelse(bp1);
    bfree(ip->dev, ip->addrs[EXT2_DIND_BLOCK]);
    ip->addrs[EXT2_DIND_BLOCK] = 0;
  }

  // for triple indirect blocks
  if (ip->addrs[EXT2_TIND_BLOCK]){
    bp1 = bread(ip->dev, ip->addrs[EXT2_TIND_BLOCK]);
    a = (uint *)bp1->data;
    for (i = 0; i < EXT2_INDIRECT; i++){
      if(a[i]){
        bp2 = bread(ip->dev, a[i]);
	b = (uint *)bp2->data;
	for (j = 0; j < EXT2_INDIRECT; j++){
	  if (b[j]){
	    bp3 = bread(ip->dev, b[j]);
	    c = (uint *)bp3->data;
	    for (k = 0; k < EXT2_INDIRECT; k++){
	      if (c[k]){
	        bfree(ip->dev, c[k]);
		c[k] = 0;
	      }
	    }
	    brelse(bp3);
	    bfree(ip->dev, b[j]);
	    b[j] = 0;
	  }
	}
	brelse(bp2);
	bfree(ip->dev, a[i]);
	a[i] = 0;
      }
    }
    brelse(bp1);
    bfree(ip->dev, ip->addrs[EXT2_TIND_BLOCK]);
    ip->addrs[EXT2_TIND_BLOCK] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// Read data from inode.
// Caller must hold ip->lock.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
int
readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot = 0; tot < n; tot += m, off += m, dst += m){
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off % BSIZE);
    //memmove((char*)dst, bp->data + off % BSIZE, m);
    either_copyout(user_dst, dst, bp->data + (off % BSIZE), m);
    brelse(bp);
  }
  return n;

  /*if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    uint addr = bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;*/
}

// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
int
writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  printf("called writei\n");
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    uint addr = bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      brelse(bp);
      break;
    }
   // log_write(bp);
    brelse(bp);
  }

  if(off > ip->size)
    ip->size = off;

  // write the i-node back to disk even if the size didn't change
  // because the loop above might have called bmap() and added a new
  // block to ip->addrs[].
  iupdate(ip);

  return tot;
}

// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct ext2_dirent de;
  char file_name[EXT2_NAME_LEN + 1];
  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += de.rec_len){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inode == 0)
      continue;
    strncpy(file_name, de.name, de.name_len);
    file_name[de.name_len] = '\0';
    if((namecmp(name, file_name+1) == 0 && de.name[0]=='_') || namecmp(name, file_name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inode;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

void
print_root_files(void)
{
  uint off;
  struct ext2_dirent de;
  struct inode *dp;
  printf("name | inum\n");
  dp = iget(ROOTDEV, EXT2_ROOTINO);
  ilock(dp);
  for(off = 0; off < dp->size; off += de.rec_len){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inode == 0)
      continue;
    printf("%s | %u\n", de.name, de.inode);
  }
  iunlockput(dp);
  exit(0);
}

void
print_cwd_files(void)
{
  uint off;
  struct ext2_dirent de;
  struct inode *cwd_inode;
  printf("name | inum\n");
  cwd_inode=myproc()->cwd;
  ilock(cwd_inode);
  for(off = 0; off < cwd_inode->size; off += de.rec_len){
    if(readi(cwd_inode, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inode == 0)
      continue;
    printf("%s | %u\n", de.name, de.inode);
  }
  iunlockput(cwd_inode);
  exit(0);
}

void
print_cat_file(char * name)
{
  struct inode *dp, *fi;
  uint poff;
  dp = myproc()->cwd;
  fi = dirlookup(dp, name, &poff);
  char b[fi->size];
  readi(fi, 0, (uint64)b, 0, sizeof(b));
  printf("%s\n", b);
  exit(0);
}

// Write a new directory entry (name, inum) into the directory dp.
// Returns 0 on success, -1 on failure (e.g. out of disk blocks).
int
dirlink(struct inode *dp, char *name, uint inum)
{
  printf("called dirlink\n");
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, EXT2_ROOTINO);
  else
  {
    ip = idup(myproc()->cwd);
  }

  if (*(path+1))
  {
    if (!ip->valid && ip->inum == EXT2_ROOTINO)
      rootilock(ip);
  }

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[EXT2_DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
