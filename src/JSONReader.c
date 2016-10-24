#include <string.h>
#include <stdio.h>
#include "JSONReader.h"

// #define ItsDisplaySearchResults_
#define ItsMaxKeyLength_ 50
#define ItsMaxLevel_ 5

static char keyLevel[ItsMaxLevel_][ItsMaxKeyLength_], requestKeyLevel[ItsMaxLevel_][ItsMaxKeyLength_],
    *jsonBodyBuf;
static uint16_t jsonBodyPtr, jsonRecordStartPtr, jsonBodyLength;
static uint8_t level, requestLevel, requestArrayIndex[ItsMaxLevel_], arrayIndex[ItsMaxLevel_];
static bool keysFound, isRequestArray[ItsMaxLevel_], isArray[ItsMaxLevel_], arraySpecified;

static bool isPrintable(unsigned char character)
{
    return (character >= 32) && (character <= 126);
}

static bool isNumber(unsigned char character)
{
    return (character >= 48) && (character <= 57);
}

static bool findKey(char *thisKey)
{
    bool found = false, begin = false, end = false;
    uint16_t keyPtr = 0, jsonRecordIndex = 0;
    char thisChar;
    
    // the key would be the first string in the line "jsonDataRecord"
    // and should come prior to a ':' colon
    // so look for: 1) '"'
    //              2) one or more printable characters
    //              3) another '"'
    //              4) ':'
    
    thisKey[0] = 0;
        
    while(!found && (jsonRecordStartPtr + jsonRecordIndex < jsonBodyLength)) {
        thisChar = jsonBodyBuf[jsonRecordStartPtr + jsonRecordIndex++];
        
        // find begin or end of key
        if(thisChar == '"') {
            if(begin) {
                
                // found end of key
                end = true;
                thisKey[keyPtr] = 0;
            }
            else begin = true;		// found begin
        }
        else {
            if(begin) {
                if(end) found = (thisChar == ':');
                else thisKey[keyPtr++] = thisChar;		// must be a character in key
            }
        }
    }
    
    // for debugging
    // if(found) printf("key \"%s\" found for level = %d\n", thisKey, level);
    
    return found;
}

static void processParseRecord(uint8_t sender)
{
    char key[50];
    uint8_t n;
    
    #ifdef ItsDebugProcessParseRecord_
        bool newLine;
    #endif

    // for debugging
    // printf("process parse record (from sender: %d)\n", sender);
    // printf("JSON: body ptr = %d, record start ptr = %d\n", jsonBodyPtr, jsonRecordStartPtr);
    
    if(jsonBodyPtr - jsonRecordStartPtr > 4) {
    
        // update key level array
        if(findKey(key)) {
        
            // marker 07/05/2015 - something is wrong here
            strcpy(keyLevel[level - 1], key);
            for(n = level; n < ItsMaxLevel_; n++) keyLevel[n][0] = 0;
            
            // for debugging
            /*
            printf("accumulated key: ");
            for(n = 0; n < level; n++) printf("%s\\", keyLevel[n]);
            printf("\n");
            */
        }
    
        if(level == requestLevel) {
        
            #ifdef ItsDebugProcessParseRecord_
                printf("levels Ok\n");
                newLine = false;
            #endif
        
            // check if this record has the requested keys
            keysFound = true;
            for(n = 0; n < level; n++) {
            
                if(strcmp(keyLevel[n], requestKeyLevel[n]) != 0) keysFound = false;
            
                #ifdef ItsDebugProcessParseRecord_
                    printf("n = %d, ", n);
                    printf("keyLevel = %s, ", keyLevel[n]);
                    printf("requestKeyLevel = %s, ", requestKeyLevel[n]);
                    displayThisBoolean("keysFound", keysFound);
                    printf("\n");
                #endif
            
                // if we are checking an array, we have to see if the index is right
                if(keysFound && arraySpecified && isRequestArray[n]) {
                
                    if(!isArray[n] || (arrayIndex[n] != requestArrayIndex[n]))
                        keysFound = false;
                
                    #ifdef ItsDebugProcessParseRecord_
                        displayThisBoolean("    isArray", isArray[n]);
                        printf("arrayIndex = %d, ", arrayIndex[n]);
                        printf("requestArrayIndex = %d, ", requestArrayIndex[n]);
                        displayThisBoolean("keysFound", keysFound);
                        printf("\n");
                        newLine = true;
                    #endif
                }
            }
        
            #ifdef ItsDebugProcessParseRecord_
                if(newLine) printf("\n");
            #endif
        }
        // else printf("record too short ...\n");
    }
    
    if(!keysFound) {
        jsonRecordStartPtr = jsonBodyPtr;
    
        // for debugging
        // printf(" -- new JSON record start pointer = %d -- ", jsonRecordStartPtr);
    }
}

