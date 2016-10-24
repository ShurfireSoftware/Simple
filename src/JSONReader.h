#ifndef JSONREADER_H__
#define JSONREADER_H__

#include <stdbool.h>
#include <stdint.h>

bool findJSONString(char *, char *, char *);
bool findJSONuint16(char *, char *, uint16_t *);
bool findJSONuint8(char *, char *, uint8_t *);
bool findJSONbool(char *, char *, bool *);
bool findJSONuint32(char *, char *, uint32_t *);
bool findJSONuint64(char *, char *, uint64_t *);
bool findJSONint32(char *, char *, int32_t *);

#endif
