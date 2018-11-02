#include <windows.h>
#include <stdlib.h>
#include <shlwapi.h>
#include "NandRecoveryTool.h"
#include "error_code.h"

#include <stdio.h>
#include <winsock2.h>
#include "network_library.h"
#include "UpgradeLib.h"

#define NET_PORT 0x8989

#define REBOOT_SHELL    "reset_tg.sh"  /*reboot shell, running in linux device, reboot device into upgrade mode*/

#define BB_RECOVER_TOOL      "linux_bb_recover_v0.1"    /*Fake Bad Block recover tool, runing in ARM side*/


#define WINDOWS_DEBUG_ENABLE                                       

#ifdef WINDOWS_DEBUG_ENABLE
    #define WINDOWS_DEBUG(fmt, args...)  printf(fmt, ## args)   
#else
    #define WINDOWS_DEBUG(fmt, args...)                         
#endif



#define APP_TITLE   "Unication Nandflash Scan&Recovery Tool"
#define MUTEX_NAME	"Unication Dev Tools"

/* GUI layout */
#define DEV_TOOLS_WIDTH         700         
#define DEV_TOOLS_HEIGHT        500 

#define CONTROLLER_NUM			7
#define MAX_STRING				2048

/*USER message definition */
#define WM_INIT			(WM_USER)
#define WM_CONNECT		(WM_USER+1)
#define WM_CTLDISABLE	(WM_USER+2)
#define WM_CTLENABLE	(WM_USER+3)

/*use hInfo to append information in lpszInformation,use it carefully when hInfo have been initialized */
#define AppendInfo(fmt,args...)	({snprintf(lpszInformation+strlen(lpszInformation),MAX_STRING-strlen(lpszInformation),fmt,##args);\
SetWindowText(hInfo,lpszInformation);})
#define ClearInfo	lpszInformation[0]=0

/* controller layout definition */
static LAYOUT controllerLayout[CONTROLLER_NUM] = {
	{NULL,TEXT("button"),"Partition Select:",\
	WS_CHILD | WS_VISIBLE | BS_GROUPBOX,  10, 10,675, 150},\
	{NULL,TEXT("button"),"rootfs",\
	WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX , 50, 70,100, 40,},\
	{NULL,TEXT("button"),"userdata",\
	WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX ,  250, 70,100, 40},\
	{NULL,TEXT("button"),"Partition Operation:",\
	WS_CHILD | WS_VISIBLE | BS_GROUPBOX,  10, 180,675, 300},\
	{NULL,TEXT("button"),"Scan",\
	WS_CHILD | WS_VISIBLE ,  100, 420,90, 30,},\
	{NULL,TEXT("button"),"Recover",\
	WS_CHILD | WS_VISIBLE ,  450, 420,90, 30},\
	{NULL,TEXT("static"),APP_TITLE,\
	WS_CHILD | WS_VISIBLE ,  20, 210,655, 200}
};

static TCHAR lpszInformation[MAX_STRING] = APP_TITLE; /*for information */      
static HINSTANCE hInst; /*NandRecoveryTool's instance */
static HWND hwndMain=NULL,hInfo=NULL; /*handle for Main window and information window */
static HANDLE hMutex; /* handle for mutex */
static char *szAppName = APP_TITLE;	/*APP title */
static int stage = S_INIT;	/*NandRecoveryTool's current stat */

static const unsigned char TARGET_IP[]={"10.10.0.12"}; /*target device's ip address */
extern int g_socket_to_server;
extern int g_socket_to_client;
struct Network_command *command = NULL;
static unsigned int bb_num_rootfs=-1; /*rootfs's bad block number */
static unsigned int bb_num_userdata=-1; /*userdata's bad block number */
static unsigned int recover_bb_num=-1; /*bad block number that currently recovered */

static int rootfs_checked=0;	/*stand for rootfs option is checked? */
static int userdata_checked=0;	/*stand for userdata option is checked? */
static int rootfs_recovered=0;	/*stand for rootfs option is recovered? */
static int userdata_recovered=0;	/*stand for userdata option is recovered? */

