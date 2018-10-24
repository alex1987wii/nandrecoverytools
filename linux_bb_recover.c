
/*
 *  linux_server.c
 *
 *  Copyright (C) 2000 David Woodhouse (dwmw2@infradead.org)
 *                     Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 * 
 */

#define PROGRAM_NAME "linux_bb_recover"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <getopt.h>

#include <asm/types.h>
#include "mtd/mtd-user.h"
#include <libmtd.h>
#include <network_library.h>
#include <error_code.h>
#include <nand_bd_recover.h>
#include <pthread.h>

#define NET_PORT 0x8989
#define BB_RECOVER_REPORT_INTERVAL 5   //6sec

static const unsigned char TARGET_IP[]={"10.10.0.12"};
static pthread_t thr_sent_inform;
extern pthread_mutex_t mutex_bb_recover_num;
extern uint32_t g_bb_recover_num;

#define SERVER_DEBUG_ENABLE                                       
                                                           
#ifdef SERVER_DEBUG_ENABLE                                          
    #define SERVER_DEBUG(fmt, args...)  printf(fmt, ## args)   
#else                                                      
    #define SERVER_DEBUG(fmt, args...)                         
#endif 

static void *nand_send_inform_process(void *command)
{
    struct Network_command *bb_recover_ack = NULL;
    bb_recover_ack=(struct Network_command *)command;
    uint32_t tmp_bb_recover_num_new=0;
    uint32_t tmp_bb_recover_num_old=0;

    printf("DBG------>bb_recover_ack->command_id=%0X\n", bb_recover_ack->command_id);

    while(1)
    {
        sleep(BB_RECOVER_REPORT_INTERVAL);

        pthread_mutex_lock( &mutex_bb_recover_num);
        tmp_bb_recover_num_new=g_bb_recover_num;
        pthread_mutex_unlock( &mutex_bb_recover_num);
        
        /*If there is no more bad block(s) has been recovered after 6secs interval, it indicatesthe recover process had finished or error occours, so terminate this thread. */
        if(tmp_bb_recover_num_new >tmp_bb_recover_num_old)
        {
            bb_recover_ack->data=tmp_bb_recover_num_new;
            send_to_client(bb_recover_ack); 
            tmp_bb_recover_num_old=tmp_bb_recover_num_new;
            printf("DBG------> %d bad blocks have been recover\n", tmp_bb_recover_num_new);

        }
        else
            break;
        
    }
    printf("DBG------>Thread: nand_send_inform_process EXIT!\n");
    return ((void *)0);
}
static void print_command_name(enum command_ID cmd_id)
{
    printf("command_ID=%s\n",
    cmd_id==CHECK_BB_ROOT_PATITION ? 
    "CHECK_BB_ROOT_PATITION":
    cmd_id==CHECK_BB_USER_PATITON ? 
    "CHECK_BB_USER_PATITON" : 
    cmd_id==CHECK_NETWORK_STATU ? 
    "CHECK_NETWORK_STATU" : 
    cmd_id==RECOVER_BB_ROOT_PATITION ? 
    "RECOVER_BB_ROOT_PATITION":
    cmd_id==RECOVER_BB_USER_PATITION ? 
    "RECOVER_BB_USER_PATITION" :
    cmd_id==COMMAND_EXECUTE ? 
    "COMMAND_EXECUTE" :"UNKNOW_COMMAND!"
);

}
int wati_for_cmd(int Socket, struct Network_command *cmd)
{
    g_socket_to_client = Socket;

    SERVER_DEBUG("Enter function: %s\n", __FUNCTION__);  

    int retval = -1;
    struct Network_command *command_client_ack = NULL;
    command_client_ack= (struct Network_command *)malloc(sizeof(struct Network_command) );
    if(command_client_ack==NULL)
    {
        perror("Can't allocate memory for struct Network_command!\n");
        retval = ERROR_CODE_MALLOC_ERROR;
        goto END;
    }
    memset(command_client_ack, 0, sizeof(struct Network_command));

    /*Get the command from client(PC), will block until recv data or error occur*/
    retval=recv_from_client(cmd);
    if(retval <0)
    {
        printf("Error in recv_from_client(),  error code is: %d\n",retval);
        goto END;
    }

    /*ACK to client(PC) according the command that previously acquire*/
    retval=server_ack_to_client(cmd);
    if(retval <0)
    {
        printf("Error in send_to_client(),  error code is: %d\n",retval);
        goto END;
    }

    retval=recv_from_client(command_client_ack);
    if(retval <0)
    {
        printf("Error in recv_from_client(),  error code is: %d\n",retval);
        goto END;
    }
    /*Handshak succeed if we receive COMMAND_EXECUTE from client*/
    if(command_client_ack->command_id==COMMAND_EXECUTE)
        return cmd->command_id;
    else
        goto END;

END:
    free(command_client_ack);
    //close(g_socket_to_client);
    /* error occured, close socket unilaterally */
    perror("Error occur on reciver!\n");
    return retval;

}

