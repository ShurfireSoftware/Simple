#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "JSONParser_v2.h"


#ifdef USE_ME
#include <mqx.h>
#include <shell.h>
#include <ctype.h>
#include "JSONParser_v2.h"
#include <stdlib.h>
#include "config.h"
#endif


//-----------------------------------------------------------------------------

void jv2_makeObjectFromString(JSON_PARSE_OBJECT * parseObject, char *source)
{
    parseObject->characterPointer = 0;
    parseObject->characterCount = strlen(source);
    parseObject->parseBuffer = source;
}
//-----------------------------------------------------------------------------

void jv2_showObject(JSON_PARSE_OBJECT * parseObject)
{
	printf("%s", parseObject->parseBuffer);
}


//-----------------------------------------------------------------------------

bool jv2_getObjectString(JSON_PARSE_OBJECT * parseObject, char * dest)
{
    uint32_t i;
    
    if ('"'==parseObject->parseBuffer[0]) {
        for (i=1;((i<parseObject->characterCount)&&('"'!=parseObject->parseBuffer[i]));i++) {
            dest[i-1] = parseObject->parseBuffer[i];
        }
        dest[i-1] = 0;
        return true;
    } else {
        return false;
    }
}

//-----------------------------------------------------------------------------

bool jv2_isObjectNull(JSON_PARSE_OBJECT * parseObject)
{
    if (strstr(parseObject->parseBuffer, "null") == parseObject->parseBuffer)
        return true;
    else
        return false;
}
//-----------------------------------------------------------------------------

bool jv2_getObjectBool(JSON_PARSE_OBJECT * parseObject, bool * boolPtr)
{
    bool found;
    if (strstr(parseObject->parseBuffer, "true") == parseObject->parseBuffer) {
        *boolPtr = true;
        found = true;
    }
    else if (strstr(parseObject->parseBuffer, "false") == parseObject->parseBuffer) {
        *boolPtr = false;
        found = true;
    }
    else {
        found = false;
    }
    return found;
}
//-----------------------------------------------------------------------------

bool jv2_getObjectInteger(JSON_PARSE_OBJECT * parseObject, uint16_t * objectValue)
{
    uint16_t thisValue=0;
    bool isNumber=parseObject->characterCount!=0;
    uint32_t    i;
    
    for (i=0;(i<parseObject->characterCount) && isNumber;i++) {
        if (isdigit(parseObject->parseBuffer[i])) {
            thisValue=thisValue*10+parseObject->parseBuffer[i]-'0';
        } else {
            isNumber = false;
         }
    }   
    
    if (isNumber) {
        *objectValue=thisValue;
    }

    return isNumber;
}
bool jv2_getObjectUint32(JSON_PARSE_OBJECT * parseObject, uint32_t * objectValue)
{
    uint32_t thisValue=0;
    bool isNumber=parseObject->characterCount!=0;
    uint32_t    i;
    
    for (i=0;(i<parseObject->characterCount) && isNumber;i++) {
        if (isdigit(parseObject->parseBuffer[i])) {
            thisValue=thisValue*10+parseObject->parseBuffer[i]-'0';
        } else {
            isNumber = false;
         }
    }   
    
    if (isNumber) {
        *objectValue=thisValue;
    }

    return isNumber;
}

bool jv2_getObjectInt32(JSON_PARSE_OBJECT * parseObject, int32_t * objectValue)
{
    int32_t thisValue=0;
    bool isNumber=parseObject->characterCount!=0;
    uint32_t    i;
    char *p_val;
    bool is_neg = false;
    uint32_t len;
    len = parseObject->characterCount;
    p_val = parseObject->parseBuffer;
    if (*p_val == '-') {
        ++p_val;
        is_neg = true;
        --len;
    }
    for (i=0;(i<len) && isNumber;i++) {
        if (isdigit(p_val[i])) {
            thisValue=thisValue*10+p_val[i]-'0';
        } else {
            isNumber = false;
         }
    }   
    
    if (isNumber) {
        if (is_neg == true) {
            thisValue = -thisValue;
        }
        *objectValue=thisValue;
    }

    return isNumber;
}

bool jv2_getObjectFloat(JSON_PARSE_OBJECT * parseObject, float * objectValue)
{
    *objectValue = 0;
    bool isNumber=parseObject->characterCount!=0;
    
    if (isNumber == true) {
        *objectValue=atof(parseObject->parseBuffer);
    }

    return isNumber;
}

bool jv2_getObjectUTC(JSON_PARSE_OBJECT * parseObject, struct tm *p_date)
{
    bool isNumber=false;
    char *p_str;
    char eval[5];
    p_str = parseObject->parseBuffer;

//"2015-05-12 02:04:00.000"
    if (*p_str == '"') {
        ++p_str;
    }
    if ( (p_str[4] == '-') && (p_str[7] == '-') && ((p_str[10] == ' ') ||(p_str[10] == 'T'))
        && (p_str[13] == ':') && (p_str[16] == ':') && (p_str[19] == '.')) {
        memcpy(eval,&p_str[0],4);
        eval[4] = 0;
        p_date->tm_year = atol(eval)-1900;
        memcpy(eval,&p_str[5],2);
        eval[2] = 0;
        p_date->tm_mon = atol(eval) - 1;
        memcpy(eval,&p_str[8],2);
        p_date->tm_mday = atol(eval);
        memcpy(eval,&p_str[11],2);
        p_date->tm_hour = atol(eval);
        memcpy(eval,&p_str[14],2);
        p_date->tm_min = atol(eval);
        memcpy(eval,&p_str[17],2);
        p_date->tm_sec = atol(eval);
        isNumber = true;
    }
    return isNumber;
}

