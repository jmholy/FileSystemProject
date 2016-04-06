#include "type.h"
MINODE minode[100];
MINODE *root;
PROC proc[NPROC], *running;

//Level 1
void init()
{
    int k = 0;
    proc[0].uid = 0;
    proc[0].pid = 1;
    proc[0].cwd = 0
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