/*errcode map, use for translate error code to error information */
const char *errcode_map[]={
	"ERROR_CODE_OPEN_FILE_ERROR",	//-1000        
    "ERROR_CODE_FREAD_FILE_ERROR",               
    "ERROR_CODE_FWRITE_FILE_ERROR",    //-998          
    "ERROR_CODE_MALLOC_ERROR",                   
    "ERROR_CODE_THREAD_CREATE_FAILED",      //-996
    "ERROR_CODE_CMD_ERROR",                      
    "ERROR_CODE_UNKNOW_CMD_ACK",                 //-994 
    "ERROR_CODE_CMD_ACK_NOT_MATCH",              
    "ERROR_CODE_FUNCTION_NOT_REALIZE",             
    "ERROR_CODE_NETWORK_CONNECT_ERROR",          
    "ERROR_CODE_BUFFER_SIZE_NOT_ENOUGH",              
    "ERROR_CODE_BLOCK_ALIGN_ERROR",                   
    "ERROR_CODE_PAGE_ALIGN_ERROR",                    
    "ERROR_CODE_SIZE_OUT_OF_RANGE",                   
    "ERROR_CODE_IOCTL_MEMGETBADBLOCK_ERROR",           
    "ERROR_CODE_IOCTL_MEMERASE_ERROR",                 
    "ERROR_CODE_IOCTL_MEMWRITEOOB_ERROR",              
    "ERROR_CODE_IOCTL_MEMREADOOB_ERROR",               
    "ERROR_CODE_IOCTL_MEMGETINFO_ERROR",               
    "ERROR_CODE_IOCTL_MTDFILEMODE_ERROR",              
    "ERROR_CODE_PWRITE_ERROR",                         
    "ERROR_CODE_PREAD_ERROR",                          
    "ERROR_CODE_NO_SUCH_PARTITION",                      
    "ERROR_CODE_OPEN_MTD_ERROR",                      
    "ERROR_CODE_INIT_LIBMTD_ERROR",                      


    "ERROR_CODE_RETRY_AGAIN",// = -40,                      

    "ERROR_CODE_WSASTARTUP",// = -39,                      
    "ERROR_CODE_SOCKET",                                  
    "ERROR_CODE_SETSOCKOPT",                                 
    "ERROR_CODE_BIND",                                         
    "ERROR_CODE_LISTEN",                                
    "ERROR_CODE_ACCEPT",                                
    "ERROR_CODE_SELECT",                                
    "ERROR_CODE_INVALID_SOCKET",                        
    "ERROR_CODE_RECV_ERROR",                            
    "ERROR_CODE_RECV_TIMEOUT",                          
    "ERROR_CODE_SOCKET_CLOSE",                          
    "ERROR_CODE_SEND_ERROR",                            

    "ERROR_USB_WRITE_TIMEOUT",// = -19,                                                       
    "ERROR_USB_RETRY_WEAR_OUT",                                                              
    "ERROR_USB_INTERRUPT_BY_CMD",                                                          
    "ERROR_USB_CABLE_UNPLUG",     
};
static const char *unkown_error = "Unkown Error"; /*illegal errcode information */
/*translate errcode to error information */
static inline const char *errcode2_string(int errcode){
	if(errcode < ERROR_CODE_OPEN_FILE_ERROR)
		return unkown_error;
	else if(errcode <= ERROR_CODE_INIT_LIBMTD_ERROR){
		return errcode_map[errcode - ERROR_CODE_OPEN_FILE_ERROR];
	}
	else if(errcode < ERROR_CODE_RETRY_AGAIN){
		return unkown_error;
	}
	else if(errcode <= ERROR_CODE_SEND_ERROR){
		return errcode_map[errcode - ERROR_CODE_RETRY_AGAIN + ERROR_CODE_INIT_LIBMTD_ERROR - ERROR_CODE_OPEN_FILE_ERROR + 1];
	}
	else if(errcode < ERROR_USB_WRITE_TIMEOUT){
		return unkown_error;
	}
	else if(errcode <= ERROR_USB_CABLE_UNPLUG){
		return errcode_map[errcode - ERROR_USB_WRITE_TIMEOUT + ERROR_CODE_SEND_ERROR - ERROR_CODE_RETRY_AGAIN + ERROR_CODE_INIT_LIBMTD_ERROR - ERROR_CODE_OPEN_FILE_ERROR + 2];
	}
	else
		return unkown_error;
}
/*reset target*/
static int reset_tg()
{
    char  j, retval;
    printf( "TRYING TO REBOOT TARGET...\n" );

    /*Initialize net */   
    WSADATA wsaData;
    retval = WSAStartup(MAKEWORD(1, 1), &wsaData);
    if (retval != 0)
    {
      printf("%s_%d: WSAStartup() error, error code is %d\n", __FILE__, __LINE__, WSAGetLastError());
      goto END;
    }

    WINDOWS_DEBUG("Before download \n");

    /*Try 3 times...*/
    for (j = 0;j < 3;j++)
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

    /*Try 3 times...*/
    for (j = 0;j < 3;j++)
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
/*check the files we need if it's exist,if it's not ,quit immediately*/
static inline void CheckUnitils(void)
{	
	if(!PathFileExists(REBOOT_SHELL)){
		MessageBox(NULL,REBOOT_SHELL" lost!","Error",MB_ICONERROR);
		exit(-1);
	}
	if(!PathFileExists(BB_RECOVER_TOOL)){
		MessageBox(NULL,BB_RECOVER_TOOL" lost!","Error",MB_ICONERROR);
		exit(-1);
	}		
}
//check u3/u3_2nd device online status
static int IsDeviceOffLine(void)
{	
	struct Network_command check_command;
	memset(&check_command, 0, sizeof(struct Network_command));
	check_command.command_id=CHECK_NETWORK_STATU;	
	if(windows_send_command(&check_command)<0)
		return 1;
	memset(&check_command, 0, sizeof(struct Network_command));	          
	if(recv_from_server(&check_command) > 0 && check_command.command_id == CHECK_NETWORK_STATU_ACK)
		return 0;
	return 1;
}
//try to connect u3/u3_2nd device
DWORD WINAPI ConnectDevice(LPVOID lpParameter)
{
	int retval=0;
	stage = S_CONNECTING;
	ClearInfo;
	AppendInfo("Try to reboot device..\n");
	retval = reset_tg();
	if(retval != 0){
		AppendInfo("Reboot device failed!\nPlease check your device if it's connect correctly!\n");
		stage = S_INIT;
	}
	else{
		
		Sleep(10000);
		retval = download_bb_rec_tool();
		AppendInfo("Downloading bad block recover tool..\n");
		if(0 != retval){
			AppendInfo("Download_bb_rec_tool failed!\n");
			stage = S_INIT;
		}
		else {
			Sleep(2000);
			retval=network_try2_connect(TARGET_IP, NET_PORT);
			if(retval < 0){
				AppendInfo("Connect error!! Please RETRY after turn off and turn on target.\n");
				stage = S_INIT;
			}
			else if(stage == S_CONNECTING){
				AppendInfo("Connect success!\n");
				PostMessage(hwndMain,WM_CTLENABLE,0,0);
				stage = S_CONNECTED;
			}
			else
				AppendInfo("Connect error!! Please RETRY after turn off and turn on target.\n");
		}
	}	
	return (DWORD)retval;
}
//scan device partition
DWORD WINAPI ScanDevice(LPVOID lpParameter)
{
	int retval = 0;
	if(stage != S_CONNECTED){
		MessageBox(hwndMain,"Stage error!","error",MB_ICONERROR);
		exit(-1);
	}	
	PostMessage(hwndMain,WM_CTLDISABLE,0,0);
	stage = S_SCANING;
	/*do the scan job */
	ClearInfo;
	if(rootfs_checked != 1 && userdata_checked !=1){
		AppendInfo("No partition selected.\n");
		goto Exit;
	}
	memset(command, 0, sizeof(struct Network_command));
	if(rootfs_checked == 1){		
		AppendInfo("Scaning rootfs partition..\n");
		command->command_id = CHECK_BB_ROOT_PATITION;
		retval=windows_send_command(command);
		if(retval < 0){
			AppendInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));			
			goto Exit;
		}
		Sleep(3000);
		memset((unsigned char*)command, 0, sizeof(struct Network_command));
        retval=recv_from_server(command);
		if(retval<0){
			/*Linux device did not respond(time out), or something wrong with the connection*/
            AppendInfo("Error: recv_from_server().\nError code=%s.\n", errcode2_string(retval));			
			goto Exit;
		}
		if(command->command_id == CHECK_BB_ROOT_PATITION_ACK && retval >0){
			bb_num_rootfs = command->data;
            AppendInfo("Rootfs bad block number:%u%s\n", bb_num_rootfs,rootfs_recovered?"(need reboot to update)":"");
		}
		else{
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE CHECK_BB_ROOT_PATITION FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("CHECK_BB_ROOT_PATITION FAILED! UNKNOW ERROR!\n");        
			goto Exit;
		}
	}
	if(userdata_checked == 1){
		AppendInfo("Scaning userdata partition..\n");
		command->command_id = CHECK_BB_USER_PATITON;
		retval=windows_send_command(command);
		if(retval < 0){
			AppendInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));
			goto Exit;
		}
		Sleep(3000);
		memset((unsigned char*)command, 0, sizeof(struct Network_command));
        retval=recv_from_server(command);
		if(retval<0){
			/*Linux device did not respond(time out), or something wrong with the connection*/
            AppendInfo("Error: recv_from_server().\nError code=%s.\n", errcode2_string(retval));			
			goto Exit;
		}
		if(command->command_id == CHECK_BB_USER_PATITON_ACK && retval >0){
			bb_num_userdata = command->data;
            AppendInfo("Userdata bad block number:%u%s\n", bb_num_userdata,userdata_recovered?"(need reboot to update)":"");
		}
		else {
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE CHECK_BB_USER_PATITON FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("CHECK_BB_USER_PATITON FAILED! UNKNOW ERROR!\n");        
			goto Exit;
		}
	}		
