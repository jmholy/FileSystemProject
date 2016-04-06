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

//Level 1
void mount_root()
{

}
void mkdir()
{

}
void rmdir()
{

}

void ls()
{

}
void cd()
{

}
void pwd()
{

}
void creat()
{

}
void link()
{

}
void unlink()
{

}
void symlink()
{

}
void stat()
{

}
void chmod()
{

}
void touch()
{

}
void quit()
{

}
void chown(newOwner, pathname)
{

}
void chgrp(newGroup, pathname)
{

}
//Level 2
void open()
{

}
void close()
{

}
void read()
{

}
void write()
{

}
void lseek()
{

}
void cat()
{

}
void cp()
{

}
void mv()
{

}
//Level 3
void mount()
{

}
void unmount()
{

}
int checkPerm()
{

}
//Helper Functions
int get_block(int dev, int blk, char *buf)
{

}
int put_block(int dev, int blk, char *buf)
{

}
int ialloc(int dev)        // allocate an ino
{

}
int balloc(int dev)        // allocate a  bno
{

}
int idealloc(int dev, int ino) // deallocate ino
{

}
int bdealloc(int dev, int bno) // deallocate bno
{

}
int enter_name(MINODE *pip, int myino, char *myname)
{

}
int rmchild(MINODE *parent, char *name)
{

}
//main
int main(int agrc, char *agrv[], char *env[])
{

}