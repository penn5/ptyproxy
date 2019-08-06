#define _GNU_SOURCE 1
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <sysexits.h>
#ifdef __linux__
#include <pty.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#elif defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#include <sys/ioctl.h>
#endif

int do_proxy(int argc, char **argv)
{
    pid_t child;
    int master;
    int child_status;
    int exit_status = 0;
    struct winsize win;
    struct termios termios;
    struct termios orig_termios;

    if (argc < 1)
        return -1;

    /* Get current win size */
    ioctl(0, TIOCGWINSZ, &win);

    /* Ensure that terminal echo is switched off so that we
       do not get back from the spawned process the same
       messages that we have sent it. */
    if (tcgetattr(STDIN_FILENO, &termios) < 0) {
        perror("tcgetattr");
        return -1;
    }
    orig_termios = termios;

    child = forkpty(&master, NULL, &termios, &win);
    if (child == -1) {
        perror("forkpty");
        exit(1);
    }

    if (child == 0) {

        if (execvp(argv[0], argv)) {
            perror("execl");
            return -1;
        }

    } else {
        struct pollfd pfds[2];

        pfds[0].fd = master;
        pfds[0].events = POLLOUT;
        pfds[0].revents = 0;
        pfds[1].fd = STDIN_FILENO;
        pfds[1].events = POLLIN;
        pfds[1].revents = 0;

        fcntl(master, F_SETFL, O_NONBLOCK);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

        termios.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
        termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                             INPCK | ISTRIP | IXON | PARMRK);
        termios.c_oflag &= ~OPOST;
        termios.c_cc[VMIN] = 1;
        termios.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &termios) < 0) {
            perror("tcsetattr");
            exit_status = -1;
        }

        while (1) {
            int res;
            char buf[4096];
            int nb_fds;
            pid_t pid;

            pid = waitpid(child, &child_status, WNOHANG);
            if (pid == -1) {
                perror("waitpid");
                exit_status = -1;
                break;
            }
            if (pid == child && WIFEXITED(child_status)) {
                break;
            }

            nb_fds = poll(pfds, 2, -1);
            if (nb_fds < 1) {
                perror("poll");
                exit_status = -1;
                break;
            }

            /* master */
            if (pfds[0].revents & POLLOUT) {
                nb_fds--;
                do {
                    res = read(master, buf, 4096);
                    if (res > 0) {
                        write(STDOUT_FILENO, buf, res);
                    }
                } while (res == 4096);
                pfds[0].revents &= ~POLLOUT;
            }
            /* stdin */
            if (nb_fds && pfds[1].revents & POLLIN) {
                do {
                    res = read(STDIN_FILENO, buf, 4096);
                    if (res > 0) {
                        write(master, buf, res);
                    }
                } while (res == 4096);
                pfds[1].revents &= ~POLLIN;
            }
        }

        if (tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios) < 0) {
            perror("tcsetattr");
            exit_status = -1;
        }
        close(master);
    }
    return exit_status;
}

static void
usage(void)
{
    fprintf(stderr,
            "ptyproxy prog...\n"
            "prog is the software program to proxy");
    exit(EX_USAGE);
}

int main (int argc, char **argv)
{
    if (argc <= 1) {
        usage();
    }

    do_proxy(--argc, ++argv);

    return EX_OK;
}
