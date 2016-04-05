#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
//#include <ext2_fs.h>
//#include <ext2fs/ext2_fs.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define BLKSIZE 1024

typedef unsigned int   u32;

typedef struct ext2_group_desc  GD;
typedef struct ext2_super_block SUPER;
typedef struct ext2_inode INODE;
typedef struct ext2_dir_entry_2 DIR;

GD    *gp;
SUPER *sp;
INODE *ip;
DIR   *dp;

int fd, iblock, argscount;
char buf[BLKSIZE];
char *pathitems[32];
char *path;
char *dev;