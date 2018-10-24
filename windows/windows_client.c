/*
 * Name:    windows_client.c
 * Purpose: NAND FLASH fake bad blocks recover tool, windows side.
 * ChangeList
 * Date                Author              Purpose
 * 2018.06.13          Jarick              Create
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright: (c) 2018 Unication Co., Ltd.
 */


#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include "network_library.h"
#include "UpgradeLib.h"

#define NET_PORT 0x8989

#define REBOOT_SHELL    "reset_tg.sh"  /*reboot shell, running in linux device, reboot device into upgrade mode*/

#define BB_RECOVER_TOOL      "linux_bb_recover_v0.1"    /*Fake Bad Block recover tool, runing in ARM side*/

static const unsigned char TARGET_IP[]={"10.10.0.12"};
extern int g_socket_to_server;
extern int g_socket_to_client;

#define WINDOWS_DEBUG_ENABLE                                       

#ifdef WINDOWS_DEBUG_ENABLE
    #define WINDOWS_DEBUG(fmt, args...)  printf(fmt, ## args)   
#else
    #define WINDOWS_DEBUG(fmt, args...)                         
#endif


static void print_command_name(enum command_ID cmd_id)
{
    printf("command_ID=%s\n", cmd_id==CHECK_BB_ROOT_PATITION ? 
    "CHECK_BB_ROOT_PATITION": cmd_id==CHECK_BB_USER_PATITON ? 
    "CHECK_BB_USER_PATITON" : cmd_id==CHECK_NETWORK_STATU ? 
    "CHECK_NETWORK_STATU" : cmd_id==RECOVER_BB_ROOT_PATITION ? 
    "RECOVER_BB_ROOT_PATITION": cmd_id==RECOVER_BB_USER_PATITION ? 
    "RECOVER_BB_USER_PATITION" : cmd_id==COMMAND_EXECUTE ? 
    "COMMAND_EXECUTE" : cmd_id==COMMAND_REBOOT ? 
    "COMMAND_REBOOT" :"UNKNOW_COMMAND!");
}


static int reset_tg()
{
    char  j, retval;
    printf( "TRYING TO REBOOT TARGET...\n" );

    /*Initialize net */   
    WSADATA wsaData;
    retval      = WSAStartup(MAKEWORD(1, 1), &wsaData);
    if (retval != 0)
    {
      printf("%s_%d: WSAStartup() error, error code is %d\n", __FILE__, __LINE__, WSAGetLastError());
      goto END;
    }

    WINDOWS_DEBUG("Before download \n");

    /*Try 6 times...*/
    for (j = 0;j < 6;j++)
    {
        retval = file_download(REBOOT_SHELL, "/tmp/reset_tg.sh");
        WINDOWS_DEBUG("Download result = %x\n",retval);
        if( 0 == retval ) 
        {
            printf("Download %s success.\n", REBOOT_SHELL);
            retval = exec_file_in_tg("/tmp/reset_tg.sh");
            if( 0 == retval )
            {
              printf("Exec %s success!\n", REBOOT_SHELL);
              break;
            }
        }
    }

END:
    WSACleanup();
    return  retval;
}

/*Downlaod and exec BB_RECOVER_TOOL in target device*/
static int download_bb_rec_tool()
{
    char  j, retval;
    printf( "Starting Download %s to target device\n", BB_RECOVER_TOOL);

    //Initialize net
    WSADATA wsaData;
    retval      = WSAStartup(MAKEWORD(1, 1), &wsaData);
    if (retval != 0)
    {
      printf("%s_%d: WSAStartup() error, error code is %d\n", 
            __FILE__, __LINE__, WSAGetLastError());
      return -1;
    }
    WINDOWS_DEBUG("Before download \n");

    /*Try 6 times...*/
    for (j = 0;j < 6;j++)
    {
        retval = file_download(BB_RECOVER_TOOL, "/tmp/linux_bb_recover" );
        WINDOWS_DEBUG("Download result = %x\n",retval);
        if( 0 == retval ) 
        {
            Sleep(50);
            printf("Download %s success\n", BB_RECOVER_TOOL);
            retval = exec_file_in_tg("/tmp/linux_bb_recover");
            if( 0 == retval )
            {
              printf("Exec %s success!\n", BB_RECOVER_TOOL);
              break;
            }
        }
    }
END:
    WSACleanup();
    return  retval;
}



