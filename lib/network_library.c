/*
 *  Copyright: (c) 2013 Unication Co., Ltd.
 *  All rights reserved.
 *
 *  Name: network_library.c
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

#include "network_library.h"
#include "error_code.h"

#if ( defined (NT_WINDOW) || defined (NT_LINUX) ) // {

/*******************************************************/
/*                      macro                          */
/*******************************************************/
#define MAX_SOCKETS                 (1)

#ifdef NT_WINDOW
#define WAIT_FOR_RECV               (20)
#define CONNECTION_TIMEOUT          (3)
#endif

enum NETWORK_STATU{ NETWORK_CONNECTED, NETWORK_DISSCONNECTED, NETWORK_ERROR}net_statu=NETWORK_DISSCONNECTED;

/*******************************************************/
/*                  global variables                   */
/*******************************************************/
int g_socket_to_server=-1;
int g_socket_to_client=-1;

/*******************************************************/
/*                      interfaces                     */
/*******************************************************/

/*
 *  create an socket and listen to it. Would be blocked, until a client connection
 *
 *  return >=0: success, return index of g_socket_arrays
 *         <0: negative fail, error code
 */
int network_listen_to(char *ip_addr, int port)
{
    int retval = -1;

#ifdef NT_LINUX

    int socket_server = -1;
    struct sockaddr_in sn;

    /* create server socket first */
    socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_server < 0) 
    {
        NT_DEBUG("socket(dat) error, error code is %d\n", errno);
        retval = ERROR_CODE_SOCKET;
        goto END;
    }

    memset(&sn, 0, sizeof(sn));
    sn.sin_family = AF_INET;
    sn.sin_port = htons((unsigned short)port);

    if (ip_addr)
    {
        NT_DEBUG("local address is %s\n", ip_addr);
        sn.sin_addr.s_addr = inet_addr(ip_addr); /* local address */
    }
    else
    {
        NT_DEBUG("local address is INADDR_ANY\n");
        sn.sin_addr.s_addr = htonl(INADDR_ANY); /* any interface */
    }

    /*
     * When using bind with the SO_REUSEADDR socket option, 
     * the socket option must be set prior to executing bind to have any affect.
     */
    const int val = 1;

    if (setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val)) < 0)
    {
        NT_DEBUG("setsockopt(SO_REUSEADDR) error, error code is %d\n", errno);
        close(socket_server);
        retval = ERROR_CODE_SETSOCKOPT;
        goto END;
    }

    if (bind(socket_server, (struct sockaddr *)&sn, sizeof(sn)) < 0) 
    {
        NT_DEBUG("bind() error, error code is %d\n", errno);
        close(socket_server);
        retval = ERROR_CODE_BIND;
        goto END;
    }
    
    if (listen(socket_server, 1) < 0) 
    {
        NT_DEBUG("listen() error, error code is %d\n", errno);
        close(socket_server);
        retval = ERROR_CODE_LISTEN;
        goto END;
    }


    struct sockaddr_in sn_client;
    int snlen = sizeof(sn_client);
    int socket_client = -1;    

    socket_client = accept(socket_server, (struct sockaddr *)&sn_client, &snlen);
    
    if (socket_client < 0)
    {
        NT_DEBUG("accept() error, error code is %d\n", errno);
        /* accept error, must close socket_server */
        close(socket_server);
        socket_server = -1;
            
        retval = ERROR_CODE_ACCEPT;

        goto END;
    } 

    unsigned long addr = ntohl(sn_client.sin_addr.s_addr);
    NT_DEBUG("Accept client (%d.%d.%d.%d:%04x) - OK\n",
             (addr >> 24) & 0xFF,
             (addr >> 16) & 0xFF,
             (addr >>  8) & 0xFF,
              addr        & 0xFF,
             sn_client.sin_port);

    /*return happy*/
    return retval=socket_client;
END:

#endif    

    return retval;    
}

/*
 *  create an socket and connect to a specified server. Would not be blocked
 * 
 *  return >=0, success socket index of g_socket_arrays
 *         <0: negative fail, error code
 */
