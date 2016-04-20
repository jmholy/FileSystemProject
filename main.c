#include "type.h"
MINODE minode[100];
MINODE *root;
PROC proc[NPROC], *running;
int fd, iblock, argscount;
int blk = 0, offset = 0;
char buf[BLKSIZE];
char *pathitems[32];
char *path, *pathname, *parameter;
char *inputdev;
char *cmd;
char *names[64][64];

/*char cmdarrayinput[23] = {"mkdir","rmdir", "ls", "cd", "pwd", "creat", "link", "unlink",
                     "symlink", "stat", "chmod", "touch", "open", "close", "read",
                    "write", "lseek", "cat", "cp", "mv", "mount", "unmount", "help", "quit"};*/
/*void (*cmdarrayuse[23]) = {mymkdir, myrmdir, ls, cd, pwd, mycreat, mylink, myunlink,
                        mysymlink, mystat, mychmod, touch, myopen, myclose, myread,
                        mywrite, mylseek, cat, cp, mv, mount, unmount, help, quit};*/
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
        minode[k].ino = 0;
    }
    root = 0;
    running = &proc[0];
}
MINODE *iget(int dev, int ino)
{
    int k = 0;
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
    minode[k].INODE = *ip;
    minode[k].refCount = 1;
    minode[k].dev = dev;
    minode[k].ino = ino;
    minode[k].dirty = 0;
    minode[k].mounted = 0;
    return &minode[k];
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
    blk = (mip->ino-1)/8 + iblock;
    offset = (mip->ino-1)%8;
    get_block(fd, blk, buf);
    //strncpy(buf + offset,(char *)mip->INODE, sizeof(mip->INODE));
    put_block(fd, blk, buf);
}
void mount_root()
{
    fd = open(inputdev, O_RDWR);
    if (fd < 0)
    {
        printf("Failed to open %s\n", inputdev);
        exit(1);
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
    get_block(fd, 2, buf);
    gp = (GD *)buf;
    iblock = gp->bg_inode_table;
    root = iget(fd, 2);
    proc[0].cwd = iget(fd, 2);
    proc[1].cwd = iget(fd, 2);
}
void mymkdir()
{

}
void myrmdir()
{

}

void ls()
{
    int ino;
    int dev = running->cwd->dev;
    MINODE *mip = running->cwd;
    if (pathname)
    {
        if (pathname[0] == '/')
        {
            dev = root->dev;
        }
        ino = getino(&dev, pathname);
        mip = iget(dev, ino);
    }
    if (S_ISDIR(mip->INODE.i_mode))
    {
        get_block(fd, mip->INODE.i_block[0], buf);
        dp = (DIR *)buf;
        char *cur = buf;
        while (cur < (buf + BLKSIZE))
        {
            printf("%s   ", dp->name);
            cur += dp->rec_len;
            dp = (DIR *)cur;
        }
        printf("\n");
    }
}
void cd()
{

}
void pwd()
{
    printf("%s\n", running->cwd);

}
void mycreat()
{

}
void mylink()
{

}
void myunlink()
{

}
void mysymlink()
{

}
void mystat()
{

}
void mychmod()
{

}
void touch()
{

}
void quit()
{

}
void mychown(newOwner, pathname)
{

}
void chgrp(newGroup, pathname)
{

}
//Level 2
void myopen()
{

}
void myclose()
{

}
void myread()
{

}
void mywrite()
{

}
int mylseek(int fd, int position)
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
int get_block(int fd, int blk, char *buf)
{
    lseek(fd, (long)blk * BLKSIZE, 0);
    read(fd, buf, BLKSIZE);
}
int put_block(int fd, int blk, char *buf)
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
    char buf[BLOCK_SIZE];
    int k = 0;
    int inonum = 0;
    char *tok;
    MINODE *mip = running->cwd;

    if(pathname[0] == '/')
    {
        dev = root->dev;
        mip = root;
    }
    else
    {
        dev = running->cwd->dev;
    }
    tokenize(pathname);
    while (strcmp(names[k], "\0"))
    {
        inonum = search(mip, names[k]);
        if (inonum)
        {
            k++;
            mip = iget(dev, inonum);
        }
        else
        {
            return 0;
        }
    }
    return inonum;
}
int search (MINODE *mip, char *name)
{
    int i;
    char *cp, sbuf[BLKSIZE];
    DIR *dp;
    INODE *ip;

    ip = &(mip->INODE);
    for (i=0; i<12; i++){  // ASSUME DIRs only has 12 direct blocks
        if (ip->i_block[i] == 0)
            return 0;

        get_block(fd, ip->i_block[i], sbuf);
        dp = (DIR *)sbuf;
        cp = sbuf;
        while (cp < sbuf + BLKSIZE){
            if (strcmp(dp->name, name) == 0)
            {
                return dp->inode;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    return 0;
}
void tokenize(char *pathname)
{
    char *temp;
    int k = 0;
    temp = strtok(pathname, "/");
    strcpy(names[k], temp);
    k++;
    while(temp = strtok(NULL, "/"))
    {
        strcpy(names[k], temp);
        k++;
    }
    strcpy(names[k], "\0");
}

//main
int main(int argc, char *argv[], char *env[])
{
    char inputtemp[128];
    if (argc <= 1)
    {
        printf("Incorrect Arguements, Use: a.out Diskname\n");
        exit(0);
    }
    inputdev = argv[1];
    printf("Welcome to Hunter and Mark's filesystem!\n");
    init();
    mount_root();
    while(1)
    {
        printf("~HM:");
        fgets(inputtemp, 128, stdin);
        inputtemp[strlen(inputtemp)-1] = 0;
        cmd = strtok(inputtemp, " ");
        pathname = strtok(NULL, " ");
        parameter = strtok(NULL, " ");
        printf("%s %s %s\n", cmd, pathname, parameter);
        if (strcmp(cmd, "ls") == 0)
        {
            ls();
        }
    }
    printf("Press click to continue");
    getchar();
    exit(0);
}