static void scanJSONBody(char *source)
{
    uint8_t m, dimLevel = ItsMaxLevel_ - 1;
    char thisChar;
    
    // breaks the JSON body up into separate records
    // and keeps up with levels
    jsonRecordStartPtr = 0;
    level = 0;
    keysFound = false;
    for(m = 0; m < ItsMaxLevel_; m++) isArray[m] = false;

    jsonBodyBuf = source;
    jsonBodyLength = strlen(jsonBodyBuf);
    
    jsonBodyPtr = 0;
    
    // for debugging
    // printf(" \"(1)");
    
    do {
        thisChar = jsonBodyBuf[jsonBodyPtr++];
        
        // for debugging
        // printf("%c", thisChar);
        
        if(isPrintable(thisChar)) switch(thisChar) {
            
            // check for JSON "control characters"
            case '[':
                isArray[dimLevel] = true;
                arrayIndex[dimLevel] = 0;
                break;
            case '{':
                processParseRecord(1);
                level++;
                dimLevel = level - 1;
                break;
            case ',':
                processParseRecord(2);
                if(isArray[dimLevel]) {
                    
                    arrayIndex[dimLevel]++;
                    
                    // for debugging
                    // printf("incremented arrayIndex(%d) is now: %d\n", dimLevel, arrayIndex[dimLevel]);
                }
                break;
            case '}':
                processParseRecord(3);
                level--;
                if(level) dimLevel = level - 1;
                else level = ItsMaxLevel_ - 1;
                break;
            case ']':
                isArray[dimLevel] = false;
                break;
        }
    } while(!keysFound && level && thisChar);
    
    // for debugging
    // printf("\" ");
}

static void scanForKeys(char *jsonData, char *jsonKeys)
{
    char thisChar;
    uint8_t levels = 0, n, jsonKeysLength = strlen(jsonKeys), keyPtr = 0,
        thisArrayIndex;
    bool readingArrayIndex = false;
    
    // start with a clean slate
    for(n = 0; n < ItsMaxLevel_; n++) isRequestArray[n] = false;
    arraySpecified = false;

    // extract request key levels
    requestLevel = 1;
    for(n = 0; n < jsonKeysLength; n++) {
        thisChar = jsonKeys[n];
        if(readingArrayIndex) {
            if(thisChar == ']') {
                readingArrayIndex = false;
                isRequestArray[levels] = true;
                requestArrayIndex[levels] = thisArrayIndex;
            }
            else {
                if(isNumber(thisChar)) {
                    thisArrayIndex *= 10;
                    thisArrayIndex += thisChar - '0';
                }
            }
        }
        else {
            if(thisChar == '\\') {
                requestKeyLevel[levels][keyPtr] = 0;
                levels++;
                keyPtr = 0;
                requestLevel++;
            }
            else if(thisChar == '[') {
                readingArrayIndex = true;
                thisArrayIndex = 0;
                arraySpecified = true;
            }
            else requestKeyLevel[levels][keyPtr++] = thisChar;
        }
    }
    requestKeyLevel[levels][keyPtr] = 0;
    
    // now we are ready to go
    scanJSONBody(jsonData);
}