int network_try2_connect(char *ip_addr, int port)
{
    int retval = -1;

#ifdef NT_WINDOW
    WSADATA wsaData;  
    WSAStartup(MAKEWORD(2, 2), &wsaData);  

    struct sockaddr_in dn;
    SOCKET socket_to_server = INVALID_SOCKET;
    
    socket_to_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    NT_DEBUG("Clinet: socket_to_server=%d, socket()\n", socket_to_server);

    if (socket_to_server == INVALID_SOCKET) 
    {
        NT_DEBUG("socket() error! error code is %d\n", WSAGetLastError());
        retval = ERROR_CODE_SOCKET;
        goto END;
    }

    NT_DEBUG("NET_DBG: Trying to connect, IP: %s, Port:0x%x\n", ip_addr, port);

    memset(&dn, 0, sizeof(dn));
    dn.sin_family = AF_INET;
    dn.sin_port = htons((unsigned short)port); /* server's cmd port */

    dn.sin_addr.s_addr = inet_addr(ip_addr); /* server address */

    unsigned long nonblock = 1;
    ioctlsocket(socket_to_server, FIONBIO, &nonblock); /* non block socket */

    connect(socket_to_server, (struct sockaddr *)&dn, sizeof(dn));

    {
        /* test connection */
        fd_set fdset_write;  // set fd write
        fd_set fdset_except;  // set fd except

        struct timeval timeout;

        FD_ZERO(&fdset_write);
        FD_SET(socket_to_server, &fdset_write);

        FD_ZERO(&fdset_except);
        FD_SET(socket_to_server, &fdset_except);

        /* delay 3s
         * that is enough for connection if server is ready
         * and if server is not ready, connection would
         * return with fail, caller would block only up to 300ms
         */
        timeout.tv_sec = CONNECTION_TIMEOUT; // 3s
        timeout.tv_usec = 0; 

        retval = select(1, NULL, &fdset_write, &fdset_except, &timeout);

        if (retval <= 0) /* timeout or error */
        {
            int err_code = WSAGetLastError();

            NT_DEBUG("connect() error, error code is %d\n", err_code);

            closesocket(socket_to_server);

            retval = ERROR_CODE_SELECT;

            goto END;
        }

        if (FD_ISSET(socket_to_server, &fdset_except))
        {
            int err_code = WSAGetLastError();

            NT_DEBUG("connect() error, error code is %d\n", err_code);

            closesocket(socket_to_server);

            retval = ERROR_CODE_SELECT;

            goto END;
        }

        g_socket_to_server=socket_to_server;
        NT_DEBUG("connect() succeed, g_socket_to_server is %d\n", g_socket_to_server);
    }

END:

#endif //NT_WINDOW

    return retval;
}

/*
 *  in linux, network_read_sync would not return, until recved at least one byte
 *            but can be interrupted by socket close
 *  in window, network_read_sync would return after timeout
 *
 *  return >0, success actual bytes to be read
 *         <0: negative fail, error code
 */
int network_read_sync(int Socket, unsigned char *buffer, int length)
{
    int retval = -1;
    
#ifdef NT_WINDOW
    SOCKET socket = Socket;

    if (socket == INVALID_SOCKET)
    {
        retval = ERROR_CODE_INVALID_SOCKET;
        goto END;
    }

    fd_set fdset_read;  // set fd read
    struct timeval timeout; 

    FD_ZERO(&fdset_read);
    FD_SET(socket, &fdset_read);

    timeout.tv_sec = WAIT_FOR_RECV; // delay 10 secs
    timeout.tv_usec = 0;

    retval = select(1, &fdset_read, NULL, NULL, &timeout);

    if (FD_ISSET(socket, &fdset_read))
    {
        retval = recv(socket, (unsigned char *)buffer, length, 0);

        if (retval > 0)
        {
            goto END;
        }
        else if (retval == 0)
        {
            retval = ERROR_CODE_SOCKET_CLOSE;
        }
        else
        {
            retval = ERROR_CODE_RECV_ERROR;
        }
    }
    else if (retval == 0)
    {
        NT_DEBUG("RECEIVE TIMEOUT\n");
        retval = ERROR_CODE_RECV_TIMEOUT;
    }
    else
    {
        NT_DEBUG("error\n");
        retval = ERROR_CODE_RECV_ERROR;
    }
  
#endif

#ifdef NT_LINUX
    int socket = Socket; 

    if (socket < 0)
    {
        retval = ERROR_CODE_INVALID_SOCKET;

        goto END;
    }    
    
    retval = recv(socket, (unsigned char *)buffer, length, 0);

    if (retval > 0)
    {
        goto END;
    }
    else if (retval == 0)
    {
        retval = ERROR_CODE_SOCKET_CLOSE;
    }
    else
    {
        retval = ERROR_CODE_RECV_ERROR;
    }    
#endif  

END:

    return retval;
}

