/*
 *  Copyright: (c) 2013 Unication Co., Ltd.
 *  All rights reserved.
 *
 *  Name: network_library.h
 *
 *  Created By: kamingli <kaming_li@unication.com.cn>
 *  Created Date: 2012-05-24
 *
 *  This program defines macros used in file transfer module 
 *  for shakehand transmit and receive.
 *
 *  This program is free software; you can redistribute it 
 *  and/or modify it under the terms of the GNU General Public 
 *  License version 2 as published by the Free Software 
 *  Foundation.
 *
 *  ChangLog:
 *  Date                Author              Purpose
 *  2012-05-24          kamingli            Create
 *
 */

#if ( defined (NT_WINDOW) || defined (NT_LINUX) ) // {

#ifndef _NETWORK_LIB_H_
#define _NETWORK_LIB_H_

#ifdef __cplusplus
extern "C"
{

#endif //__cplusplus

#ifdef NT_LINUX // {
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <linux/types.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <sys/mman.h>
#include <linux/sockios.h>
#include <linux/netlink.h>
#endif // NT_LINUX }

#ifdef NT_WINDOW // {
#include <winsock2.h>
#include <windows.h>

#include <stdio.h>
#endif  // NT_WINDOW }

/*******************************************************/
/*                      macro                          */
/*******************************************************/
/*
 * debug control
 */
#define ENABLE_DEBUG_X

#ifdef ENABLE_DEBUG
    #define NT_DEBUG(fmt, args...)  printf(fmt, ## args)
#else
    #define NT_DEBUG(fmt, args...)
#endif

#define LISTENING_PORT    (0x8989)
#define IQ_CAPTURE_LISTENING_PORT    (0x3090)
#define FIXED_DATA_SIZE 1
enum command_ID{ 
    CHECK_BB_ROOT_PATITION =0xAA,
    CHECK_BB_ROOT_PATITION_ACK, 
    CHECK_BB_USER_PATITON,      //0xAC 
    CHECK_BB_USER_PATITON_ACK, 
    CHECK_NETWORK_STATU,        //0xAE
    CHECK_NETWORK_STATU_ACK,
    RECOVER_BB_ROOT_PATITION,   //0xB0
    RECOVER_BB_ROOT_PATITION_ACK,
    RECOVER_BB_USER_PATITION,   //0xB2
    RECOVER_BB_USER_PATITION_ACK,
    RECOVER_BB_FINISH_ACK,      //0xB4
    COMMAND_EXECUTE,             
    COMMAND_UNKNOW_ACK,         //0xB6
    COMMAND_NOT_MATCH,          
    COMMAND_OPERATION_ERROR_ACK,  //0xB8
    COMMAND_REBOOT,             
    COMMAND_REBOOT_ACK             
};

struct Network_command
{
    enum command_ID command_id;
    unsigned int data; 
    
};


/*******************************************************/
/*                  global variables                   */
/*******************************************************/
extern int g_socket_to_server;
extern int g_socket_to_client;

/*******************************************************/
/*                       declare                       */
/*******************************************************/

/*
 *  return 0: success
 *         <0: negative fail, error code
 */
extern int network_initial(void);

/*
 *  create an socket and listen to it. Would be blocked, until a client connection
 *
 *  return >=0: success, return index of g_socket_arrays
 *         <0: negative fail, error code
 */
extern int network_listen_to(char *ip_addr, int port);

/*
 *  create an socket and connect to a specified server. Would not be blocked
 * 
 *  return >=0, success socket index of g_socket_arrays
 *         <0: negative fail, error code
 */
extern int network_try2_connect(char *ip_addr, int port);

/*
 *  in linux, network_read_sync would not return, until recved at least one byte
 *            but can be interrupted by socket close
 *  in window, network_read_sync would return after timeout
 *
 *  return >0, success actual bytes to be read
 *         <0: negative fail, error code
 */
extern int network_read_sync(int Socket, unsigned char *buffer, int length);

/*
 *  in linux, network_write_sync would not return, until sent at least one byte
 *            but can be interrupted by socket close
 *  in window, network_write_sync would return after timeout
 *   
 *  return >0, success actual bytes to be written
 *         <0: negative fail, error code
 */
extern int network_write_sync(int Socket, unsigned char *buffer, int length);

extern void network_close(int handler_index);

extern void network_release(void);


/*
* send_to_server
* send command from clinet(PC here) to server(U3 device)
* @Network_command cmd: specify command and data that given to server(U3 device)
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
extern int send_to_server(struct Network_command *cmd);

/*
* recv_from_server
* receive acknowledgement and data from server(U3 device)
* @Network_command cmd: sturct to get specify command acknowledgement and data
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
extern int recv_from_server(struct Network_command *cmd);

/*
* send_to_client
* send command acknowledgement and data from server(U3 device) to clinet(PC)
* @Network_command cmd: specify command acknowledgement and data that send to the client(PC)
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
extern int send_to_client(struct Network_command *cmd);

/*
* recv_from_client
* receive command from client(PC)
* @Network_command cmd: sturct to get specify command and data
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
extern int recv_from_client(struct Network_command *cmd);

/*
* server_ack_to_client
* receiver(U3) send ack to client(PC), according the given parameter command id(@cmd)
* @Network_command cmd: sturct of command and data
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
extern int server_ack_to_client(struct Network_command *cmd);

/*
* client_ack_confirm
* @Network_command *cmd: sturct of command and data
* @Network_command *cmd_ack: sturct of command and data(command ack, receive from server)
* Compare command id from *cmd and the *cmd_ack, if they're corresponded then sent COMMAND_EXECUTE to server, finally execute the command on server side.
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
extern int client_ack_confirm(struct Network_command *cmd, struct Network_command *cmd_ack);

/*
* windows_send_command
* @Network_command *cmd: sturct of command and data sent from windows side
*
* Sent command from windows to target(linux devices),
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
extern int windows_send_command(struct Network_command *cmd);

#ifdef __cplusplus
}

#endif //__cplusplus

#endif // _NETWORK_LIB_H_

#endif // ( defined (NT_WINDOW) || defined (NT_LINUX) ) }