Exit:
	if(stage == S_SCANING){
		stage = S_CONNECTED;
		PostMessage(hwndMain,WM_CTLENABLE,0,0);
	}
	return (DWORD)retval;
}
//recover device partition
DWORD WINAPI RecoverDevice(LPVOID lpParameter)
{
	int pos,retval = 0;
	if(stage != S_CONNECTED){
		MessageBox(hwndMain,"Stage error!","error",MB_ICONERROR);
		exit(-1);
	}
	PostMessage(hwndMain,WM_CTLDISABLE,0,0);
	stage = S_RECOVERING;
	/*do the recover job */
	ClearInfo;
	if(rootfs_checked != 1 && userdata_checked !=1){
		AppendInfo("No partition selected.\n");
		goto Exit;
	}
	/*this macro used just for here,do not use it in other place
	*it do that if it will do actual recover job,then you will get 1(TRUE),
	*otherwise,you will get 0(FALSE) */
#if 0
 #define NEED_WARNING	((rootfs_checked == 1 && bb_num_rootfs != -1 && bb_num_rootfs > 20)\
 || (userdata_checked == 1 && bb_num_userdata != -1 && bb_num_userdata > 30))
	if(NEED_WARNING && IDOK != MessageBox(hwndMain,"Warning:This operation maybe will lost your partition data.Are you sure to do this?","Warning",MB_OKCANCEL | MB_ICONWARNING))
		goto Exit;
