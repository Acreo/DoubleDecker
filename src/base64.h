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

#ifndef BASE64_H_INCLUDED
#define BASE64_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _base64_t base64_t;

//  @interface
//  Create a new base64
DD_EXPORT base64_t *
base64_new(void);

//  Destroy the base64
DD_EXPORT void
base64_destroy(base64_t **self_p);

//  Self test of this class
DD_EXPORT void
base64_test(bool verbose);

//  @end

typedef enum {
    step_a, step_b, step_c, step_d
} base64_decodestep;

typedef struct {
    base64_decodestep step;
    char plainchar;
} base64_decodestate;

void base64_init_decodestate(base64_decodestate *state_in);

int base64_decode_value(char value_in);

int base64_decode_block(const char *code_in, const int length_in, char *plaintext_out, base64_decodestate *state_in);

typedef enum {
    step_A, step_B, step_C
} base64_encodestep;

typedef struct {
    base64_encodestep step;
    char result;
    int stepcount;
} base64_encodestate;

void base64_init_encodestate(base64_encodestate *state_in);

char base64_encode_value(char value_in);

int base64_encode_block(const char *plaintext_in, int length_in, char *code_out,
                        base64_encodestate *state_in);

int base64_encode_blockend(char *code_out, base64_encodestate *state_in);

#ifdef __cplusplus
}
#endif

#endif
