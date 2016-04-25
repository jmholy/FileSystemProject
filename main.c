#include "type.h"
MINODE minode[100];
MINODE *root;
PROC proc[NPROC], *running;
int fd, iblock, argscount;
int imap, bmap, ninodes, nblocks;
int blk = 0, offset = 0;
char buf[BLKSIZE];
char *pathitems[32];
char *path, *pathname, *parameter;
char *inputdev;
char *cmd;
char *names[64][64];

char *cmdarrayinput[24] = {"mkdir","rmdir", "ls", "cd", "pwd", "creat", "link", "unlink",
                     "symlink", "stat", "chmod", "touch", "open", "close", "read",
                    "write", "lseek", "cat", "cp", "mv", "mount", "umount", "help", "quit"};
void (*cmdarry[24])(void);
//Level 1
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
    INODE *ip;
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
    ip = (INODE *)buf + offset;
    *ip = mip->INODE;
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
    ninodes = sp->s_inodes_count;
    nblocks = sp->s_blocks_count;
    get_block(fd, 2, buf);
    gp = (GD *)buf;
    iblock = gp->bg_inode_table;
    imap = gp->bg_inode_bitmap;
    bmap = gp->bg_block_bitmap;
    root = iget(fd, 2);
    proc[0].cwd = iget(fd, 2);
    proc[1].cwd = iget(fd, 2);
}
void mymkdir()
{
    MINODE *mip;
    MINODE *pip;
    int dev;
    int pino;
    char *parent, *child;
    if (pathname)
    {
        if (pathname[0] == '/')
        {
            mip = root;
            dev = root->dev;
        }
        else
        {
            mip = running->cwd;
            dev = running->cwd->dev;
        }
        child = basename(pathname);
        parent = dirname(pathname);
        pino = getino(&dev, parent);
        pip = iget(dev, pino);
        if(S_ISDIR(pip->INODE.i_mode))
        {
            if (search(pip, child))
            {
                printf("%s already exists!\n", child);
                return;
            }
            mymkdirhelp(pip, child);
            //inc parents inode link count by 1
            pip->INODE.i_links_count++;
            //touch its atime and mark it dirty
            pip->INODE.i_atime = time(0L);
            pip->dirty = 1;
        }
    }
    iput(pip);
}
int mymkdirhelp(MINODE *pip, char *name)
{
    int ino, bno, dev, k;
    MINODE  *mip;
    INODE *ip;
    char buf[BLKSIZE], *cp;
    dev = pip->dev;
    ino = ialloc(dev);
    bno = balloc(dev);
    mip = iget(dev, ino);
    ip = &mip->INODE;
    ip->i_mode = 0x41ED;
    ip->i_uid = running->uid;
    ip->i_gid = running->gid;
    ip->i_size = BLKSIZE;
    ip->i_links_count = 2;
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 2;
    ip->i_block[0] = bno;
    for (k = 1; k < 15; k++)
    {
        ip->i_block[k] = 0;
    }
    mip->dirty = 1;

    cp = buf;
    dp = (DIR *)cp;
    dp->inode = ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    strcpy(dp->name, ".");
    cp += dp->rec_len;
    dp = (DIR *)cp;
    dp->inode = pip->ino;
    dp->rec_len = 1012;
    dp->name_len = 2;
    strcpy(dp->name, "..");

    put_block(dev, bno, buf);
    iput(mip);
    enter_name(pip, ino, name);
}
int enter_name(MINODE *pip, int myino, char *myname)
{
    int k, ideal, nlen, remaining, needed, flag = 0;
    char buf[BLKSIZE], *cp;
    nlen = strlen(myname);
    needed = 4 * ((8 + nlen + 3) / 4);
    for(k = 0; k < 12; k++)
    {
        if (pip->INODE.i_block[k] == 0)
        {
            break;
        }

        get_block(fd, pip->INODE.i_block[k], buf);

        cp = buf;
        dp = (DIR *) cp;

        while (cp+dp->rec_len < buf + BLKSIZE)
        {
            cp += dp->rec_len;
            dp = (DIR *) cp;
        }

        ideal = 4 * ((8 + dp->name_len + 3) / 4);
        remaining = dp->rec_len - ideal;


        if (remaining >= needed)
        {
            flag = 1;
            dp->rec_len = ideal;
            cp += ideal;
            dp = (DIR *)cp;
            dp->inode = myino;
            dp->rec_len = remaining;
            dp->name_len = nlen;
            strcpy(dp->name, myname);
            put_block(fd, pip->INODE.i_block[k], buf);
        }

    }

    if (flag == 0)
    {
        pip->INODE.i_block[k] = balloc(pip->dev);

        get_block(fd, pip->INODE.i_block[k], buf);

        dp = (DIR *) buf;

        dp->inode = myino;
        dp->rec_len = remaining;
        dp->name_len = nlen;
        strcpy(dp->name, myname);
        put_block(fd, pip->INODE.i_block[k], buf);
    }

}
void myrmdir()
{
    int dev, ino, pino, k;
    char *cp, buf[BLKSIZE], *name, *temppath;
    MINODE *mip, *pip;
    temppath = pathname;
    dev = running->cwd->dev;
    ino = getino(&dev, pathname);
    mip = iget(dev, ino);
    name = basename(temppath);
    if (running->uid == 0 || mip->INODE.i_uid == running->uid)
    {
        printf("mode = %x\n", mip->INODE.i_mode);
        if (S_ISDIR(mip->INODE.i_mode))
        {
            printf("refCount = %d\n", mip->refCount);
            if (mip->refCount == 1) {
                if (mip->INODE.i_links_count > 2) {
                    printf("Directory is not empty!\n");
                }
                else {
                    get_block(dev, mip->INODE.i_block[0], buf);
                    cp = buf + 12;
                    dp = (DIR *) cp;
                    if (dp->rec_len != 1012) {
                        printf("Directory is not empty!\n");
                    }
                    else {
                        truncate(mip); // changed this to use truncate
                        idealloc(dev, ino);
                        iput(mip);
                        pino = dp->inode;
                        pip = iget(dev, pino);
                        rmchild(pip, name);
                        pip->INODE.i_links_count--;
                        pip->INODE.i_atime = pip->INODE.i_mtime = time(0L);
                        pip->dirty = 1;
                        iput(pip);
                    }
                }
            }
        }
        else
        {
            printf("Cannot rmdir a file!\n");
        }
    }
    else
    {
        printf("Permissions insufficient!\n");
    }
    iput(mip);
}
int rmchild(MINODE *mip, char *name)
{
    int i, ideal_len, k;
    int prevrec_len, currec_len, len;
    char *cp, buf[BLKSIZE], next;
    DIR *dp;
    INODE *ip;
    printf("%d\n", mip->ino);
    ip = &(mip->INODE);
    for (i=0; i<12; i++){  // ASSUME DIRs only has 12 direct blocks
        if (ip->i_block[i] == 0)
            return 0;

        get_block(fd, ip->i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;
        while (cp < buf + BLKSIZE){
            if (strncmp(dp->name, name, strlen(name)) == 0)
            {
                ideal_len = 4 * ((8 + dp->name_len + 3) / 4);
                if (dp->rec_len == BLKSIZE)//only entry
                {
                    bdealloc(mip->dev, ip->i_block[i]);
                    ip->i_block[i] = 0;
                    ip->i_size -= BLKSIZE;
                    k = i;
                    while (k < 12 && ip->i_block[k])
                    {
                        ip->i_block[k] = ip->i_block[k+1];
                        k++;
                    }
                }
                else if (dp->rec_len != ideal_len)//last, but not only entry
                {
                    currec_len = dp->rec_len;
                    cp -= prevrec_len;
                    dp = (DIR *)cp;
                    dp->rec_len += currec_len;
                }
                else//any other cases
                {
                    currec_len = dp->rec_len;
                    len = BLKSIZE - ((cp + currec_len) - buf);
                    memcpy(cp, cp + currec_len, len);
                    while(cp + dp->rec_len < buf + BLKSIZE - currec_len)
                    {
                        cp += dp->rec_len;
                        dp = (DIR *)cp;
                    }
                    dp->rec_len += currec_len;
                }
                put_block(mip->dev, ip->i_block[i], buf);
            }
            prevrec_len = dp->rec_len;
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
}
void ls()
{
    int ino, k, i = 0;
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
        while(mip->INODE.i_block[i])
        {
            get_block(fd, mip->INODE.i_block[0], buf);
            dp = (DIR *) buf;
            char *cur = buf;
            while (cur < (buf + BLKSIZE))
            {
                for (k = 0; k < dp->name_len; k++)
                {
                    putchar(dp->name[k]);
                }
                printf("\n");
                cur += dp->rec_len;
                dp = (DIR *) cur;
            }
            i++;
        }
    }
    if (mip != running->cwd)
    {
        iput(mip);
    }
}
void cd()
{
    int k;
    MINODE *temp;

    if (pathname)
    {
        k = getino(fd, pathname);
        temp = iget(fd, k);
        if (S_ISDIR(temp->INODE.i_mode))
        {
            iput(running->cwd);
            running->cwd = temp;
        }
        else
        {
            printf("Cannot cd into a file!\n");
        }

    }
    else
    {
        iput(running->cwd);
        running->cwd = iget(fd, 2);
    }
}
void pwd() // pwd
{
    if (running->cwd->ino == 2)
    {
        printf("/");
    }
    pwdrec(running->cwd);
    printf("\n");
}
void pwdrec(MINODE *cur) // recursive func for pwd
{
    int k = 0;
    MINODE *temp;
    char patharr[] = "..";
    if (cur->ino == 2)
    {
        return;
    }
    else
    {
        k = getino(fd, patharr);
        temp = iget(fd, k);
        pwdrec(temp);
        printchild(temp, cur->ino);
        iput(temp);
    }
}
void printchild(MINODE *mip, int inonum) // finds and prints child
{
    int i;
    char *cp, buf[BLKSIZE];
    DIR *dp;
    INODE *ip;

    ip = &(mip->INODE);
    for (i=0; i<12; i++){  // ASSUME DIRs only has 12 direct blocks
        if (ip->i_block[i] == 0)
            return;

        get_block(fd, ip->i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;
        while (cp < buf + BLKSIZE){
            if (dp->inode == inonum)
            {
                printf("/%s", dp->name);
                return;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    return;
}
void mycreat()
{
    MINODE *mip;
    MINODE *pip;
    int dev;
    int pino;
    char *parent, *child;
    if (pathname)
    {
        if (pathname[0] == '/')
        {
            mip = root;
            dev = root->dev;
        }
        else
        {
            mip = running->cwd;
            dev = running->cwd->dev;
        }
        child = basename(pathname);
        parent = dirname(pathname);
        pino = getino(&dev, parent);
        pip = iget(dev, pino);
        if(S_ISDIR(pip->INODE.i_mode))
        {
            if (search(pip, child))
            {
                printf("%s already exists!\n", child);
                return;
            }
            mycreathelp(pip, child);
            //touch its atime and mark it dirty
            pip->INODE.i_atime = time(0L);
            pip->dirty = 1;
        }
    }
    iput(pip);
}
int mycreathelp(MINODE *pip, char *name)
{
    int ino, bno, dev, k;
    MINODE  *mip;
    INODE *ip;
    char buf[BLKSIZE], *cp;
    dev = pip->dev;
    ino = ialloc(dev);
    mip = iget(dev, ino);
    ip = &mip->INODE;
    ip->i_mode = 0x81A4;
    ip->i_uid = running->uid;
    ip->i_gid = running->gid;
    ip->i_size = 0;
    ip->i_links_count = 1;
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 0;
    ip->i_block[0] = 0;
    for (k = 1; k < 15; k++)
    {
        ip->i_block[k] = 0;
    }
    mip->dirty = 1;
    iput(mip);
    enter_name(pip, ino, name);
}
void mylink()
{
    int ino, dev, pino;
    char *parent, *child;
    char *temppar = parameter;
    MINODE *mip, *pip;
    if (pathname[0] == '/')
    {
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }
    ino = getino(dev, pathname);
    mip = iget(dev, ino);
    if (S_ISREG(mip->INODE.i_mode) || S_ISLNK(mip->INODE.i_mode))
    {
        child = basename(temppar);
        parent = dirname(temppar);
        pino = getino(&dev, parent);
        pip = iget(dev, pino);
        if(S_ISDIR(pip->INODE.i_mode))
        {
            if(search(pip, child) == 0)
            {
                enter_name(pip, ino, child);
                mip->INODE.i_links_count++;
                mip->dirty = 1;
                pip->dirty = 1;
            }
            else
            {
                printf("Child already exists!\n");
            }
        }
        else
        {
            printf("Cannot link to a non-Directory destination!\n");
        }
        iput(pip);
    }
    iput(mip);
}
void myunlink()
{
    int ino, pino, dev;
    char *parent, child;
    MINODE *mip, *pip;
    dev = running->cew->dev;
    ino = getino(dev, pathname);
    mip = iget(dev, ino);
    if (S_ISREG(mip->INODE.i_mode) || S_ISLNK(mip->INODE.i_mode))
    {
        mip->INODE.i_links_count--;
        if (mip->INODE.i_links_count == 0)
        {
            truncate(mip);
        }
      
        child = basename(pathname);
        parent = dirname(pathname);
        pino = getino(dev, parent);
        pip = iget(dev, pino);
        rm_child(pip, child);
        pip->dirty = 1;
        iput(pip);
    }
    else
    {
        printf("Unable to unlink: Not a regular file or link\n"); 
    }
    mip->dirty = 1;
    iput(mip);
}
void truncate(MINODE *mip)
{
    int k;
    for (k = 0; k < 12; k++) 
    {
        if (mip->INODE.i_block[k] == 0) 
        {
            break;
        }
        else 
        {
            bdealloc(dev, mip->INODE.i_block[k]);
        }
    }
}
void mysymlink()
{
    int ino1, ino2, dev;
    MINODE *mip1, *mip2;
    char buf[BLKSIZE], temp[64], *cp;
    dev = running->cwd->dev;
    ino1 = getino(dev, pathname);
    mip1 = iget(dev, ino1);
    if (S_ISDIR(mip1->INODE.i_mode) || S_ISREG(mip1->INODE.i_mode))
    {
        strcpy(temp, pathname);
        pathname = parameter;
        ino2 = getino(dev, pathname);
        if (ino2 == 0)
        { 
            mycreat(); // cll creat to make a file with the second argument's name and path
            ino2 = getino(dev, pathname);
            mip2 = iget(dev, ino2);
            
            // Alright, I'm stuck here now. Not totally sure how to change to type to LNK
            // and also write the old file's pathname into the inode block.
            
        }
        else
        {
            printf("%s already exists", pathname);
        }
    }
    else
    {
        printf("%s is neither a file nor a directory\n", pathname);
    }
    iput(mip1);
}
void mystat()
{

}
void mychmod()
{

}
void touch()
{
    int ino, dev;
    MINODE *mip;
    if (pathname[0] == '/')
    {
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }
    ino = getino(&dev, pathname);
    mip = iget(dev, ino);
    mip->INODE.i_atime =  mip->INODE.i_mtime = time(0L);
    mip->dirty = 1;
    iput(mip);
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
void help()
{

}
void quit()
{
    int k = 0;
    for (k = 0; k < NMINODES; k++)
    {
        if (minode[k].refCount > 1) {
            minode[k].refCount = 1;
            iput(&minode[k]);
        }
    }
    printf("Quitting the program!\n");
    exit(1);
}
//Level 3
void mount()
{

}
void umount()
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
    int  i;
    char buf[BLKSIZE];
    get_block(dev, imap, buf);
    for (i=0; i < ninodes; i++){
        if (tst_bit(buf, i)==0){
            set_bit(buf,i);
            decFreeInodes(dev);

            put_block(dev, imap, buf);

            return i+1;
        }
    }
    printf("ialloc(): no more free inodes\n");
    return 0;
}
int balloc(int dev)        // allocate a  bno
{
    int  i;
    char buf[BLKSIZE];
    get_block(dev, bmap, buf);
    for (i=0; i < nblocks; i++){
        if (tst_bit(buf, i)==0){
            set_bit(buf,i);
            decFreeBlocks(dev);

            put_block(dev, bmap, buf);

            return i+1;
        }
    }
    printf("balloc(): no more free blocks\n");
}
int idealloc(int dev, int ino) // deallocate ino
{
    char buf[BLKSIZE];
    ino--;
    get_block(dev, imap, buf);
    clr_bit(buf, ino);
    incFreeInodes(dev);
    put_block(dev, imap, buf);
}
int bdealloc(int dev, int bno) // deallocate bno
{
    char buf[BLKSIZE];
    bno--;
    get_block(dev, bmap, buf);
    clr_bit(buf, bno);
    incFreeBlocks(dev);
    put_block(dev, bmap, buf);
}
int incFreeInodes(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_inodes_count++;
    put_block(dev, 1, buf);

    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_inodes_count++;
    put_block(dev, 2, buf);
}
int decFreeInodes(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_inodes_count--;
    put_block(dev, 1, buf);

    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_inodes_count--;
    put_block(dev, 2, buf);
}
int incFreeBlocks(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_blocks_count++;
    put_block(dev, 1, buf);

    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_blocks_count++;
    put_block(dev, 2, buf);
}
int decFreeBlocks(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_blocks_count--;
    put_block(dev, 1, buf);

    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_blocks_count--;
    put_block(dev, 2, buf);
}
int tst_bit(char *buf, int bit)
{
    int i, j;
    i = bit/8; j=bit%8;
    if (buf[i] & (1 << j))
        return 1;
    return 0;
}
int set_bit(char *buf, int bit)
{
    int i, j;
    i = bit/8; j=bit%8;
    buf[i] |= (1 << j);
}
int clr_bit(char *buf, int bit)
{
    int i, j;
    i = bit/8; j=bit%8;
    buf[i] &= ~(1 << j);
}
int getino(int dev, char *pathname)
{
    char buf[BLOCK_SIZE];
    int k = 0;
    int inonum = 0;
    char *tok;
    char temp[64];
    MINODE *mip = running->cwd;
    strcpy(temp, pathname);
    if(pathname[0] == '/')
    {
        dev = root->dev;
        mip = root;
    }
    else
    {
        dev = running->cwd->dev;
    }
    tokenize(temp);
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
    iput(mip);
    return inonum;
}
int search (MINODE *mip, char *name)
{
    int i;
    char *cp, buf[BLKSIZE];
    DIR *dp;
    INODE *ip;

    ip = &(mip->INODE);
    for (i=0; i<12; i++){  // ASSUME DIRs only has 12 direct blocks
        if (ip->i_block[i] == 0)
            return 0;

        get_block(fd, ip->i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;
        while (cp < buf + BLKSIZE){
            if (strncmp(dp->name, name, strlen(name)) == 0)
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
int cmdSearch()
{
    int k = 0;
    for (k = 0; k < 24; k++)
    {
        if (strcmp(cmd, cmdarrayinput[k]) == 0)
        {
            return k;
        }
    }
    return -1;
}
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

    cmdarry[0] = (void *)mymkdir;
    cmdarry[1] = (void *)myrmdir;
    cmdarry[2] = (void *)ls;
    cmdarry[3] = (void *)cd;
    cmdarry[4] = (void *)pwd;
    cmdarry[5] = (void *)mycreat;
    cmdarry[6] = (void *)mylink;
    cmdarry[7] = (void *)myunlink;
    cmdarry[8] = (void *)mysymlink;
    cmdarry[9] = (void *)mystat;
    cmdarry[10] = (void *)mychmod;
    cmdarry[11] = (void *)touch;
    cmdarry[12] = (void *)myopen;
    cmdarry[13] = (void *)myclose;
    cmdarry[14] = (void *)myread;
    cmdarry[15] = (void *)mywrite;
    cmdarry[16] = (void *)mylseek;
    cmdarry[17] = (void *)cat;
    cmdarry[18] = (void *)cp;
    cmdarry[19] = (void *)mv;
    cmdarry[20] = (void *)mount;
    cmdarry[21] = (void *)umount;
    cmdarry[22] = (void *)help;
    cmdarry[23] = (void *)quit;
}
//main
int main(int argc, char *argv[], char *env[])
{
    char inputtemp[128];
    int cmdid;
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
        printf("Command:%s  Pathname:%s  Parameter:%s\n", cmd, pathname, parameter);
        cmdid = cmdSearch();
        if (cmdid < 0)
        {
            printf("Incorrect command, enter another! Type \"menu\" for commands!\n");
        }
        else
        {
            (*cmdarry[cmdid])();
        }
    }
}