#undef NEED_WARNING
#endif
	memset(command, 0, sizeof(struct Network_command));	
rootfs:
	if(rootfs_checked == 1){
		AppendInfo("Recovering rootfs partition..\n");
		if(bb_num_rootfs==-1){
			AppendInfo("Please check the rootfs option and push scan button to get rootfs bad block(s) first.\n");
			goto Exit;
		}
		else if(bb_num_rootfs <= 20){
			if(bb_num_rootfs == 0)
				AppendInfo("There's no bad block in rootfs partition.\n");
			else
				AppendInfo("There's only %u bad block(s) in rootfs partition, it's NOT necessary to recover it.\n", 
                            bb_num_rootfs);			
			goto userdata;
		}		
		command->command_id = RECOVER_BB_ROOT_PATITION;		
		retval=windows_send_command(command);
		if(retval < 0){
			AppendInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));
			goto Exit;
		}
		/*save the position of information */
		pos = strlen(lpszInformation);
		do /*Polling every 5sec, to get current inform.*/
        {
            Sleep(5000); 
            retval=recv_from_server(command);
            recover_bb_num=(unsigned int)command->data;
			/*truncate the processing information */
			lpszInformation[pos] = 0;
            AppendInfo("Recovered rootfs bad block:%u, processing: %2.2f%%\n", 
            recover_bb_num, ( (float)recover_bb_num / (float)bb_num_rootfs ) *100);
			
		}while(command->command_id != RECOVER_BB_FINISH_ACK && retval > 0);
		if(command->command_id == RECOVER_BB_FINISH_ACK && retval >0)
        {
            /*All the Bad Blocks are recovered*/
            if(recover_bb_num%bb_num_rootfs==0)
                AppendInfo("Recover rootfs partition success!\n");
            else /*Leave some TURE Bad Block(s), that is not recoverable*/
                AppendInfo("Recover rootfs partition success, remain %d TURE Bad Block(s).\n", 
                    bb_num_rootfs -recover_bb_num);
			rootfs_recovered = 1;
        }
        else {
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE RECOVER_BB_ROOT_PATITION FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("RECOVER_BB_ROOT_PATITION FAILL!!!\n");			
			goto Exit;
		}		
	}