int main(int argc, void **argv[])
{
    int retval=-1;
    int quit=0;
    unsigned int bb_num_rootfs=-1;
    unsigned int bb_num_userdata=-1;
    unsigned int recover_bb_num=-1;
    char key;

#if 1
    open_debug_log();
    /*Reboot linux device into upgrade mode */
    retval = reset_tg();
    if(0!=retval) {
        perror("reboot target error, please RETRY AFTER turn off and turn on target.");
        return -1;
    }
    /*It takes time to reboot the linux devices, we wait 10sec here*/
    printf("Rebooting target device, please wait for 10 second.\n");
    Sleep(10000);

    /*Download bad block recover tool into linux target, and exec it*/
    retval = download_bb_rec_tool();
    if(0!=retval) {
        perror("Download exec file error, please RETRY AFTER turn off and turn on target.");
        return -1;
    }
#endif //Download && run bad block recover tool in ARM side.

    /*Wait 2sec, make sure bad block recover tool established host server.*/
    Sleep(2000);
    /*Started communicate with bad block recover tool...*/
    retval=network_try2_connect(TARGET_IP, NET_PORT);
    if(retval<0)
    {
        perror("Connect error!! Please RETRY AFTER turn off and turn on target.\n");
        exit(-1);
    }

    /*Established connection with linux deviec, finally started our recover job...*/
    struct Network_command *command = NULL;
    command= (struct Network_command *)malloc(sizeof(struct Network_command) );
    if(command==NULL)
    {
        perror("Can't allocate memory for struct Network_command!\n");
        exit(-1);
    }
	while (!quit) {
		printf("\n\n");
		printf("**********  Please choose the command  ***********\n");
		printf("* a. CHECK_BB_ROOT_PATITION                      *\n");
		printf("* b. CHECK_BB_USER_PATITON                       *\n");
		printf("* c. CHECK_NETWORK_STATU                         *\n");
		printf("* d. RECOVER_BB_ROOT_PATITION                    *\n");
        printf("* e. RECOVER_BB_USER_PATITION                    *\n");		
		printf("* z. Reboot target & Quit                        *\n");
		printf("**************************************************\n");
		printf("Please input:");
        scanf("%c", &key);
        fflush(stdin);
        printf("Input key:%c\n", key);
		switch(key){
            case 'a':
            case 'A':
                memset(command, 0, sizeof(struct Network_command));
                command->command_id=CHECK_BB_ROOT_PATITION;
                print_command_name(command->command_id);
                retval=windows_send_command(command);
                if(retval<0)
                {
                    printf("Error: windows_send_command(), error code=%d\n", retval);
                    printf("DBG: ------------->=File: %s, fun=%s(), +%d\n", __FILE__, __FUNCTION__, __LINE__);
                    break;
                }                
                /*It takes few sec for Target(U3 Devices) to scann, for ROOT partiton,
                mornally less than 128M, 3sec is enough(at least 2sec).*/
                printf("Plaese wait a sec...\n");
                Sleep(3000);
                memset((unsigned char*)command, 0, sizeof(struct Network_command));
                retval=recv_from_server(command);
                if(retval<0)
                {
                    /*Linux device did not respond(time out), or something wrong with the connection*/
                    printf("Error: recv_from_server(), error code=%d\n", retval);
                    printf("DBG: ------------->=File: %s, fun=%s(), +%d\n", __FILE__, __FUNCTION__, __LINE__);
                    break;
                }                
                
                /*Successful receive the results from linux device*/
                if(command->command_id == CHECK_BB_ROOT_PATITION_ACK && retval >0)
                {
                    bb_num_rootfs=(unsigned int)command->data;
                    printf("DBG: -------->retval=%d, Root file system partition: bb_num_rootfs=%d\n", retval, bb_num_rootfs);
                }
                else if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
                    printf("EXECUTE CHECK_BB_ROOT_PATITION FAILED! ERROR CODE:0x%x\n", command->data);
                else
                    printf("CHECK_BB_ROOT_PATITION FAILED! UNKNOW ERROR!\n");
                    
                break;

            case 'b':
            case 'B':
                memset(command, 0, sizeof(struct Network_command));
                command->command_id=CHECK_BB_USER_PATITON;
                print_command_name(command->command_id);

                retval=windows_send_command(command);
                if(retval<0)
                {
                    printf("Error: windows_send_command(), error code=%d\n", retval);
                    printf("DBG: ------------->=File: %s, fun=%s(), +%d\n", __FILE__, __FUNCTION__, __LINE__);
                    break;
                }                
                /*It takes few sec for Target(U3 Devices) to scann, for USER partiton,
                maximum space is 948M(U3 7/800), 8 sec is enough(at least 6sec).*/
                printf("Plaese wait a sec...\n");
                Sleep(8000);
                memset((unsigned char*)command, 0, sizeof(struct Network_command));
                retval=recv_from_server(command);
                if(retval<0)
                {
                    printf("error: recv_from_server(), error code=%d\n", retval);
                    printf("DBG: ------------->=file: %s, fun=%s(), +%d\n", __FILE__, __FUNCTION__, __LINE__);
                    break;
                }                
                if(command->command_id == CHECK_BB_USER_PATITON_ACK && retval >0)
                {
                    bb_num_userdata=(unsigned int)command->data;
                    printf("DBG: -------->retval=%d, user data partiton: bb_num_userdata=%d\n", retval, bb_num_userdata);
                }
                else if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
                    printf("EXECUTE CHECK_BB_USER_PATITON FAILED! ERROR CODE:0x%x\n", command->data);
                else
                    printf("CHECK_BB_USER_PATITON FAILED UNKNOW ERROR!\n");
                break;

            case 'c':
            case 'C':
                memset(command, 0, sizeof(struct Network_command));
                command->command_id=CHECK_NETWORK_STATU;
                print_command_name(command->command_id);
                retval=windows_send_command(command);
                if(retval<0)
                {
                    printf("Error: windows_send_command(), error code=%d\n", retval);
                    printf("DBG: ------------->=File: %s, fun=%s(), +%d\n", 
                        __FILE__, __FUNCTION__, __LINE__);
                }                
                memset((unsigned char*)command, 0, sizeof(struct Network_command));
                retval=recv_from_server(command);
                if(retval<0)
                {
                    printf("error: recv_from_server(), error code=%d\n", retval);
                    printf("DBG: ------------->=File: %s, fun=%s(), +%d\n", 
                        __FILE__, __FUNCTION__, __LINE__);
                }                
                if(command->command_id == CHECK_NETWORK_STATU_ACK && retval >0)
                {
                    printf("DBG: -------->retval=%d, Network connection is good!\n", retval);
                }
                else
                    printf("Connection error! You may need to reboot your linux device and try it again.\n");
                break;

            case 'd':
            case 'D':
                memset(command, 0, sizeof(struct Network_command));
                command->command_id=RECOVER_BB_ROOT_PATITION;
                print_command_name(command->command_id);
                /*Must get badblock number(bb_num_rootfs) fist, in order to calculate recovering persentage*/
                if(bb_num_rootfs==-1)
                {
                    printf("Please run command CHECK_BB_ROOT_PATITION to get bad block(s) first.\n");
                    break;
                }
                if(bb_num_rootfs==0)
                {
                    printf("There's no bad block in rootfs partition, no need to recover!.\n");
                    break;
                }

                if(bb_num_rootfs<=20)/* 128k/block, 30*128k=2.5MB, for rootfs partition, 64MB in total*/
                {
                    printf("There's only %d bad block(s) in rootfs partition, in case of security it's NOT necessary to recover it.\n", 
                            bb_num_rootfs);
                    break;
                }
            
                /*Start recover process, command->command_id=RECOVER_BB_ROOT_PATITION*/
                retval=windows_send_command(command);
                if(retval<0)
                {
                    printf("Error: windows_send_command(), error code=%d\n", retval);
                    break;
                }                

                do /*Polling every 5sec, to get current inform.*/
                {
                    Sleep(5000); 
                    retval=recv_from_server(command);
                    recover_bb_num=(unsigned int)command->data;
                    printf("DBG: --------> recover_bb_num=%d, processing: %2.2f%%\n", 
                        recover_bb_num, ( (float)recover_bb_num / (float)bb_num_rootfs ) *100);

                }while(command->command_id != RECOVER_BB_FINISH_ACK && retval > 0);

                /*Recover process terminated, either recover finished or error occur, get result here*/
                if(command->command_id == RECOVER_BB_FINISH_ACK && retval >0)
                {
                    /*All the Bad Blocks are recovered*/
                    if(recover_bb_num%bb_num_rootfs==0)
                        printf("RECOVER_BB_ROOT_PATITION SUCCEED\n");
                    else /*Leave some TURE Bad Block(s), that is not recoverable*/
                        printf("RECOVER_BB_ROOT_PATITION SUCCEED, remain %d TURE Bad Block(s)\n", 
                            bb_num_rootfs -recover_bb_num);
                }
                else if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
                    printf("EXECUTE RECOVER_BB_ROOT_PATITION FAILED! ERROR CODE:0x%x\n", command->data);
                else
                    printf("RECOVER_BB_ROOT_PATITION FAILL!!!\n");

                break;

            case 'e':
            case 'E':
                memset(command, 0, sizeof(struct Network_command));
                command->command_id=RECOVER_BB_USER_PATITION;
                print_command_name(command->command_id);
                /*Must get badblock number(bb_num_userdata) fist, in order to calculate recovering persentage*/
                if(bb_num_userdata==-1)
                {
                    printf("Please run command CHECK_BB_USER_PATITON to get bad block(s) first.\n");
                    break;
                }

                if(bb_num_userdata==0)
                {
                    printf("There's no bad block in user data partition, no need to recover!\n");
                    break;
                }

                /* 128k/block, 30*128k=3.7MB, for user partition, U3 device is 52MB in total,
                   U3 7/800 devices is 948MB, for U3 7/800 devices it's suggested that better NOT to recover it 
                   when the bad block numbers are less than 100(100*128k=12.5MB)*/
                if(bb_num_userdata<=30) 
                {
                    printf("There's only %d bad block(s) in user data partition, in case of security it's NOT necessary to recover it.\n", bb_num_userdata);
                    break;
                }

                /*Start recover process, command->command_id=RECOVER_BB_USER_PATITION*/
                retval=windows_send_command(command);
                if(retval<0)
                {
                    printf("Error: windows_send_command(), error code=%d\n", retval);
                    break;
                }                

                do /*Polling every 5sec, to get current inform.*/
                {
                    Sleep(5000); 
                    retval=recv_from_server(command);
                    recover_bb_num=(unsigned int)command->data;
                    printf("DBG: --------> recover_bb_num=%d, processing: %2.2f%%\n", 
                        recover_bb_num, ( (float)recover_bb_num / (float)bb_num_userdata ) *100);

                }while(command->command_id != RECOVER_BB_FINISH_ACK && retval > 0);

                /*Recover process terminated, get result*/
                if(command->command_id == RECOVER_BB_FINISH_ACK && retval >0)
                {
                    /*All the Bad Blocks are recovered*/
                    if(recover_bb_num%bb_num_userdata==0)
                        printf("RECOVER_BB_ROOT_PATITION SUCCEED\n");
                    else /*Leave some TURE Bad Block(s), that is not recoverable*/
                        printf("RECOVER_BB_ROOT_PATITION SUCCEED, remain %d TURE Bad Block(s)\n", 
                            bb_num_userdata - recover_bb_num);
                }
                else if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
                    printf("EXECUTE RECOVER_BB_USER_PATITION FAILED! ERROR CODE:0x%x\n", command->data);
                else
                    printf("RECOVER_BB_USER_PATITION FAILL!!!\n");
    
                break;

			case 'z':
			case 'Z': 
                memset(command, 0, sizeof(struct Network_command));
                command->command_id=COMMAND_REBOOT;
                print_command_name(command->command_id);
                retval=windows_send_command(command);
                if(retval<0)
                {
                    printf("Error: windows_send_command(), error code=%d\n", retval);
                    printf("DBG: ------------->=File: %s, fun=%s(), +%d\n", 
                        __FILE__, __FUNCTION__, __LINE__);
                }                
                else
                    printf("Reboot target, exit..\n");
                Sleep(3000);
				quit = 1;
				break;

		     default:
		        break;

        }
        if(retval<0)
            printf("Error: windows_send_command(), error code=%d\n", retval);
    }

    //Close socket
    closesocket(g_socket_to_server);  
    //terminated DLL  
    WSACleanup();  
    printf("Exit main()!\n");
    return 0;
}







