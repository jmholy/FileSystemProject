#include "type.h"
MINODE minode[100];
MINODE *root;
PROC proc[NPROC], *running;
int fd, iblock, argscount;
char buf[BLKSIZE];
char *pathitems[32];
char *path, *pathname, *pathname2;
char *dev;
char *cmd;

//Level 1
void init()
{
    int k = 0;
    proc[0].uid = 0;
    proc[0].pid = 1;
    proc[0].cwd = 0;
    proc[1].uid = 1;
    proc[1].pid = 2;
    proc[1].cwd = 0;
    for (k = 0; k < 100; k++)
    {
        minode[k].refCount = 0;
    }
    root = 0;
}
void mount_root()
{
    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        printf("Failed to open %s\n", dev);
        exit(0);
    }
    // read SUPER block
    get_block(fd, 1, buf);
    sp = (SUPER *)buf;

    // check for EXT2 magic number:

    printf("s_magic = %x\n", sp->s_magic);
    if (sp->s_magic != 0xEF53)
    {
        printf("NOT an EXT2 FS\n");
        exit(1);
    }
    root = iget(dev, 2);
    proc[0].cwd = iget(dev, 2);
    proc[1].cwd = iget(dev, 2);
}
void mkdir()
{

}
void rmdir()
{

}

void ls()
{
    int ino, dev = running->cwd->dev;
    MINODE *mip = running->cwd;

    if (pathname)
    {
        if (pathname[0]=='/')
        {
            dev = root->dev;
        }
        ino = getino(&dev, pathname);
        mip = iget(dev, ino);
    }
    if (S_ISDIR(mip.INODE->i_mode))
    {
        get_block(fd, mip.INODE->i_block[0], buf);
        dp = (DIR *)buf;
        char *cur = buf;
        while (cur < buf + BLKSIZE)
        {
            printf("%s  \n", dp->name);
            cur += dp->rec_len;
            dp = (DIR *)cur;
        }
    }
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
int lseek(int fd, int position)
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
    lseek(fd, (long)blk * BLKSIZE, 0);
    read(fd, buf, BLKSIZE);
}
int put_block(int dev, int blk, char *buf)
{
    lseek(fd, (long)blk * BLKSIZE, 0);
    write(fd, buf, BLKSIZE);
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
int getino(int dev, char *pathname)
{

}
int getinohelp(int dev, char *pathname)
{

}
MINODE iget(int dev, ino)
{
    int k = 0;
    int blk = 0, offset = 0;
    for (k = 0; k < 100; k++)
    {
        if (minode[k].refCount > 0)
        {
            if (minode[k].dev == dev && minode[k].ino == ino)
            {
                minode[k].refCount++;
                return &minode[k];
            }
        }
    }
    for (k = 0; k < 100; k++)
    {
        if (minode[k].refCount == 0)
        {
            break;
        }
    }
    blk    = (ino-1)/8 + iblock;
    offset = (ino-1)%8;
    get_block(fd, blk, buf);
    ip = (INODE *)buf + offset;
    minode[k].inode = ip;
    minode[k].refCount = 1;
    minode[k].dev = dev;
    minode[k].ino = ino;
    minode[k].dirty = 0;
    minode[k].mounted = 0;
    return minode[k];
}
void iput(MINODE *mip)
{
    mip->refCount--;
    if (mip->refCount > 0)
    {
        return;
    }
    if (mip->dirty == 0)
    {
        return;
    }
    blk    = (ino-1)/8 + iblock;
    offset = (ino-1)%8;
    get_block(fd, blk, buf);
    ((INODE *)buf + offset) = mip->INODE;
    put_block(fd, blk, buf);
}
//main
int main(int agrc, char *agrv[], char *env[])
{
    if (argc <= 1)
    {
        printf("Incorrect Arguements, Use: a.out Diskname\n");
        exit(0);
    }
    dev = argv[1];
    printf("Welcome to Hunter and Mark's filesystem!\n");
    init();
    mountroot();
    printf("~HM:");
    scanf("%s %s %s", cmd, pathname, pathname2);
    if (strcmp(cmd, "ls") == 0)
    {
        ls();
    }
    printf("Press click to continue");
    getchar();
    exit(0);
}