userdata:
	if(userdata_checked == 1){
		AppendInfo("Recovering userdata partition..\n");
		if(bb_num_userdata==-1){
			AppendInfo("Please check the userdata option and push scan button to get userdata bad block(s) first.\n");
			goto Exit;
		}		
		else if(bb_num_userdata <= 30){
			if(bb_num_userdata == 0)
				AppendInfo("There's no bad block in userdata partition.\n");
			else
				AppendInfo("There's only %d bad block(s) in userdata partition, it's NOT necessary to recover it.\n", 
				bb_num_userdata);		
			goto Exit;
		}		
		command->command_id = RECOVER_BB_USER_PATITION;
		retval=windows_send_command(command);
		if(retval < 0){
			AppendInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));
			goto Exit;
		}
		/*save the position of information */
		pos = strlen(lpszInformation);
		do /*Polling every 5sec, to get current inform.*/
        {
            Sleep(5000); 
            retval=recv_from_server(command);
            recover_bb_num=(unsigned int)command->data;
			/*truncate the processing information */
			lpszInformation[pos] = 0;
            AppendInfo("Recovered userdata bad blocks:%u, processing: %2.2f%%\n", 
                    recover_bb_num, ( (float)recover_bb_num / (float)bb_num_userdata ) *100);
			
		}while(command->command_id != RECOVER_BB_FINISH_ACK && retval > 0);

        /*Recover process terminated, get result*/
        if(command->command_id == RECOVER_BB_FINISH_ACK && retval >0)
        {
            /*All the Bad Blocks are recovered*/
            if(recover_bb_num%bb_num_userdata==0)
                AppendInfo("Recover userdata partition success!\n");
            else /*Leave some TURE Bad Block(s), that is not recoverable*/
                AppendInfo("Recover userdata partition success, remain %d TURE Bad Block(s).\n", 
                        bb_num_userdata - recover_bb_num);
			userdata_recovered = 1;
        }
        else{ 
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE RECOVER_BB_USER_PATITION FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("RECOVER_BB_USER_PATITION FAILL!!!\n");
			goto Exit;
		}       
		
	}		
		
Exit:
	if(stage == S_RECOVERING){
		stage = S_CONNECTED;
		PostMessage(hwndMain,WM_CTLENABLE,0,0);
	}
	return (DWORD)retval;
}

