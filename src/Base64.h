#ifndef BASE64_H__
#define BASE64_H__

#include <stdint.h>
#include <stdbool.h>

void ConvertToBase64(char *p_input, uint16_t len, char *p_result);
void processTriplet(unsigned char *, uint8_t, char *);
uint8_t processQuartet(char *, unsigned char *);

#endif