/*
 *  in linux, network_write_sync would not return, until sent at least one byte
 *            but can be interrupted by socket close
 *  in window, network_write_sync would return after timeout
 *   
 *  return >0, success actual bytes to be written
 *         <0: negative fail, error code
 */
int network_write_sync(int Socket, unsigned char *buffer, int length)
{
    int retval = -1;
    int optval = -1;
    int optlen = sizeof(int); 
    int flags = 0;

    NT_DEBUG("Enter %s()\n", __FUNCTION__);

#ifdef NT_WINDOW
    SOCKET socket = Socket;

    if (socket == INVALID_SOCKET)
    {
        retval = ERROR_CODE_INVALID_SOCKET;
        goto END;
    }    
#endif

#ifdef NT_LINUX
    int socket = Socket; 

    if (socket < 0)
    {
        retval = ERROR_CODE_INVALID_SOCKET;

        goto END;
    }

    flags = MSG_NOSIGNAL;
#endif  

    //NT_DEBUG("Before getsockopt()\n");
    retval = getsockopt(socket, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen);

    if ((retval == 0) && (optval == 0))
    {
        /*
         *  would not fail, as send write data to socket buffer,
         *  which size is usually 8K, large enough to store
         *  our payload
         */
        retval = send(socket, (unsigned char *)buffer, length, flags);

        //NT_DEBUG("After socket send()\n");

        if (retval > 0)
        {
            goto END;
        }
        else if (retval == 0)
        {
            retval = ERROR_CODE_SOCKET_CLOSE;
        }
        else
        {
            retval = ERROR_CODE_SEND_ERROR;
        }
    }
    else
    {
        retval = ERROR_CODE_SEND_ERROR;
    }

END:

    return retval;
}



enum NETWORK_STATU check_network_status(void)
{

    if(net_statu==NETWORK_DISSCONNECTED)
        return NETWORK_DISSCONNECTED;
    if(net_statu==NETWORK_ERROR)
        return NETWORK_ERROR;
    /*previously statu is connected, do send & rec test make sure it's doing well,
    otherwise consider sth worng, change net_statu to error*/
    if(net_statu==NETWORK_CONNECTED)
    {
        return NETWORK_CONNECTED;
    }
    
}

/*
* send_to_server
* send command from clinet(PC here) to server(U3 device)
* @Network_command cmd: specify command and data that given to server(U3 device)
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
int send_to_server(struct Network_command *cmd)
{
    int retval=-1;
    if(g_socket_to_server < 0)
    {
        perror("ERROR_CODE_INVALID_SOCKET!\n");
        return ERROR_CODE_INVALID_SOCKET;
    }

    NT_DEBUG("Send_to_server() cmd_id=0x%x\n", cmd->command_id);

    switch(cmd->command_id)
    {
        case CHECK_BB_ROOT_PATITION:
        case CHECK_BB_USER_PATITON:
        case CHECK_NETWORK_STATU:
        case RECOVER_BB_ROOT_PATITION:
        case RECOVER_BB_USER_PATITION:
        case COMMAND_EXECUTE:
        case COMMAND_REBOOT:
        case COMMAND_NOT_MATCH:
            retval = network_write_sync(g_socket_to_server, (unsigned char *)cmd, sizeof(struct Network_command));
        break;

        default :
        { 
            retval=ERROR_CODE_CMD_ERROR;
            break;
        }
    }
    

    return retval;
}

/*
* recv_from_server
* receive acknowledgement and data from server(U3 device)
* @Network_command cmd: sturct to get specify command acknowledgement and data
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
int recv_from_server(struct Network_command *cmd)
{
    int retval=-1;
    if(g_socket_to_server < 0)
    {
        perror("ERROR_CODE_INVALID_SOCKET!\n");
        return ERROR_CODE_INVALID_SOCKET;
    }

    retval = network_read_sync(g_socket_to_server, (unsigned char *)cmd, sizeof(struct Network_command));
    if(retval < 0)
        return retval;

    NT_DEBUG("recv_from_server: receive command id=0x%x\n", cmd->command_id);

    /*retval >=0, return happy*/
    return retval;
}

