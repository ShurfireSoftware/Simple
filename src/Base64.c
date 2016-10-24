#include <string.h>
#include "Base64.h"
#include "config.h"

void ConvertToBase64(char *p_input, uint16_t len, char *p_result)
{
    uint8_t triplet_len;
    char *p_cnvrt;
    p_cnvrt = p_input;
    for ( ; len != 0; len -= triplet_len) {
        
        if (len > 3) {
            triplet_len = 3;
        }
        else {
            triplet_len = len;
        }

        processTriplet((uint8_t*)p_cnvrt, triplet_len, p_result);
        p_cnvrt += triplet_len;
        p_result += 4;
    }

    *p_result = 0;
}

char getValue(char input)
{
    char value;
    
    // input 0 to 25, starting with value 'A' (65)
    if(input < 26) value = input + 65;
    
    // input 26 to 51, starting with value 'a' (97)
    else if(input < 52) value = input + 71;
    
    // input 52 to 61, starting with '0' (48)
    else if(input < 62) value = input - 4;
    
    // input 62 gets value '+'
    else if(input == 62) value = '+';
    
    // input 63 gets value '/'
    // else if(input == 63) value = '/';
       
    // input 63 gets value '@'
    else if(input == 63) value = '@';	// this works better in a JSON body

    return value;
}
//-----------------------------------------------------------------------------

char getBackValue(char input)
{
    char value;
    
    // value 0 to 25, starting with input 65
    if((input >= 'A') && (input <= 'Z')) value = input - 65;
    
    // value 26 to 51, starting with input 97
    else if((input >= 'a') && (input <= 'z')) value = input - 71;
    
    // value 52 to 61, starting with input 48
    else if((input >= '0') && (input <= '9')) value = input + 4;
    
    // value = 62
    else if(input == '+') value = 62;
    
    // value = 63
    // else if(input == '/') value = 63;
    
    // value = 63
    else if(input == '@') value = 63;
    
    return value;
}
//-----------------------------------------------------------------------------

void processTriplet(unsigned char *triplet, uint8_t tripletLength, char *quartet)
{
    uint8_t byte1, byte2;
    
    // first six bits of first input byte
    byte1 = triplet[0] >> 2;
    quartet[0] = getValue(byte1);
    
    // last two bits of first input byte + first four bits of second input byte
    byte1 = (triplet[0] & 0x03) << 4;
    byte2 = (tripletLength > 1) ? triplet[1] >> 4 : 0;
    quartet[1] = getValue(byte1 | byte2);
    
    // last four bits of second input byte + first two bits of third input byte
    if(tripletLength > 1) {
        byte1 = (triplet[1] & 0x0F) << 2;
        byte2 = (tripletLength > 2) ? triplet[2] >> 6 : 0;
        quartet[2] = getValue(byte1 | byte2);
        
        // last six bits of third input byte
        if(tripletLength > 2) {
            byte1 = triplet[2] & 0x3F;
            quartet[3] = getValue(byte1);
        }
        
        // padding
        else quartet[3] = '=';
    }
    else {
        
        // even more padding
        quartet[2] = '=';
        quartet[3] = '=';
    }
}
//-----------------------------------------------------------------------------

uint8_t processQuartet(char *quartet, unsigned char *triplet)
{
    // first byte of triplet
    // first six bits: 6 bit value of quartet[0] - last 2 bits: first 2 bits of 6 bit value of quartet[1];
    char byte1, byte2, byte3, byte4;
	uint8_t tripletLength;
	
    byte1 = getBackValue(quartet[0]);
    byte2 = getBackValue(quartet[1]);
    
    triplet[0] = (byte1 << 2) | (byte2 >> 4);
    
    tripletLength = 3;
    if(quartet[3] == '=') tripletLength = 2;
    if(quartet[2] == '=') tripletLength = 1;
    
    if(tripletLength > 1) {
        
        // second byte of triplet
        // first four bits: last four bits of 6 bit value of quartet[1] - last four bits: first 4 bits of 6 bit value of quartet[2];
        byte3 = getBackValue(quartet[2]);
        
        triplet[1] = (byte2 << 4) | (byte3 >> 2);
        
        if(tripletLength > 2) {
            
            // third byte of triplet
            // first two bits: last two bits of 6 bit value of quartet[1] - last six bits: 6 bit value of quartet[2];
            byte4 = getBackValue(quartet[3]);
            
            triplet[2] = (byte3 << 6) | byte4;
        }
        else triplet[2] = 0;
    }
    else triplet[1] = 0;
    
    return tripletLength;
}
//-----------------------------------------------------------------------------
