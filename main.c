#include "type.h"
MINODE minode[100];
MINODE *root;
PROC proc[NPROC], *running;
int fd, iblock, argscount;
int imap, bmap, ninodes, nblocks;
int blk = 0, offset = 0;
char buf[BLKSIZE];
char rbuf[BLKSIZE];
char *pathitems[32];
char *path, *pathname, *parameter;
char *inputdev;
char *cmd;
char *names[64][64];
char *cmdarrayinput[25] = {"mkdir","rmdir", "ls", "cd", "pwd", "creat", "link", "unlink",
                     "symlink", "stat", "chmod", "touch", "open", "close", "read",
                    "write", "lseek", "cat", "cp", "mv", "mount", "umount", "help", "quit", "pfd"};
void (*cmdarry[25])(void);
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
void mytruncate(MINODE *mip)
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
            bdealloc(mip->dev, mip->INODE.i_block[k]);
        }
    }
    mip->dirty = 1;
    mip->INODE.i_size = 0;
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
    ip->i_atime = time(0L);
    ip->i_ctime = time(0L);
    ip->i_mtime = time(0L);
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
                        mytruncate(mip); // changed this to use truncate
                        idealloc(dev, ino);
                        iput(mip);
                        pino = dp->inode;
                        pip = iget(dev, pino);
                        rmchild(pip, name);
                        pip->INODE.i_links_count--;
                        pip->INODE.i_atime = time(0L);
                        pip->INODE.i_mtime = time(0L);
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
    char buf[BLKSIZE];
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
            get_block(fd, mip->INODE.i_block[i], buf);
            dp = (DIR *) buf;
            char *cur = buf;
            while (cur < (buf + BLKSIZE))
            {
                char held = dp->name[dp->name_len];
                dp->name[dp->name_len] = 0;
                lsdetails(dev, dp->inode, dp->name);
                dp->name[dp->name_len] = held;
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
void lsdetails(int dev, int inonum, char *name)
{
    char *t1 = "xwrxwrxwr-------";
    char *t2 = "----------------";
    char tempname[64], timing[128];
    int i;
    MINODE *mip;
    mip = iget(dev, inonum);
    if ((mip->INODE.i_mode & 0xF000) == 0x8000)
        printf("%c",'-');
    if ((mip->INODE.i_mode & 0xF000) == 0x4000)
        printf("%c",'d');
    if ((mip->INODE.i_mode & 0xF000) == 0xA000)
        printf("%c",'l');
    for (i=8; i >= 0; i--)
    {
        if (mip->INODE.i_mode & (1 << i))
            printf("%c", t1[i]);
        else
            printf("%c", t2[i]);
    }
    printf("  %d  ", mip->INODE.i_links_count);
    printf("%d  ", mip->INODE.i_gid);
    printf("%d  ", mip->INODE.i_uid);
    strcpy(timing, ctime(&mip->INODE.i_mtime));
    timing[strlen(ctime(&mip->INODE.i_mtime)) - 1] = 0;
    printf("%s  ", timing);
    printf("%d  ", mip->INODE.i_size);
    printf("%s\n", name);
    iput(mip);
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
    ip->i_atime = time(0L);
    ip->i_ctime = time(0L);
    ip->i_mtime = time(0L);
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
    char *parent, *child;
    MINODE *mip, *pip;
    char *temppath = pathname;
    dev = running->cwd->dev;
    ino = getino(dev, pathname);
    mip = iget(dev, ino);
    if (S_ISREG(mip->INODE.i_mode) || S_ISLNK(mip->INODE.i_mode))
    {
        mip->INODE.i_links_count--;
        if (mip->INODE.i_links_count == 0 && S_ISREG(mip->INODE.i_mode))
        {
            mytruncate(mip);
        }
        child = basename(temppath);
        parent = dirname(temppath);
        pino = getino(dev, parent);
        pip = iget(dev, pino);
        rmchild(pip, child);
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
            mycreat(); // call creat to make a file with the second argument's name and path
            ino2 = getino(dev, pathname);
            mip2 = iget(dev, ino2);
            mip2->INODE.i_mode = 0xA000;
            cp = (char *)mip2->INODE.i_block;
            strcpy(cp, temp);
            mip2->dirty = 1;
            iput(mip2);
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
    int ino, dev;
    char timing[128];
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
    if (ino)
    {
        mip = iget(dev, ino);
        if (S_ISDIR(mip->INODE.i_mode))
        {
            printf("Type: Directory\n");
        }
        else if (S_ISREG(mip->INODE.i_mode))
        {
            printf("Type: File\n");
        }
        else if (S_ISLNK(mip->INODE.i_mode))
        {
            printf("Type: Link\n");
        }
        printf("Device: %d\n", mip->dev);
        printf("Inode #: %d\n", mip->ino);
        printf("Size: %d\n", mip->INODE.i_size);
        printf("User ID: %d\n", mip->INODE.i_uid);
        printf("Group ID: %d\n", mip->INODE.i_gid);
        printf("Links Count: %d\n", mip->INODE.i_links_count);
        printf("Blocks: %d\n", mip->INODE.i_blocks);
        strcpy(timing, ctime(&mip->INODE.i_mtime));
        timing[strlen(ctime(&mip->INODE.i_mtime)) - 1] = 0;
        printf("Modified Time: %s\n", timing);
        iput(mip);
    }
    else
    {
        printf("does not exist!\n");
    }
}
void mychmod()
{
    int ino, dev, parnum, k, orig;
    char *temp;
    MINODE *mip;
    if (strlen(parameter) <= 4)
    {
        if (parameter[0] != 48)
        {
            printf("Invalid mode entered!\n");
            return;
        }
        for (k = 0; k < 3; k++)
        {
            if (parameter[k] < 48 || parameter[k] > 55)
            {
                printf("Invalid mode entered!\n");
                return;
            }
        }
        if (pathname[0] == '/')
        {
            dev = root->dev;
        }
        else
        {
            dev = running->cwd->dev;
        }
        ino = getino(&dev, pathname);
        if (ino)
        {
            mip = iget(dev, ino);
            if (mip->INODE.i_uid == running->uid && running->uid == 0)
            {
                parnum = strtol(parameter, &temp, 8);
                orig = mip->INODE.i_mode &= 0b1111000000000000;
                orig |= parnum;
                mip->INODE.i_mode = orig;
                mip->dirty = 1;
                iput(mip);
            }
            else
            {
                printf("Incorrect permissions!\n");
            }
        }
        else
        {
            printf("Pathname does not exist!\n");
        }
    }
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
    mip->INODE.i_atime = time(0L);
    mip->INODE.i_mtime = time(0L);
    mip->dirty = 1;
    iput(mip);
}
//Level 2
int myopen()
{
    int mode, dev, ino, k;
    MINODE *mip;
    OFT *oftp = NULL;
    if (parameter)
    {
        if (strcmp("R", parameter) == 0) {
            mode = 0;
        }
        else if (strcmp("W", parameter) == 0) {
            mode = 1;
        }
        else if (strcmp("RW", parameter) == 0) {
            mode = 2;
        }
        else if (strcmp("APPEND", parameter) == 0) {
            mode = 3;
        }
        else {
            printf("Usage: R | W | RW | APPEND\nMake sure letters are uppercase!\n");
            return -1;
        }
    }
    else
    {
        printf("Usage: R | W | RW | APPEND\nMake sure letters are uppercase!\n");
        return -1;
    }
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
    if (S_ISREG(mip->INODE.i_mode))
    {
        for (k = 0; k < 10; k++)
        {
            if (running->fd[k] == 0)
            {
                break;
            }
            else if (running->fd[k]->inodeptr->ino == ino)
            {
                if (running->fd[k]->mode != 0 || mode != 0)
                {
                    printf("File already open with incompatible type\n");
                    return -1;
                }
                else
                {
                    oftp = running->fd[k];
                }
            }

        }
        if (oftp == NULL)
        {
            oftp = (OFT *)malloc(sizeof(OFT));
            oftp->refCount = 1;
        }
        else
        {
            oftp->refCount++;
        }
        oftp->mode = mode;
        oftp->inodeptr = mip;
        switch(mode)
        {
            case 0:
                oftp->offset = 0;
                break;
            case 1:
                mytruncate(mip);
                oftp->offset = 0;
                break;
            case 2:
                oftp->offset = 0;
                break;
            case 3:
                oftp->offset = mip->INODE.i_size;
                break;
        }
        running->fd[k] = oftp;
        if (mode == 0)
        {
            mip->INODE.i_atime = time(0L);
        }
        else
        {
            mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
        }
    }
    else
    {
        printf("Cannot open a non-file!\n");
        return -1;
    }
    return k;
}
void myclose()
{
    int tempfd;
    OFT *oftp;
    MINODE *mip;
    if (pathname[0] < '0' || pathname[0] > '9')
    {
        printf("Incorrect usage!\n");
        return;
    }
    tempfd = atoi(pathname);
    if (tempfd > 9 || tempfd < 0)
    {
        printf("Index is out of range!\n");
        return;
    }
    else
    {
        if (running->fd[tempfd])
        {
            oftp = running->fd[tempfd];
            running->fd[tempfd] = 0;
            oftp->refCount--;
            if (oftp->refCount > 0)
            {
                return;
            }
            mip = oftp->inodeptr;
            iput(mip);
            free(oftp);
        }
        else
        {
            printf("Index does not exist!\n");
        }
    }
}
void myread()
{
    int tempfd, tempnum, count;
    OFT *oftp;
    if (pathname == NULL || parameter == NULL)
    {
        printf("Incorrect usage!\n");
        return;
    }
    tempfd = atoi(pathname);
    tempnum = atoi(parameter);
    if (tempfd > 9 || tempfd < 0)
    {
        printf("Index is out of range!\n");
        return;
    }
    oftp = running->fd[tempfd];
    if (oftp->mode == 0 || oftp->mode == 2)
    {
        printf("--------------------\n");
        count = myreadhelp(tempfd, rbuf, tempnum);
        printf("\n--------------------\n");
        printf("myread: read %d char from file descriptor %d\n", count, tempfd);
        printf("--------------------\n");
    }
    else
    {
        printf("incorrect mode!\n");
        return;
    }
}
int myreadhelp(int fd, char *buf, int nbytes)
{
    OFT *oftp = running->fd[fd];
    int count = 0, lbk, startbyte, blk, len;
    int avil = oftp->inodeptr->INODE.i_size - oftp->offset;
    int ilbk;
    char readBuf[BLKSIZE];
    int ibuf[BLKSIZE/8], dibuf[BLKSIZE/8];
    char *cq = buf;
    while (nbytes > 0 && avil > 0)
    {
        lbk = oftp->offset / BLKSIZE;
        startbyte = oftp->offset % BLKSIZE;
        if (lbk < 12)
        {
           blk = oftp->inodeptr->INODE.i_block[lbk];
        }
        else if (lbk >= 12 && lbk < 268)
        {
            get_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[12], (char *)ibuf);
            blk = ibuf[lbk - 12];
        }
        else
        {
            get_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[13], (char *)ibuf);
            ilbk = (lbk - 268) / 256;
            get_block(oftp->inodeptr->dev, ibuf[ilbk], (char *)dibuf);
            blk = dibuf[(lbk - 268) % 256];
        }
        get_block(oftp->inodeptr->dev, blk, readBuf);
        char *cp = readBuf + startbyte;
        int remain = BLKSIZE - startbyte;
        if (nbytes <= remain)
        {
            len = nbytes;
        }
        else
        {
            len = remain;
        }
        if (len > avil)
        {
            len = avil;
        }
        memset(cq, '\0', BLKSIZE);
        strncpy(cq, cp, len);
        oftp->offset += len;
        count += len;
        avil -= len;
        nbytes -= len;
        remain -= len;
        printf("%s", cq);
    }
    return count;
}
void mywrite()
{
    int tempfd, count;
    char tbuf[128];
    OFT *oftp;
    if (pathname == NULL || parameter == NULL)
    {
        printf("Incorrect usage!\n");
        return;
    }
    tempfd = atoi(pathname);
    oftp = running->fd[tempfd];
    if (oftp->mode == 1 || oftp->mode == 2 || oftp->mode == 3)
    {
        strcpy(tbuf, parameter);
        count = mywritehelp(tempfd, tbuf, strlen(tbuf));
        printf("--------------------\n");
        printf("mywrite: wrote %d char to file descriptor %d\n", count, tempfd);
        printf("--------------------\n");
    }
    else
    {
        printf("incorrect mode!\n");
        return;
    }
}
int mywritehelp(int fd, char buf[], int nbytes)
{
    OFT *oftp = running->fd[fd];
    int count = 0, lbk, startbyte, blk, len;
    int ilbk;
    int ibuf[BLKSIZE/8], dibuf[BLKSIZE/8];
    char *cq = buf;
    while (nbytes > 0)
    {
        lbk = oftp->offset / BLKSIZE;
        startbyte = oftp->offset % BLKSIZE;
        if (lbk < 12)
        {
            if (oftp->inodeptr->INODE.i_block[lbk] == 0)
            {
                oftp->inodeptr->INODE.i_block[lbk] = balloc(oftp->inodeptr->dev);
            }
            blk = oftp->inodeptr->INODE.i_block[lbk];
        }
        else if (lbk >= 12 && lbk < 268)
        {
            if (oftp->inodeptr->INODE.i_block[12] == 0)
            {
                oftp->inodeptr->INODE.i_block[12] = balloc(oftp->inodeptr->dev);
                get_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[12], (char *) ibuf);
                memset(ibuf, 0, (BLKSIZE/8));
                put_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[12], (char *) ibuf);
            }
            else
            {
                get_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[12], (char *) ibuf);
            }
            if (ibuf[lbk - 12] == 0)
            {
                ibuf[lbk - 12] = balloc(oftp->inodeptr->dev);
            }
            blk = ibuf[lbk - 12];
        }
        else
        {
            if (oftp->inodeptr->INODE.i_block[13] == 0)
            {
                oftp->inodeptr->INODE.i_block[13] = balloc(oftp->inodeptr->dev);
                get_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[13], (char *) ibuf);
                memset(ibuf, 0, (BLKSIZE/8));
                put_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[13], (char *) ibuf);
            } else
            {
                get_block(oftp->inodeptr->dev, oftp->inodeptr->INODE.i_block[13], (char *) ibuf);
            }
            ilbk = (lbk - 268) / 256;
            if (ibuf[ilbk] == 0)
            {
                ibuf[ilbk] = balloc(oftp->inodeptr->dev);
                get_block(oftp->inodeptr->dev, ibuf[ilbk], (char *)dibuf);
                memset(dibuf, 0, (BLKSIZE/8));
                put_block(oftp->inodeptr->dev, ibuf[ilbk], (char *)dibuf);
            }
            else
            {
                get_block(oftp->inodeptr->dev, ibuf[ilbk], (char *) dibuf);
            }
            if (dibuf[(lbk - 268) % 256] == 0)
            {
                dibuf[(lbk - 268) % 256] = balloc(oftp->inodeptr->dev);
            }
            blk = dibuf[(lbk - 268) % 256];
        }
        char writeBuf[BLKSIZE];
        get_block(oftp->inodeptr->dev, blk, writeBuf);
        char *cp = writeBuf + startbyte;
        int remain = BLKSIZE - startbyte;
        if (nbytes < BLKSIZE)
        {
            len = nbytes;
        }
        else
        {
            len = BLKSIZE;
        }
        if (len > remain)
        {
            len = remain;
        }
        memset(cp, '\0', BLKSIZE);
        strncpy(cp, cq, len);
        oftp->offset += len;
        if (oftp->offset > oftp->inodeptr->INODE.i_size)
        {
            oftp->inodeptr->INODE.i_size = oftp->offset;
        }
        count += len;
        nbytes -= len;
        remain -= len;
        put_block(oftp->inodeptr->dev, blk, cp);
    }
    oftp->inodeptr->dirty = 1;
    return nbytes;
}
int mylseek()
{
    int tempfd, position;
    OFT *oftp;
    if (pathname == NULL && parameter == NULL)
    {
        printf("Incorrect usage!\n");
        return;
    }
    tempfd = atoi(pathname);
    position = atoi(parameter);
    oftp = running->fd[tempfd];
    if (oftp == NULL)
    {
        printf("Index not found!\n");
        return;
    }
    else
    {
        if (oftp->inodeptr->INODE.i_size < position || position < 0)
        {
            printf("Over run!\n");
        }
        else
        {
            oftp->offset = position;
        }
    }
}
void pfd()
{
    int k = 0;
    char *table[4] = {"R", "W", "RW", "APPD"};
    OFT *oftp;
    printf(" fd   mode  offset  INODE\n");
    printf("----  ----  ------  -----\n");
    while (running->fd[k])
    {
        oftp = running->fd[k];
        printf("  %d    %s     %d      %d, %d\n", k, table[oftp->mode], oftp->offset, oftp->inodeptr->dev, oftp->inodeptr->ino);
        k++;
    }
    printf("-------------------------\n");
}
void cat()
{
    char mybuf[BLKSIZE];
    int k, fd;
    char *table[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    parameter = "R";
    fd = myopen();
    if (fd == -1)
    {
        return;
    }
    myreadhelp(fd, mybuf, running->fd[fd]->inodeptr->INODE.i_size);
    pathname = table[fd];
    myclose();
    printf("\n");
}
void cp()
{
    int fd, gd, ino, k;
    char *dest = parameter;
    char tbuf[BLKSIZE];
    char *table[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    parameter = "R";
    fd = myopen();
    pathname = dest;
    parameter = "W";
    ino = getino(running->cwd->dev, pathname);
    if (!ino)
    {
        mycreat();
    }
    gd = myopen();
    while(k = myreadhelp(fd, tbuf, BLKSIZE))
    {
        mywritehelp(gd, tbuf, k);
        memset(tbuf, '\0', BLKSIZE);
    }
    pathname = table[fd];
    myclose();
    pathname = table[gd];
    myclose();
}
void mv()
{
    int ino;
    ino = getino(running->cwd->dev, pathname);
    if (ino != 0)
    {
        mylink();
        myunlink();
    }
    else
    {
        printf("%s not found\n", pathname);
    }
}
void help()
{
    printf("|-----------Commands-----------|\n");
    printf("|------------------------------|\n");
    printf("|   mkdir - rmdir - ls - cd    |\n");
    printf("| pwd - creat - link - unlink  |\n");
    printf("|symlink - stat - chmod - touch|\n");
    printf("| open - close - read - write  |\n");
    printf("| lseek - cat - cp - mv - pfd  |\n");
    printf("|------------------------------|\n");
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
    get_block(dev, imap, buf);
    clr_bit(buf, ino);
    incFreeInodes(dev);
    put_block(dev, imap, buf);
}
int bdealloc(int dev, int bno) // deallocate bno
{
    char buf[BLKSIZE];
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
    for (k = 0; k < 25; k++)
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
    for (k = 0; k < 10; k++)
    {
        proc[0].fd[k] = NULL;
        proc[1].fd[k] = NULL;
    }

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
    cmdarry[24] = (void *)pfd;
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
        printf("~HM: ");
        fgets(inputtemp, 128, stdin);
        inputtemp[strlen(inputtemp)-1] = 0;
        cmd = strtok(inputtemp, " ");
        pathname = strtok(NULL, " ");
        parameter = strtok(NULL, "");
        if (cmd != NULL)
        {
            printf("Command:%s  Pathname:%s  Parameter:%s\n", cmd, pathname, parameter);
            cmdid = cmdSearch();
            if (cmdid < 0) {
                printf("Incorrect command, enter another! Type \"menu\" for commands!\n");
            }
            else {
                (*cmdarry[cmdid])();
            }
        }
    }
}