/*
* send_to_client
* send command acknowledgement and data from server(U3 device) to clinet(PC)
* @Network_command cmd: specify command acknowledgement and data that send to the client(PC)
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
int send_to_client(struct Network_command *cmd)
{
    int retval=-1;
    if(g_socket_to_client < 0)
    {
        perror("ERROR_CODE_INVALID_SOCKET!\n");
        return ERROR_CODE_INVALID_SOCKET;
    }

    NT_DEBUG("send_to_client: cmd_id=0x%x\n", cmd->command_id);

    switch(cmd->command_id)
    {
        case CHECK_BB_ROOT_PATITION_ACK:
        case CHECK_BB_USER_PATITON_ACK:
        case CHECK_NETWORK_STATU_ACK:
        case RECOVER_BB_ROOT_PATITION_ACK:
        case RECOVER_BB_USER_PATITION_ACK:
        case RECOVER_BB_FINISH_ACK:
        case COMMAND_UNKNOW_ACK:
        case COMMAND_OPERATION_ERROR_ACK:
        case COMMAND_REBOOT_ACK:
            retval = network_write_sync(g_socket_to_client, (unsigned char *)cmd, sizeof(struct Network_command));
        break;

        default :
        { 
            NT_DEBUG("Error: unexpect command id!!\n");
            retval=ERROR_CODE_CMD_ERROR;
            break;
        }
    }
    

    return retval;
}

/*
* recv_from_client
* receive command from client(PC)
* @Network_command cmd: sturct to get specify command and data
* Return >= 0 indicates succeed, Return < 0 indicates failure.
*/
int recv_from_client(struct Network_command *cmd)
{
    int retval=-1;
    NT_DEBUG("Enter %s()\n", __FUNCTION__);
    if(g_socket_to_client < 0)
    {
        perror("ERROR_CODE_INVALID_SOCKET!\n");
        return ERROR_CODE_INVALID_SOCKET;
    }

    retval = network_read_sync(g_socket_to_client, (unsigned char *)cmd, sizeof(struct Network_command));
    NT_DEBUG("recv_from_client: retval=%d, receive command id=0x%x\n", retval, cmd->command_id);

    return retval;
}

int server_ack_to_client(struct Network_command *cmd)
{
    struct Network_command server_ack;
    int retval=-1;
    switch(cmd->command_id) 
    {
        case CHECK_BB_ROOT_PATITION : 
            server_ack.command_id=CHECK_BB_ROOT_PATITION_ACK; 
            retval=send_to_client(&server_ack);
            break; 

        case CHECK_BB_USER_PATITON : 
            server_ack.command_id=CHECK_BB_USER_PATITON_ACK; 
            retval=send_to_client(&server_ack);
            break; 

        case CHECK_NETWORK_STATU : 
            server_ack.command_id=CHECK_NETWORK_STATU_ACK; 
            retval=send_to_client(&server_ack);
            break; 

        case RECOVER_BB_ROOT_PATITION : 
            server_ack.command_id=RECOVER_BB_ROOT_PATITION_ACK; 
            retval=send_to_client(&server_ack);
            break; 

        case RECOVER_BB_USER_PATITION : 
            server_ack.command_id=RECOVER_BB_USER_PATITION_ACK; 
            retval=send_to_client(&server_ack);
            break; 

        case COMMAND_REBOOT : 
            server_ack.command_id=COMMAND_REBOOT_ACK; 
            retval=send_to_client(&server_ack);
            break; 

        default:
            NT_DEBUG("Error: unexpect command acknowledgement id!!\n");
            server_ack.command_id=COMMAND_UNKNOW_ACK; 
            send_to_client(&server_ack);
            retval=ERROR_CODE_CMD_ERROR;
            break;

    }
    return retval;

}

