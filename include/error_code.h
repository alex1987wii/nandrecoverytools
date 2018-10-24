/*
 *  Copyright: (c) 2013 Unication Co., Ltd.
 *  All rights reserved.
 *
 *  Name: error_code.h
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


#ifndef _ERROR_CODE_H_
#define _ERROR_CODE_H_

#ifdef __cplusplus
extern "C"
{

#endif //__cplusplus

#define ERROR_CODE_TRANSFER(ec)             (0x10000 - ((unsigned short)ec))

#ifdef ENABLE_DEBUG
#define PRINTF_ERROR_CODE(from, to)    do {int i; \
for (i=from; i<=to; i++) printf("%/%/ %04X\n", (unsigned short)i);} while (0)
#endif

enum _ERROR_CODE
{
    ERROR_CODE_OPEN_FILE_ERROR = -1000,        
    ERROR_CODE_FREAD_FILE_ERROR,               
    ERROR_CODE_FWRITE_FILE_ERROR,    //-998          
    ERROR_CODE_MALLOC_ERROR,                   
    ERROR_CODE_THREAD_CREATE_FAILED,      //-996
    ERROR_CODE_CMD_ERROR,                      
    ERROR_CODE_UNKNOW_CMD_ACK,                 //-994 
    ERROR_CODE_CMD_ACK_NOT_MATCH,              
    ERROR_CODE_FUNCTION_NOT_REALIZE,             
    ERROR_CODE_NETWORK_CONNECT_ERROR,          
    ERROR_CODE_BUFFER_SIZE_NOT_ENOUGH,              
    ERROR_CODE_BLOCK_ALIGN_ERROR,                   
    ERROR_CODE_PAGE_ALIGN_ERROR,                    
    ERROR_CODE_SIZE_OUT_OF_RANGE,                   
    ERROR_CODE_IOCTL_MEMGETBADBLOCK_ERROR,           
    ERROR_CODE_IOCTL_MEMERASE_ERROR,                 
    ERROR_CODE_IOCTL_MEMWRITEOOB_ERROR,              
    ERROR_CODE_IOCTL_MEMREADOOB_ERROR,               
    ERROR_CODE_IOCTL_MEMGETINFO_ERROR,               
    ERROR_CODE_IOCTL_MTDFILEMODE_ERROR,              
    ERROR_CODE_PWRITE_ERROR,                         
    ERROR_CODE_PREAD_ERROR,                          
    ERROR_CODE_NO_SUCH_PARTITION,                      
    ERROR_CODE_OPEN_MTD_ERROR,                      
    ERROR_CODE_INIT_LIBMTD_ERROR,                      


    ERROR_CODE_RETRY_AGAIN = -40,                      

    ERROR_CODE_WSASTARTUP = -39,                      
    ERROR_CODE_SOCKET,                                  
    ERROR_CODE_SETSOCKOPT,                                 
    ERROR_CODE_BIND,                                         
    ERROR_CODE_LISTEN,                                
    ERROR_CODE_ACCEPT,                                
    ERROR_CODE_SELECT,                                
    ERROR_CODE_INVALID_SOCKET,                        
    ERROR_CODE_RECV_ERROR,                            
    ERROR_CODE_RECV_TIMEOUT,                          
    ERROR_CODE_SOCKET_CLOSE,                          
    ERROR_CODE_SEND_ERROR,                            

    ERROR_USB_WRITE_TIMEOUT = -19,                                                       
    ERROR_USB_RETRY_WEAR_OUT,                                                              
    ERROR_USB_INTERRUPT_BY_CMD,                                                          
    ERROR_USB_CABLE_UNPLUG,                                                             
};

#ifdef __cplusplus
}

#endif //__cplusplus

#endif //_ERROR_CODE_H_


