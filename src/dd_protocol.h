/*  =========================================================================
    dd_protocol - class description

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

#ifndef DD_PROTOCOL_H_INCLUDED
#define DD_PROTOCOL_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

// Commands and version
#define DD_CMD_SEND 0
#define DD_CMD_FORWARD 1
#define DD_CMD_PING 2
#define DD_CMD_ADDLCL 3
#define DD_CMD_ADDDCL 4
#define DD_CMD_ADDBR 5
#define DD_CMD_UNREG 6
#define DD_CMD_UNREGDCLI 7
#define DD_CMD_UNREGBR 8
#define DD_CMD_DATA 9
#define DD_CMD_ERROR 10
#define DD_CMD_REGOK 11
#define DD_CMD_PONG 12
#define DD_CMD_CHALL 13
#define DD_CMD_CHALLOK 14
#define DD_CMD_PUB 15
#define DD_CMD_SUB 16
#define DD_CMD_UNSUB 17
#define DD_CMD_SENDPUBLIC 18
#define DD_CMD_PUBPUBLIC 19
#define DD_CMD_SENDPT 20
#define DD_CMD_FORWARDPT 21
#define DD_CMD_DATAPT 22
#define DD_CMD_SUBOK 23

#define DD_CALLBACK  0
#define DD_ACTOR  1


extern const uint32_t dd_cmd_send;
extern const uint32_t dd_cmd_forward;
extern const uint32_t dd_cmd_ping;
extern const uint32_t dd_cmd_addlcl;
extern const uint32_t dd_cmd_adddcl;
extern const uint32_t dd_cmd_addbr;
extern const uint32_t dd_cmd_unreg;
extern const uint32_t dd_cmd_unregdcli;
extern const uint32_t dd_cmd_unregbr;
extern const uint32_t dd_cmd_data;
extern const uint32_t dd_cmd_error;
extern const uint32_t dd_cmd_regok;
extern const uint32_t dd_cmd_pong;
extern const uint32_t dd_cmd_chall;
extern const uint32_t dd_cmd_challok;
extern const uint32_t dd_cmd_pub;
extern const uint32_t dd_cmd_sub;
extern const uint32_t dd_cmd_unsub;
extern const uint32_t dd_cmd_sendpublic;
extern const uint32_t dd_cmd_pubpublic;
extern const uint32_t dd_cmd_sendpt;
extern const uint32_t dd_cmd_forwardpt;
extern const uint32_t dd_cmd_datapt;
extern const uint32_t dd_cmd_subok;
extern const uint32_t dd_version;
extern const uint32_t dd_error_regfail;
extern const uint32_t dd_error_nodst;
extern const uint32_t dd_error_version;

DD_EXPORT void
dd_protocol_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
