#ifdef __cplusplus
extern "C" {
#endif
#ifndef _DD_PROTOCOL_H_
#define _DD_PROTOCOL_H_
// Commands and version
#define DD_VERSION 0x0d0d0003
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

#endif
#ifdef __cplusplus
}
#endif



