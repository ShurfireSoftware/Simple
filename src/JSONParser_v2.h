#ifndef JSONPARSER_v2_H__
#define JSONPARSER_v2_H__
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
//#include <mqx.h>

typedef struct {
    uint16_t characterCount;
    uint16_t characterPointer;
    char *   parseBuffer;
} JSON_PARSE_OBJECT;

void jv2_makeObjectFromString(JSON_PARSE_OBJECT * parseObject, char *source);
bool jv2_findObject(JSON_PARSE_OBJECT * root,char *objectName,JSON_PARSE_OBJECT * object);
void jv2_showObject(JSON_PARSE_OBJECT * parseObject);
bool jv2_isObjectNull(JSON_PARSE_OBJECT * parseObject);
bool jv2_getObjectString(JSON_PARSE_OBJECT * parseObject, char * dest);
bool jv2_getObjectBool(JSON_PARSE_OBJECT * parseObject, bool * objectValue);
bool jv2_getObjectInteger(JSON_PARSE_OBJECT * parseObject, uint16_t * objectValue);
bool jv2_getObjectInt32(JSON_PARSE_OBJECT * parseObject, int32_t * objectValue);
bool jv2_getObjectUint32(JSON_PARSE_OBJECT * parseObject, uint32_t * objectValue);
bool jv2_findObjectString(JSON_PARSE_OBJECT * root,char *objectName,char * objectValue);
bool jv2_findObjectInteger(JSON_PARSE_OBJECT * root,char *objectName,uint16_t * objectValue);
bool jv2_getObjectFloat(JSON_PARSE_OBJECT * parseObject, float * objectValue);
bool jv2_getObjectUTC(JSON_PARSE_OBJECT * parseObject, struct tm *objectValue);
void jv2_percentEncodeURIData( char *p_in, char * p_out);

#endif