//callback function for main window
LRESULT CALLBACK WndProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;	
	switch(message){
		case WM_CREATE:
		/*create the controller */
		{
			int i;
			for(i = 0; i < CONTROLLER_NUM; ++i){
				controllerLayout[i].hwnd = CreateWindow (controllerLayout[i].lpszType,controllerLayout[i].lpszString,
                         controllerLayout[i].dwStyle,controllerLayout[i].x,controllerLayout[i].y,
						 controllerLayout[i].nWidth,controllerLayout[i].nHeight,
                         hwnd, (HMENU)i,
                         hInst, NULL);
				if( NULL == controllerLayout[i].hwnd){
					MessageBox(NULL,"Controller create error","Error",MB_ICONERROR);
					exit(-1);
				}
			}
			hInfo = controllerLayout[6].hwnd;
		}
		command= (struct Network_command *)malloc(sizeof(struct Network_command));
		if(command == NULL){
			MessageBox(NULL,"Memery alloc failed!","Error",MB_ICONERROR);
			exit(-1);
		}
		break;
		case WM_COMMAND:
		{
			WORD childId = LOWORD(wParam);
			WORD notify = HIWORD(wParam);
			if(notify == BN_CLICKED){				
				switch(childId){
					//rootfs checked
					case 1:rootfs_checked = rootfs_checked == 1 ? 0 : 1;
					break;
					//userdata checked
					case 2:userdata_checked = userdata_checked == 1 ? 0 : 1;
					break;
					//scan button click
					case 4:				
					CreateThread(NULL,0,ScanDevice,NULL,0,NULL);					
					break;
					//recover button click
					case 5:			
					CreateThread(NULL,0,RecoverDevice,NULL,0,NULL);	
					break;
					default:
					MessageBox(NULL,"Controller message error","Error",MB_ICONERROR);
					break;
				}
			}							
		}				
		break;
		case WM_CLOSE:
		/*give notice to user that will reboot the device*/
		if(S_CONNECTED == stage){
			MessageBox(hwndMain,"Need reboot the device,click to reboot the device.","Notice",MB_OK | MB_ICONEXCLAMATION);
			memset(command, 0, sizeof(struct Network_command));
			command->command_id=COMMAND_REBOOT;			
			windows_send_command(command);
		}
		else if(S_SCANING == stage || S_RECOVERING == stage){
			MessageBox(hwndMain,"Can't quit now!","Warning",MB_OK | MB_ICONWARNING);
			return 0;
		}
		break;
		
		case WM_DESTROY:
		free(command);
		//Close socket
		closesocket(g_socket_to_server);  
		//terminated DLL  
		WSACleanup();
		CloseHandle(hMutex);
		PostQuitMessage(0);
		break;
		case WM_PAINT:
		hdc = BeginPaint(hwnd,&ps);		
		EndPaint(hwnd,&ps);
		break;
		
		case WM_INIT:		
		SetWindowText(hInfo,"Connecting..");
		PostMessage(hwndMain,WM_CTLDISABLE,0,0);
		PostMessage(hwndMain,WM_CONNECT,0,0);		
		break;
		
		case WM_CONNECT:/*try to connect device */		
		CreateThread(NULL,0,ConnectDevice,NULL,0,NULL);		
		break;
		
		/*disable user input controller */
		case WM_CTLDISABLE:
		EnableWindow(controllerLayout[1].hwnd,0);
		EnableWindow(controllerLayout[2].hwnd,0);
		EnableWindow(controllerLayout[4].hwnd,0);
		EnableWindow(controllerLayout[5].hwnd,0);
		break;
		
		/*enable user input controller */
		case WM_CTLENABLE:
		EnableWindow(controllerLayout[1].hwnd,1);
		EnableWindow(controllerLayout[2].hwnd,1);
		EnableWindow(controllerLayout[4].hwnd,1);
		EnableWindow(controllerLayout[5].hwnd,1);
		break;
		
		case WM_DEVICECHANGE:
		if(stage == S_INIT){
			/*if device changed and not connected,then try to connect */
			SendMessage(hwndMain,WM_CONNECT,0,0);
		}
		else if(stage != S_CONNECTING && IsDeviceOffLine()){
			/*if our device removed ,init environment and disable user input controller */
			SetWindowText(hInfo,"Device removed!\nPlease Reconnect your device!");
			stage = S_INIT;
			bb_num_rootfs=-1;
			bb_num_userdata=-1;
			recover_bb_num=-1;
			rootfs_recovered=0;
			userdata_recovered=0;
			PostMessage(hwndMain,WM_CTLDISABLE,0,0);
		}		
		break;
		
		default:
		break;
	}
	return DefWindowProc(hwnd,message,wParam,lParam);
}


//main window entry
int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,
            LPSTR lpCmdLine,int iCmdShow)
{
	MSG msg;
	int bRet;
	int screenWidth, screenHeight;	
	static WNDCLASSEX wndclass;
	/*make sure only one instance run at the same time*/
	hMutex = CreateMutex(NULL,FALSE,MUTEX_NAME);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(hMutex);
		MessageBox(NULL,"Already running!","Notice",MB_OK);
		return FALSE;
	}

	/*check the files that we need is available? */
	CheckUnitils();
	
	hInst = hInstance;
	screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
	
    memset(&wndclass, 0, sizeof(WNDCLASSEX));
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance     = hInstance;
    wndclass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    wndclass.hCursor       = LoadCursor(NULL, IDC_HAND);
    wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wndclass.lpszMenuName  = NULL;
    wndclass.lpszClassName = szAppName;
    wndclass.cbSize = sizeof(WNDCLASSEX);
    if(!RegisterClassEx(&wndclass))
      return -1;
    hwndMain = CreateWindowEx(0, szAppName, TEXT(szAppName),
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPSIBLINGS,
                         (screenWidth-DEV_TOOLS_WIDTH)/2, (screenHeight-DEV_TOOLS_HEIGHT)/2,
                         DEV_TOOLS_WIDTH, DEV_TOOLS_HEIGHT + 35,
                         NULL, NULL, hInst, NULL);
	if(0 == hwndMain){
		 MessageBox(NULL, TEXT ("Can not create main window"), szAppName, MB_ICONERROR);
		 return -1;
	}	
						 
	ShowWindow(hwndMain,iCmdShow);
	UpdateWindow(hwndMain);
	
	/*Post init message*/
	PostMessage(hwndMain,WM_INIT,0,0);		
	
	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    { 
        if (bRet == -1)
        {            
            MessageBox(NULL, TEXT("GetMessage error"), szAppName, MB_ICONERROR);
            return -1;
        }
        else
        {
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        }
    }
    return msg.wParam;
}