bool findJSONString(char *jsonData, char *jsonKeys, char *foundString)
{
    char thisChar;
    uint8_t n;
    uint16_t jsonRecordIndex = 0;
    bool colon = false, begin = false, end = false, null_found = false;
    char *p_null;

    #ifdef ItsDisplaySearchResults_
        printf("find string with keys \"%s\": ", jsonKeys);
    #endif

    // first find the proper JSON record
    scanForKeys(jsonData, jsonKeys);
    
    // then get the string in between :" and " ...
    if(keysFound) {
        n = 0;
        
        while(!end && !null_found && (jsonRecordStartPtr + jsonRecordIndex < jsonBodyLength)) {
            thisChar = jsonBodyBuf[jsonRecordStartPtr + jsonRecordIndex++];
            
            switch(thisChar) {
                case ':':

                    p_null = &jsonBodyBuf[jsonRecordStartPtr + jsonRecordIndex];
                    if (!memcmp(p_null,"null",4)) {
                        null_found = true;
                        break;
                    }

                    // the string must be found after the colon
                    if (!colon)
                        colon = true;
                    else {
                        foundString[n++] = thisChar;
                    }
                    break;
                case '"':
                    if(colon) {
                        foundString[n] = 0;
                        if(begin) end = true;
                        
                        // begin can be set after the colon is found
                        else begin = true;
                    }
                    break;
                default:
                    
                    // string can be found after colon and begin, but prior to end
                    if(colon && begin && !end) foundString[n++] = thisChar;
            }
        }
        if(!end) foundString = NULL;
    }

    // display
    #ifdef ItsDisplaySearchResults_
        if(end) printf("\"%s\"\n", foundString);
        else printf("NOT found\n");
    #endif
    
    return end;
}

bool findJSONint32(char *jsonData, char *jsonKeys, int32_t *foundIntegerPtr)
{
    char thisChar;
    uint8_t jsonKeysLength = strlen(jsonKeys);
    uint16_t jsonRecordIndex = 0;
    int32_t foundInteger = 0;
    bool colon = false, error = false, begin = false, end = false, negativ = false;

    #ifdef ItsDisplaySearchResults_
        printf("find integer (32-bit) with keys \"%s\": ", jsonKeys);
    #endif
    
    // first find the proper JSON record
    scanForKeys(jsonData, jsonKeys);
    
    // then get the integer after :
    if(keysFound) {
        
        while(!error && !end && (jsonRecordStartPtr + jsonRecordIndex < jsonBodyLength)) {
            thisChar = jsonBodyBuf[jsonRecordStartPtr + jsonRecordIndex++];
            
            if(thisChar == ':') {
                
                // only one : allowed
                if(colon) error = true;
                else colon = true;
            }
            else {
                
                // needs to be after :
                if(colon) {
                    if(thisChar == '-') {
                        if(negativ) error = true;
                        else negativ = true;
                    }
                    
                    else if(isNumber(thisChar)) {
                        if(begin) {
                            if(!end) {
                                
                                // process integer
                                foundInteger *= 10;
                                foundInteger += thisChar - 48;
                            }
                        }
                        if(!begin) {
                            
                            // start integer
                            begin = true;
                            foundInteger = thisChar - 48;
                        }
                    }
                    else if (colon && !begin && isspace(thisChar)) { } // ignore leading whitespace
                    // non_numerals mark that we are past the end of the integer
                    else if(begin) end = true;
                }
            }
        }
    }

    // display
    if(begin && !error) {
        if(negativ) foundInteger = -foundInteger;
        *foundIntegerPtr = foundInteger;
        
        #ifdef ItsDisplaySearchResults_
            printf("%d\n", foundInteger);
        #endif
    }
    #ifdef ItsDisplaySearchResults_
        else printf("NOT found\n");
    #endif
    
    return begin && !error;
}
//-----------------------------------------------------------------------------

