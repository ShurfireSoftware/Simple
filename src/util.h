#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
#include <stdlib.h>

void OS_Delay(double sec);
int kbhit();
int getch();
void set_conio_terminal_mode(void);
void create_hex_string(char *p_buff, int buf_size, char *p_out);

#endif