/*loop routine, handle command from clinet(PC)*/
int loop_routine(int Socket)
{
    int retval=-1;
    struct Network_command *command = NULL;
    command= (struct Network_command *)malloc(sizeof(struct Network_command) );
    if(command==NULL)
    {
        perror("Can't allocate memory for struct Network_command!\n");
        retval = ERROR_CODE_MALLOC_ERROR;
        goto END;
    }
    memset(command, 0, sizeof(struct Network_command));
    retval=wati_for_cmd(Socket, command);
    /*Occor error*/
    if(retval <0 )
        goto END;

    print_command_name(command->command_id);

    /*Succesfully get the command from client(PC)*/    
    switch(command->command_id)
    {
        case CHECK_BB_ROOT_PATITION:
            printf("Execute: CHECK_BB_ROOT_PATITION\n");  
            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            retval=get_bd_number("Root file system");
            if(retval<0)
            {
                /*Operate get_bd_number error, report to client(PC)*/
                printf("Error in get_bd_number(),  error code is: %d\n",retval);
                command->command_id=COMMAND_OPERATION_ERROR_ACK;
                command->data=retval;
                retval=send_to_client(command);
                break;
            }
            command->data=retval;
            SERVER_DEBUG("command->data=%d\n", command->data);
            command->command_id=CHECK_BB_ROOT_PATITION_ACK;

            retval=send_to_client(command);
            printf("DBG-------> retval=%d\n", retval);
            break;

        case CHECK_BB_USER_PATITON:
            printf("Execute: CHECK_BB_USER_PATITON\n");  
            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            retval=get_bd_number("user data");
            if(retval<0)
            {
                /*Operate get_bd_number error, report to client(PC)*/
                printf("Error in get_bd_number(),  error code is: %d\n",retval);
                command->command_id=COMMAND_OPERATION_ERROR_ACK;
                command->data=retval;
                retval=send_to_client(command);
                break;
            }
            command->data=retval;
            SERVER_DEBUG("command->data=%d\n", command->data);
            command->command_id=CHECK_BB_USER_PATITON_ACK;

            retval=send_to_client(command);
            printf("DBG-------> retval=%d\n", retval);

            break;

        case CHECK_NETWORK_STATU:
            printf("Execute: CHECK_NETWORK_STATU\n");  
            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            command->command_id=CHECK_NETWORK_STATU_ACK;
            retval=send_to_client(command);
            printf("DBG-------> retval=%d\n", retval);
            break;

        case RECOVER_BB_ROOT_PATITION:
            printf("Execute: RECOVER_BB_ROOT_PATITION\n");  

            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            command->command_id = RECOVER_BB_ROOT_PATITION_ACK;
            command->data = 0;
            /*  create a thread, send recover inform to client(PC) periodically */
            retval = pthread_create(&thr_sent_inform, NULL, nand_send_inform_process, (void*)command); 
            if (retval) {
                printf("DBG---------> create thread for nand read test failed! error code: %d\n", ERROR_CODE_THREAD_CREATE_FAILED);
                return ERROR_CODE_THREAD_CREATE_FAILED;
            }

            g_bb_recover_num=0;
            retval=nand_bd_recover("Root file system");
            if (retval <0) {
                /*Operate nand_bd_recover() error, report to client(PC)*/
                printf("DBG--------->Error occured in nand_bd_recover(), error code:%d !\n",retval);
                command->command_id=COMMAND_OPERATION_ERROR_ACK;
                command->data=retval;
                retval=send_to_client(command);
                return retval;
            }
            /*Wait for report thread exit*/
            pthread_join(thr_sent_inform, NULL);

            /*Recover finished, report it*/
            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            command->command_id = RECOVER_BB_FINISH_ACK;
            command->data = g_bb_recover_num;
            retval=send_to_client(command);
            SERVER_DEBUG("DBG-------> retval=%d\n", retval);
            break;

        case RECOVER_BB_USER_PATITION:
            printf("Execute: RECOVER_BB_USER_PATITION\n");  

            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            command->command_id = RECOVER_BB_USER_PATITION_ACK;
            command->data = 0;
            /*  create a thread, send recover inform to client(PC) periodically */
            retval = pthread_create(&thr_sent_inform, NULL, nand_send_inform_process, (void*)command); 
            if (retval) {
                printf("DBG---------> create thread for nand read test failed! error code: %d\n", ERROR_CODE_THREAD_CREATE_FAILED);
                return ERROR_CODE_THREAD_CREATE_FAILED;
            }

            g_bb_recover_num=0;
            retval=nand_bd_recover("user data");
            if (retval <0) {
                /*Operate nand_bd_recover() error, report to client(PC)*/
                printf("DBG--------->Error occured in nand_bd_recover(), error code:%d !\n",retval);
                command->command_id=COMMAND_OPERATION_ERROR_ACK;
                command->data=retval;
                retval=send_to_client(command);
                return retval;
            }
            /*Wait for report thread exit*/
            pthread_join(thr_sent_inform,NULL);

            /*Recover finished, report it*/
            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            command->command_id = RECOVER_BB_FINISH_ACK;
            command->data = g_bb_recover_num;
            retval=send_to_client(command);
            SERVER_DEBUG("DBG-------> retval=%d\n", retval);
            break;

        case COMMAND_REBOOT:
            printf("Execute: COMMAND_REBOOT\n");  
            memset((unsigned char*)command, 0, sizeof(struct Network_command));
            command->command_id=COMMAND_REBOOT_ACK;
            retval=send_to_client(command);
            printf("DBG-------> retval=%d\n", retval);
            system("reboot");
            break;

        default :
            retval=ERROR_CODE_CMD_ERROR;
            printf("Error: Unknow command id !!\n");  
            break;
    }


END:
    free(command);
    return retval;
    
}

int main(int argc, char *argv[])
{
    int socket_client = -1; 
    int retval = -1; 

    /*Wait until client connected, otherwise BLOCK here*/
    socket_client = network_listen_to(NULL, LISTENING_PORT);
    SERVER_DEBUG("socket_client=%d, %s, +%d\n", socket_client, __FILE__, __LINE__);

    /*clinet has been connected, start communicate routine */
    while(1)
    {
        retval = loop_routine(socket_client);
        sleep(1);
    }

    return 0;
}