bool findJSONuint64(char *jsonData, char *jsonKeys, uint64_t *foundIntegerPtr)
{
    char thisChar;
    uint16_t jsonRecordIndex = 0;
    uint64_t foundInteger = 0;
    bool colon = false, error = false, begin = false, end = false;

    #ifdef ItsDisplaySearchResults_
        printf("find integer with keys \"%s\": ", jsonKeys);
    #endif
    
    // first find the proper JSON record
    scanForKeys(jsonData, jsonKeys);
    
    // then get the integer after :
    if(keysFound) {
        
        // for debugging
        // printf(" \"(2)");
        
        while(!error && !end && (jsonRecordStartPtr + jsonRecordIndex < jsonBodyLength)) {
            thisChar = jsonBodyBuf[jsonRecordStartPtr + jsonRecordIndex++];
            
            // for debugging
            // printf("%c", thisChar);
            
            if(thisChar == ':') {
                
                // only one : allowed
                if(colon) error = true;
                else colon = true;
            }
            else {
                
                // needs to be after :
                if(colon) {
                    if(isNumber(thisChar)) {
                        if(begin) {
                            if(!end) {
                                
                                // process integer
                                foundInteger *= 10;
                                foundInteger += thisChar - 48;
                            }
                        }
                        if(!begin) {
                            
                            // start integer
                            begin = true;
                            foundInteger = thisChar - 48;
                        }
                    }
                    
                    // non_numerals mark that we are past the end of the integer
                    else if(begin) end = true;
                }
            }
        }
        
        // for debugging
        // printf("\"\n");
    }

    // display
    if(begin && !error) {
        *foundIntegerPtr = foundInteger;
        
        #ifdef ItsDisplaySearchResults_
            printf("%d\n", foundInteger);
        #endif
    }
    #ifdef ItsDisplaySearchResults_
        else printf("NOT found\n");
    #endif
    
    return begin && !error;
}
//-----------------------------------------------------------------------------

bool findJSONuint32(char *jsonData, char *jsonKeys, uint32_t *foundIntegerPtr)
{
    uint64_t tempVal;
    bool found = findJSONuint64(jsonData, jsonKeys, &tempVal);
    if(found) *foundIntegerPtr = tempVal;
    return found;
}
//-----------------------------------------------------------------------------

bool findJSONuint16(char *jsonData, char *jsonKeys, uint16_t *foundIntegerPtr)
{
    uint64_t tempVal;
    bool found = findJSONuint64(jsonData, jsonKeys, &tempVal);
    if(found) *foundIntegerPtr = (uint16_t)tempVal;
    return found;
}

bool findJSONuint8(char *jsonData, char *jsonKeys, uint8_t *foundIntegerPtr)
{
    uint64_t tempVal;
    bool found = findJSONuint64(jsonData, jsonKeys, &tempVal);
    if(found) *foundIntegerPtr = (uint8_t)tempVal;
    return found;
}

bool findJSONbool(char *jsonData, char *jsonKeys, bool *p_bool_rtn)
{
    char thisChar;
    uint16_t jsonRecordIndex = 0;
    bool end = false, bool_found = false;
    char *p_bool;

    #ifdef ItsDisplaySearchResults_
        printf("find string with keys \"%s\": ", jsonKeys);
    #endif

    // first find the proper JSON record
    scanForKeys(jsonData, jsonKeys);
    
    // then get the string in between :" and " ...
    if(keysFound) {
        
        while(!end && !bool_found && (jsonRecordStartPtr + jsonRecordIndex < jsonBodyLength)) {
            thisChar = jsonBodyBuf[jsonRecordStartPtr + jsonRecordIndex++];
            
            switch(thisChar) {
                case ':':

                    p_bool = &jsonBodyBuf[jsonRecordStartPtr + jsonRecordIndex];
                    if (!memcmp(p_bool,"True",4) || !memcmp(p_bool,"true",4)) {
                        bool_found = true;
                        *p_bool_rtn = true;
                        break;
                    }

                    if (!memcmp(p_bool,"False",5) || !memcmp(p_bool,"false",5)) {
                        bool_found = true;
                        *p_bool_rtn = false;
                        break;
                    }
                    end = true;
                    break;
                default:
                    break;
            }
        }
    }

    return bool_found;
}