void jv2_objectSkipWhitespace(JSON_PARSE_OBJECT * object)
{
    if (NULL == object->parseBuffer) {
          return;
      }

      while (object->characterCount && isspace(*object->parseBuffer)) {
          object->parseBuffer++;
          object->characterCount--;
      }
}

void jv2_percentEncodeURIData( char *p_in, char * p_out)
{
    int len = strlen(p_in);
    int i;
    for (i=0; i<len; ++i, ++p_in) {
        switch (*p_in) {
            case ':':
            case '/':
            case '?':
            case '#':
            case '[':
            case ']':
            case '@':
            case '!':
            case '$':
            case '&':
            case '\'':
            case '(':
            case ')':
            case '*':
            case '+':
            case ',':
            case ';':
            case '=':
            case '%':
                *p_out++ = '%';
                sprintf(p_out,"%02X",*p_in);
                p_out += 2;
                break;
            case '\\':
                break;
            default:
                *p_out = *p_in;
                ++p_out;
                break;
        }
    }
    *p_out = 0;
}

void jv2_objectTrim(JSON_PARSE_OBJECT * object) 
{
    uint16_t i, curlyCount=0;
    bool inString = false;
    
    jv2_objectSkipWhitespace(object);
    if (':' == *object->parseBuffer){
        object->parseBuffer++;
        object->characterCount--;
        jv2_objectSkipWhitespace(object);
    }  
    
    for (i=0;i<object->characterCount;i++) {
        if (inString) {
            if ('"' == object->parseBuffer[i]) {
                inString=false;
            }
        } else if (curlyCount) {
            if ('{' == object->parseBuffer[i]) {
                curlyCount++;
            } else if ('}' == object->parseBuffer[i]) {
                curlyCount--;
            }
        } else {
            if ('"' == object->parseBuffer[i]) {
                inString=true;
            } else if ('{' == object->parseBuffer[i]) {
                curlyCount++;
            } else if (('}' == object->parseBuffer[i])|| (',' == object->parseBuffer[i])) {
                object->characterCount = i;
                break;
            }
        }
    }
    
    while (object->characterCount && isspace(object->parseBuffer[object->characterCount-1])) {
        object->characterCount--;
    }

}

typedef enum {
    json_start,
    json_scan,
    json_string_start,
    json_in_string,
    json_in_curlies,
    json_end,

} json_parse_state ;
//-----------------------------------------------------------------------------

bool jv2_findObject(JSON_PARSE_OBJECT * root,char *objectName,JSON_PARSE_OBJECT * object)
{
    bool     found = false;
    uint16_t stringLength,i, curlyCount=0;
    bool inString = false;
    json_parse_state state=json_start;
    
    // determine the length of thisString
    stringLength = strlen(objectName);
    
    memset(object,0,sizeof(JSON_PARSE_OBJECT));

    if (stringLength <= root->characterCount) {
        // start at the start position (0) on the input buffer
        for (i=0;i<(root->characterCount-stringLength) && (!found) &&(state!=json_end); i++) {
            switch (state) {
            case json_start:
                if ('"' == root->parseBuffer[i]) {
                    state = json_string_start;
                } else  if ('{' == root->parseBuffer[i]) { 
                    state = json_scan;
                } else if ('}' == root->parseBuffer[i]) {  
                    state = json_end;
                }
                break;
            case json_scan:
                if ('"' == root->parseBuffer[i]) {
                    state = json_string_start;
                } else  if ('{' == root->parseBuffer[i]) { 
                    curlyCount++;
                    state = json_in_curlies;
                } else if ('}' == root->parseBuffer[i]) {  
                    state = json_end;
                }
                break;
            case json_string_start:
                if ('"' == root->parseBuffer[i]) {
                    state = json_scan;
                } else if (strncasecmp(&root->parseBuffer[i], objectName,stringLength )==0) {
                    if ('"'==root->parseBuffer[i+stringLength]) {
                        object->parseBuffer = &root->parseBuffer[i+stringLength+1];
                        object->characterCount = root->characterCount -(i+stringLength+1);
                        jv2_objectTrim(object);
                 
                        found = true;
                        state = json_end;
                    } 
                } else { 
                    state = json_in_string;
                }
                break;
            case json_in_string:
                if ('"' == root->parseBuffer[i]) {
                    state = json_scan;
                }
                break;
            case json_in_curlies:
                if (inString) {
                    if ('"' == root->parseBuffer[i]) {
                        inString=false;
                    }
                } else {
                    if ('"' == root->parseBuffer[i]) {
                        inString=true;
                    } else if ('{' == root->parseBuffer[i]) {
                        curlyCount++;
                    } else if ('}' == root->parseBuffer[i]) {
                        curlyCount--;
                        if (0==curlyCount) {
                            state = json_scan;
                        }
                    }
                } 
                break;

            
            }
        }
    }
    // report
    return found;
}

bool jv2_findObjectString(JSON_PARSE_OBJECT * root,char *objectName,char * objectValue)
{
    JSON_PARSE_OBJECT node;
    
    if (jv2_findObject(root,objectName,&node)) {
        if (jv2_getObjectString(&node, objectValue)) {
            return true;
        }
    }
    return false;
}
bool jv2_indObjectInteger(JSON_PARSE_OBJECT * root,char *objectName,uint16_t * objectValue)
{
    JSON_PARSE_OBJECT node;
    
    if (jv2_findObject(root,objectName,&node)) {
        if (jv2_getObjectInteger(&node, objectValue)) {
            return true;
        }
    }
    return false;
}
