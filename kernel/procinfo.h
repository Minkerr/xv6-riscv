#ifndef _PROCINFO_H
#define _PROCINFO_H

struct procinfo {
    int pid;
    char name[16];
    int state;
    int ppid;
};

#endif