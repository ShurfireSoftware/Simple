#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void OS_Delay(double sec)
{
    clock_t end = clock() + (sec*CLOCKS_PER_SEC);
    while(clock()<end);
}

struct termios orig_termios;

static void reset_terminal_mode()
{
    tcsetattr(0,TCSANOW, &orig_termios);
}

void set_conio_termial_mode(void)
{
    struct termios new_termios;
    tcgetattr(0,&orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0,TCSANOW, &new_termios);
}

int getch(void)
{
    int r;
    unsigned char c;
    if ((r == read(0,&c,sizeof(c))) < 0) {
        return r;
    }
    else {
        return c;
    }
}

int kbhit(void)
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0,&fds);
    return select(1, &fds, NULL, NULL, &tv);
}

void create_hex_string(char *p_buff, int buf_size, char *p_out)
{
    int count;
    char *p_str;
    p_str = p_out;
    for (count = 0;;) {
        sprintf(p_str,"%02x",p_buff[count]);
        ++count;
        if (count < buf_size) {
            strcat(p_str," ");
            p_str += 3;
        }
        else {
            break;
        }
    }
}


