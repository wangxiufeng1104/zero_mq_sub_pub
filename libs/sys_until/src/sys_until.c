#include <sys/stat.h>
#include <fcntl.h>
#include "sys_until.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
/* Returns 0 on success, -1 on error */
int becomeDaemon(int flags)
{
    int maxfd, fd;

    switch (fork())
    { /* Become background process */
    case -1:
        return -1;
    case 0:
        break; /* Child falls through... */
    default:
        _exit(EXIT_SUCCESS); /* while parent terminates */
    }

    if (setsid() == -1) /* Become leader of new session */
        return -1;

    switch (fork())
    { /* Ensure we are not session leader */
    case -1:
        return -1;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (!(flags & BD_NO_UMASK0))
        umask(0); /* Clear file mode creation mask */

    if (!(flags & BD_NO_CHDIR))
        chdir("/"); /* Change to root directory */

    if (!(flags & BD_NO_CLOSE_FILES))
    { /* Close all open files */
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)          /* Limit is indeterminate... */
            maxfd = BD_MAX_CLOSE; /* so take a guess */

        for (fd = 0; fd < maxfd; fd++)
            close(fd);
    }

    if (!(flags & BD_NO_REOPEN_STD_FDS))
    {
        close(STDIN_FILENO); /* Reopen standard fd's to /dev/null */

        fd = open("/dev/null", O_RDWR);

        if (fd != STDIN_FILENO) /* 'fd' should be 0 */
            return -1;
        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -1;
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -1;
    }

    return 0;
}
void becomeSingle(const char *process_name)
{
    char buf[32] = {0};
    sprintf(buf, "/var/run/%s.pid", process_name);
    int pid_fd = open(buf, O_WRONLY | O_CREAT, 0666);
    if (pid_fd < 0)
    {
        perror("Fail to open");
        exit(1);
    }
    struct flock lock;
    bzero(&lock, sizeof(lock));

    if (fcntl(pid_fd, F_GETLK, &lock) < 0)
    {
        perror("Fail to fcntl F_GETLK");
        exit(1);
    }
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;

    if (fcntl(pid_fd, F_SETLK, &lock) < 0)
    {
        perror("Fail to fcntl F_SETLK");
        exit(1);
    }

    pid_t pid = getpid();
    bzero(buf, sizeof(buf));
    int len = snprintf(buf, 32, "%d\n", (int)pid);
    write(pid_fd, buf, len);
}