/*  =========================================================================
    base64 - class description

        Copyright (c) 2015 Pontus Sköldström, Bertrand Pechenot           
                                                                            
        This file is part of libdd, the DoubleDecker hierarchical           
        messaging system DoubleDecker is free software; you can             
        redistribute it and/or modify it under the terms of the GNU Lesser  
        General Public License (LGPL) version 2.1 as published by the Free  
        Software Foundation.                                                
                                                                            
        As a special exception, the Authors give you permission to link this
        library with independent modules to produce an executable,          
        regardless of the license terms of these independent modules, and to
        copy and distribute the resulting executable under terms of your    
        choice, provided that you also meet, for each linked independent    
        module, the terms and conditions of the license of that module. An  
        independent module is a module which is not derived from or based on
        this library.  If you modify this library, you must extend this     
        exception to your version of the library.  DoubleDecker is          
        distributed in the hope that it will be useful, but WITHOUT ANY     
        WARRANTY; without even the implied warranty of MERCHANTABILITY or   
        FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public 
        License for more details.  You should have received a copy of the   
        GNU Lesser General Public License along with this program.  If not, 
        see http://www.gnu.org/licenses/.                                   
    =========================================================================
*/

/*
@header
    base64 - 
@discuss
@end
*/

#include "dd_classes.h"


//  --------------------------------------------------------------------------
//  Self test of this class

void
base64_test (bool verbose)
{
    printf (" * base64: ");

    //  @selftest
    //  Simple create/destroy test

    //  @end
    printf ("OK\n");
}


const int CHARS_PER_LINE = 72;

void base64_init_encodestate(base64_encodestate *state_in) {
    state_in->step = step_A;
    state_in->result = 0;
    state_in->stepcount = 0;
}

char base64_encode_value(char value_in) {
    static const char *encoding =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (value_in > 63)
        return '=';
    return encoding[(int)value_in];
}

int base64_encode_block(const char *plaintext_in, int length_in, char *code_out,
                        base64_encodestate *state_in) {
    const char *plainchar = plaintext_in;
    const char *const plaintextend = plaintext_in + length_in;
    char *codechar = code_out;
    char result;
    char fragment;

    result = state_in->result;

    switch (state_in->step) {
        while (1) {
            case step_A:
                if (plainchar == plaintextend) {
                    state_in->result = result;
                    state_in->step = step_A;
                    return codechar - code_out;
                }
            fragment = *plainchar++;
            result = (fragment & 0x0fc) >> 2;
            *codechar++ = base64_encode_value(result);
            result = (fragment & 0x003) << 4;
            case step_B:
                if (plainchar == plaintextend) {
                    state_in->result = result;
                    state_in->step = step_B;
                    return codechar - code_out;
                }
            fragment = *plainchar++;
            result |= (fragment & 0x0f0) >> 4;
            *codechar++ = base64_encode_value(result);
            result = (fragment & 0x00f) << 2;
            case step_C:
                if (plainchar == plaintextend) {
                    state_in->result = result;
                    state_in->step = step_C;
                    return codechar - code_out;
                }
            fragment = *plainchar++;
            result |= (fragment & 0x0c0) >> 6;
            *codechar++ = base64_encode_value(result);
            result = (fragment & 0x03f) >> 0;
            *codechar++ = base64_encode_value(result);

            ++(state_in->stepcount);
            if (state_in->stepcount == CHARS_PER_LINE / 4) {
                *codechar++ = '\n';
                state_in->stepcount = 0;
            }
        }
    }
    /* control should not reach here */
    return codechar - code_out;
}

int base64_encode_blockend(char *code_out, base64_encodestate *state_in) {
    char *codechar = code_out;

    switch (state_in->step) {
        case step_B:
            *codechar++ = base64_encode_value(state_in->result);
            *codechar++ = '=';
            *codechar++ = '=';
            break;
        case step_C:
            *codechar++ = base64_encode_value(state_in->result);
            *codechar++ = '=';
            break;
        case step_A:
            break;
    }
    *codechar++ = '\n';

    return codechar - code_out;
}



int base64_decode_value(char value_in) {
    static const char decoding[] = {
            62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1,
            -1, -1, -2, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
            10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
            -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
            36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};
    static const char decoding_size = sizeof(decoding);
    value_in -= 43;
    if (value_in < 0 || value_in > decoding_size)
        return -1;
    return decoding[(int)value_in];
}

void base64_init_decodestate(base64_decodestate *state_in) {
    state_in->step = step_a;
    state_in->plainchar = 0;
}

int base64_decode_block(const char *code_in, const int length_in,
                        char *plaintext_out, base64_decodestate *state_in) {
    const char *codechar = code_in;
    char *plainchar = plaintext_out;
    char fragment;

    *plainchar = state_in->plainchar;

    switch (state_in->step) {
        while (1) {
            case step_a:
                do {
                    if (codechar == code_in + length_in) {
                        state_in->step = step_a;
                        state_in->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
            *plainchar = (fragment & 0x03f) << 2;
            case step_b:
                do {
                    if (codechar == code_in + length_in) {
                        state_in->step = step_b;
                        state_in->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
            *plainchar++ |= (fragment & 0x030) >> 4;
            *plainchar = (fragment & 0x00f) << 4;
            case step_c:
                do {
                    if (codechar == code_in + length_in) {
                        state_in->step = step_c;
                        state_in->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
            *plainchar++ |= (fragment & 0x03c) >> 2;
            *plainchar = (fragment & 0x003) << 6;
            case step_d:
                do {
                    if (codechar == code_in + length_in) {
                        state_in->step = step_d;
                        state_in->plainchar = *plainchar;
                        return plainchar - plaintext_out;
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
            *plainchar++ |= (fragment & 0x03f);
        }
    }
    /* control should not reach here */
    return plainchar - plaintext_out;
}