int client_ack_confirm(struct Network_command *cmd, struct Network_command *cmd_ack)
{
    int retval = -1;
    NT_DEBUG("Enter %s, cmd->command_id=0x%x, cmd_ack->command_id=0x%x\n", __FUNCTION__, cmd->command_id, cmd_ack->command_id);
#ifdef NT_WINDOW

    BOOL is_command_match=FALSE;

    switch(cmd_ack->command_id)
    {
        case CHECK_BB_ROOT_PATITION_ACK:
            if(cmd->command_id==CHECK_BB_ROOT_PATITION)
            {
                is_command_match=TRUE;
                cmd->command_id=COMMAND_EXECUTE;
                retval=send_to_server(cmd);
            }
            break;

        case CHECK_BB_USER_PATITON_ACK:
            if(cmd->command_id==CHECK_BB_USER_PATITON)
            {
                is_command_match=TRUE;
                cmd->command_id=COMMAND_EXECUTE;
                retval=send_to_server(cmd);
            }
            break;

        case CHECK_NETWORK_STATU_ACK:
            if(cmd->command_id==CHECK_NETWORK_STATU)
            {
                is_command_match=TRUE;
                cmd->command_id=COMMAND_EXECUTE;
                retval=send_to_server(cmd);
            }
            break;

        case RECOVER_BB_ROOT_PATITION_ACK:
            if(cmd->command_id==RECOVER_BB_ROOT_PATITION)
            {
                is_command_match=TRUE;
                cmd->command_id=COMMAND_EXECUTE;
                retval=send_to_server(cmd);
            }
            break;

        case RECOVER_BB_USER_PATITION_ACK:
            if(cmd->command_id==RECOVER_BB_USER_PATITION)
            {
                is_command_match=TRUE;
                cmd->command_id=COMMAND_EXECUTE;
                retval=send_to_server(cmd);
            }
            break;
        case COMMAND_REBOOT_ACK:
            if(cmd->command_id==COMMAND_REBOOT)
            {
                is_command_match=TRUE;
                cmd->command_id=COMMAND_EXECUTE;
                retval=send_to_server(cmd);
            }
            break;

        /*Command verify fialed*/
        default:
            NT_DEBUG("Error: Unknow command ack id!!\n");
            cmd->command_id=COMMAND_UNKNOW_ACK;
            send_to_server(cmd);
            retval=ERROR_CODE_UNKNOW_CMD_ACK;
            return retval;
            break;
            
    }

    /*if command do not matching with commmand ack, return error*/

    if(is_command_match==FALSE)
    {
        NT_DEBUG("Error: intended command not matching command ack!!\n");
        cmd->command_id=COMMAND_NOT_MATCH;
        retval=send_to_server(cmd);
        retval=ERROR_CODE_CMD_ACK_NOT_MATCH;
    }


#endif //NT_WINDOW

#ifdef NT_LINUX
    NT_DEBUG("Error: Founction not realized in linux!!\n");
    retval=ERROR_CODE_FUNCTION_NOT_REALIZE;
    
#endif //NT_LINUX
    return retval;
}

#ifdef NT_WINDOW
int windows_send_command(struct Network_command *cmd)
{
    int retval=-1;
    struct Network_command *command_ack = NULL;
    command_ack= (struct Network_command *)malloc(sizeof(struct Network_command) );
    if(command_ack==NULL)
    {
        perror("Can't allocate memory for struct Network_command!\n");
        exit(-1);
    }
    memset(command_ack, 0, sizeof(struct Network_command));
    NT_DEBUG("g_socket_to_server: %d\n", g_socket_to_server);  
    NT_DEBUG("Sent to server: 0x%x\n", cmd->command_id);  
    
    retval=send_to_server(cmd);
    if(retval <0) 
    {   
        printf("Error in send_to_server(),  error code is: %d\n",retval);
        goto END;
    }   
    
    retval=recv_from_server(command_ack);
    if(retval <0) 
    {   
        printf("Error in recv_from_server(),  error code is: %d\n",retval);
        goto END;
    }   

    NT_DEBUG("Receive from server: 0x%x\n", command_ack->command_id);  

    retval=client_ack_confirm(cmd, command_ack);
    if(retval <0) 
    {   
        printf("Error in client_ack_confirm(),  error code is: %d\n",retval);
        goto END;
    }   
    
END:
    free(command_ack);
    return retval;

}
#endif //NT_WINDOW


#endif // ( defined (NT_WINDOW) || defined (NT_LINUX) ) }
