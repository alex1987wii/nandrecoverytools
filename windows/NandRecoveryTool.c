#include <windows.h>
#include <stdlib.h>
#include <shlwapi.h>
#include <assert.h>
#include <winsock2.h>
#include "network_library.h"
#include "UpgradeLib.h"
#include "NandRecoveryTool.h"
#include "error_code.h"

#define NET_PORT 0x8989

#define REBOOT_SHELL    "reset_tg.sh"  /*reboot shell, running in linux device, reboot device into upgrade mode*/

#define BB_RECOVER_TOOL      "linux_bb_recover_v0.1"    /*Fake Bad Block recover tool, runing in ARM side*/

#define APP_TITLE   TEXT("Unication Nandflash Scan&Recovery Tool")
#define MUTEX_NAME	TEXT("Unication Dev Tools")

/* GUI layout */
#define DEV_TOOLS_WIDTH         700         
#define DEV_TOOLS_HEIGHT        500 

#define CONTROLLER_NUM			7
#define MAX_STRING				1024


#define WINDOWS_DEBUG_ENABLE                                     

#ifdef WINDOWS_DEBUG_ENABLE	 
    #define WINDOWS_DEBUG(fmt, args...)  ({snprintf(MessageBoxBuff,MAX_STRING,TEXT(fmt),##args);MessageBox(NULL,MessageBoxBuff,TEXT("Debug"),MB_OK);})   
#else
    #define WINDOWS_DEBUG(fmt, args...)                         
#endif

#define POP_MESSAGE(hwnd,message,title,args...)		({snprintf(MessageBoxBuff,MAX_STRING,TEXT(message),##args);MessageBox(hwnd,MessageBoxBuff,TEXT(title),MB_OK);}) 
#define ERROR_MESSAGE(hwnd,message,args...)	({snprintf(MessageBoxBuff,MAX_STRING,TEXT(message),##args);MessageBox(hwnd,MessageBoxBuff,TEXT("ERROR"),MB_ICONERROR);}) 

/*use hInfo to append information in lpszInformation,use it carefully when hInfo have been initialized */
#define AppendInfo(fmt,args...)		({snprintf(lpszInformation+lstrlen(lpszInformation),MAX_STRING-lstrlen(lpszInformation),TEXT(fmt),##args);\
SetWindowText(hInfo,lpszInformation);})
#define ClearInfoBuff()		lpszInformation[0]=TEXT('\0')
#define ShowInfo(fmt,args...)		({snprintf(lpszInformation,MAX_STRING,TEXT(fmt),##args);SetWindowText(hInfo,lpszInformation);})


/*USER message definition */
#define WM_INIT				(WM_USER+S_INIT)
#define WM_REBOOTING		(WM_USER+S_REBOOTING)
#define WM_DOWNLOAD			(WM_USER+S_DOWNLOAD)
#define WM_REBOOTED			(WM_USER+S_REBOOTED)
#define WM_CONNECTING		(WM_USER+S_CONNECTING)
#define WM_CONNECTED		(WM_USER+S_CONNECTED)
#define WM_SCANING			(WM_USER+S_SCANING)
#define WM_SCANED			(WM_USER+S_SCANED)
#define WM_RECOVERING		(WM_USER+S_RECOVERING)
#define WM_RECOVERED		(WM_USER+S_RECOVERED)
#define WM_ERROR			(WM_USER+S_MAXSTAGE)
#define WM_CTLENABLE		(WM_ERROR+1)

/* controller layout definition 
** notice:	user controller's index can't modify directly
*/
#define USER_CONTROLLER		((1<<1)|(1<<2)|(1<<4)|(1<<5))
static LAYOUT controllerLayout[CONTROLLER_NUM] = {
	{NULL,TEXT("button"),"Partition Select:",\
	WS_CHILD | WS_VISIBLE | BS_GROUPBOX,  10, 10,675, 150},\
	{NULL,TEXT("button"),"rootfs",\
	WS_CHILD | WS_VISIBLE | BS_CHECKBOX , 50, 70,100, 40,},\
	{NULL,TEXT("button"),"userdata",\
	WS_CHILD | WS_VISIBLE | BS_CHECKBOX ,  250, 70,100, 40},\
	{NULL,TEXT("button"),"Partition Operation:",\
	WS_CHILD | WS_VISIBLE | BS_GROUPBOX,  10, 180,675, 300},\
	{NULL,TEXT("button"),"Scan",\
	WS_CHILD | WS_VISIBLE ,  100, 420,90, 30,},\
	{NULL,TEXT("button"),"Recover",\
	WS_CHILD | WS_VISIBLE ,  450, 420,90, 30},\
	{NULL,TEXT("static"),APP_TITLE,\
	WS_CHILD | WS_VISIBLE ,  20, 210,655, 200}
};

static TCHAR lpszInformation[MAX_STRING]; /*for information */
static TCHAR MessageBoxBuff[MAX_STRING];  /*buffer for messagebox */   
static HINSTANCE hInst; /*NandRecoveryTool's instance */
static HWND hwndMain=NULL,hInfo=NULL; /*handle for Main window and information window */
static HANDLE hMutex; /* handle for mutex */
static TCHAR *szAppName = APP_TITLE;	/*APP title */
static int stage = -1;	/*NandRecoveryTool's current stat */

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

/*function declaration */
LRESULT CALLBACK WndProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);

/*errcode map, use for translate error code to error information */
/*this table is reference from "error_code.h"*/
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
/*define user error code */
#define ERROR_THREAD_CREATE_FAILED		-1
#define ERROR_DEVICE_OFFLINE			-2
/*translate errcode to error information */
static inline const char *errcode2_string(int errcode){
	if(errcode < ERROR_CODE_OPEN_FILE_ERROR)
		return "Unkown Error";
	else if(errcode <= ERROR_CODE_INIT_LIBMTD_ERROR){
		return errcode_map[errcode - ERROR_CODE_OPEN_FILE_ERROR];
	}
	else if(errcode < ERROR_CODE_RETRY_AGAIN){
		return "Unkown Error";
	}
	else if(errcode <= ERROR_CODE_SEND_ERROR){
		return errcode_map[errcode - ERROR_CODE_RETRY_AGAIN + ERROR_CODE_INIT_LIBMTD_ERROR - ERROR_CODE_OPEN_FILE_ERROR + 1];
	}
	else if(errcode < ERROR_USB_WRITE_TIMEOUT){
		return "Unkown Error";
	}
	else if(errcode <= ERROR_USB_CABLE_UNPLUG){
		return errcode_map[errcode - ERROR_USB_WRITE_TIMEOUT + ERROR_CODE_SEND_ERROR - ERROR_CODE_RETRY_AGAIN + ERROR_CODE_INIT_LIBMTD_ERROR - ERROR_CODE_OPEN_FILE_ERROR + 2];
	}
	/*user definition errcode*/
	switch(errcode){
		case ERROR_THREAD_CREATE_FAILED:
		return "ERROR_THREAD_CREATE_FAILED";
		case ERROR_DEVICE_OFFLINE:
		return "ERROR_DEVICE_OFFLINE";
		case COMMAND_OPERATION_ERROR_ACK:
		return "ERROR_COMMAND_OPERATION_ERROR_ACK";
		default:
		return "Unkown Error";
	}
		
}


/*check the files we need if it's exist,if it's not ,quit immediately*/
static inline int CheckUnitils(void)
{	
	if(!PathFileExists(REBOOT_SHELL)){
		MessageBox(NULL,REBOOT_SHELL" lost!","Error",MB_ICONERROR);
		return 1;
	}
	if(!PathFileExists(BB_RECOVER_TOOL)){
		MessageBox(NULL,BB_RECOVER_TOOL" lost!","Error",MB_ICONERROR);
		return 1;
	}
	return 0;
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


DWORD WINAPI RebootTarget(LPVOID lpParameter)
{
	char  j, retval;	
	ClearInfoBuff();	
    /*Initialize net */   
    WSADATA wsaData;
    retval = WSAStartup(MAKEWORD(1, 1), &wsaData);
    if (retval != 0)
    {
	  ShowInfo("%s_%d: WSAStartup() error, error code is %d\n", __FILE__, __LINE__, WSAGetLastError());
      /*if WSAStartup failed, exit directly*/
	  MessageBox(hwndMain,TEXT("System incompatible!"),TEXT("ERROR"),MB_ICONERROR);
	  exit(-1);
    }
    
	ShowInfo("Rebooting device..\n");	
    /*Try 3 times...*/
    for (j = 0;j < 3;j++)
    {
        retval = file_download(REBOOT_SHELL, "/tmp/reset_tg.sh");        
        if( 0 == retval ) 
        {
			#ifdef WINDOWS_DEBUG_ENABLE
            AppendInfo("Download %s success.\n", REBOOT_SHELL);
			#endif           
            retval = exec_file_in_tg("/tmp/reset_tg.sh");
            if( 0 == retval )
            {
				#ifdef WINDOWS_DEBUG_ENABLE
				AppendInfo("Exec %s success!\n", REBOOT_SHELL);
				#endif
				break;
            }
        }
    }

END:
    WSACleanup();
EXIT:
	if(retval != 0){/*FAILED*/
		ClearInfoBuff();
		PostMessage(hwndMain,WM_ERROR,retval,0);
	}
	else{/*SUCCESS*/		
		PostMessage(hwndMain,WM_DOWNLOAD,0,0);
	}
    return  (DWORD)retval;
}

DWORD WINAPI DownloadTool(LPVOID lpParameter)
{	
	char  j, retval;    
	Sleep(10000);	/*wait for target reboot complete*/
	ClearInfoBuff(); /*clear information buff*/
    //Initialize net
    WSADATA wsaData;
    retval = WSAStartup(MAKEWORD(1, 1), &wsaData);
    if (retval != 0)
    {
      ShowInfo("%s_%d: WSAStartup() error, error code is %d\n",
	  __FILE__, __LINE__, WSAGetLastError());
      goto EXIT;
    }    

    /*Try 3 times...*/
    for (j = 0;j < 3;j++)
    {
        retval = file_download(BB_RECOVER_TOOL, "/tmp/linux_bb_recover" );
        if( 0 == retval ) 
        {
            Sleep(50);
			#ifdef WINDOWS_DEBUG_ENABLE
            AppendInfo("Download %s success\n", BB_RECOVER_TOOL);
			#endif
            retval = exec_file_in_tg("/tmp/linux_bb_recover");
            if( 0 == retval )
            {
			#ifdef WINDOWS_DEBUG_ENABLE
              AppendInfo("Exec %s success!\n", BB_RECOVER_TOOL);
			#endif
              break;
            }
        }
    }
END:
    WSACleanup();
EXIT:
	if(retval != 0){/*FAILED*/
		PostMessage(hwndMain,WM_ERROR,retval,0);
	}
	else{/*SUCCESS*/		
		PostMessage(hwndMain,WM_REBOOTED,0,0);
	}
    return  (DWORD)retval;
}

DWORD WINAPI ConnectDevice(LPVOID lpParameter)
{
	int j,retval=0;
	
	Sleep(500);	/*let reboot success information stay a little*/
	ShowInfo("Connecting target..\n");
	Sleep(1500);
	/*try 3 times*/
	for(j = 0; j < 3 ;++j){
		retval=network_try2_connect(TARGET_IP, NET_PORT);
		if(retval >= 0){		
			ShowInfo("Connect target success!\n");
			retval = 0;
			break;
		}					
	}
EXIT:
	if(retval != 0){/*FAILED*/
		ClearInfoBuff();
		PostMessage(hwndMain,WM_ERROR,retval,0);
	}
	else{/*SUCCESS*/		
		PostMessage(hwndMain,WM_CONNECTED,0,0);
	}
    return  (DWORD)retval;
}

//scan device partition
DWORD WINAPI ScanDevice(LPVOID lpParameter)
{
	int retval = 0;	
	ClearInfoBuff();
	/*do the scan job */	
	if(rootfs_checked != 1 && userdata_checked != 1){
		ShowInfo("No partition selected.\n");
		goto EXIT;
	}
	if(IsDeviceOffLine()){
		retval = ERROR_DEVICE_OFFLINE;
		goto EXIT;
	}	
	memset(command, 0, sizeof(struct Network_command));
	if(rootfs_checked == 1){		
		ShowInfo("Scaning ROOTFS partition..\n");
		command->command_id = CHECK_BB_ROOT_PATITION;
		retval=windows_send_command(command);
		if(retval < 0){
			ShowInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));			
			goto EXIT;
		}
		Sleep(3000);
		memset((unsigned char*)command, 0, sizeof(struct Network_command));
        retval=recv_from_server(command);
		if(retval<0){
			/*Linux device did not respond(time out), or something wrong with the connection*/
            AppendInfo("Error: recv_from_server().\nError code=%s.\n", errcode2_string(retval));			
			goto EXIT;
		}
		if(command->command_id == CHECK_BB_ROOT_PATITION_ACK && retval >0){
			bb_num_rootfs = command->data;
            ShowInfo("ROOTFS: %u bad block(s).\n", bb_num_rootfs);
			retval = 0;
		}
		else{
			/*not clear the processing information*/
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE CHECK_BB_ROOT_PATITION FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("CHECK_BB_ROOT_PATITION FAILED! UNKNOW ERROR!\n");        
			retval = COMMAND_OPERATION_ERROR_ACK;
			goto EXIT;
		}
	}
	if(userdata_checked == 1){
		/*save the information position for recover when USERDATA scaned*/
		int InfoPos = lstrlen(lpszInformation);		
		AppendInfo("Scaning USERDATA partition..\n");
		command->command_id = CHECK_BB_USER_PATITON;
		retval=windows_send_command(command);
		if(retval < 0){
			AppendInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));
			goto EXIT;
		}
		Sleep(3000);
		memset((unsigned char*)command, 0, sizeof(struct Network_command));
        retval=recv_from_server(command);
		if(retval<0){
			/*Linux device did not respond(time out), or something wrong with the connection*/
            AppendInfo("Error: recv_from_server().\nError code=%s.\n", errcode2_string(retval));			
			goto EXIT;
		}
		if(command->command_id == CHECK_BB_USER_PATITON_ACK && retval >0){
			bb_num_userdata = command->data;
			snprintf(lpszInformation+InfoPos,MAX_STRING-InfoPos,TEXT("USERDATA: %u bad block(s).\n"),bb_num_userdata);
			SetWindowText(hInfo,lpszInformation);
			retval = 0;
		}
		else {
			/*not clear the processing information*/
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE CHECK_BB_USER_PATITON FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("CHECK_BB_USER_PATITON FAILED! UNKNOW ERROR!\n");        
			retval = COMMAND_OPERATION_ERROR_ACK;
			goto EXIT;
		}
	}		
EXIT:
	if(retval != 0){/*FAILED*/
		PostMessage(hwndMain,WM_ERROR,retval,0);
	}
	else{/*SUCCESS*/		
		PostMessage(hwndMain,WM_SCANED,0,0);
	}
    return  (DWORD)retval;
}
//recover device partition
DWORD WINAPI RecoverDevice(LPVOID lpParameter)
{
	int old_pos,pos,retval = 0;	
	
	ClearInfoBuff();
	/*do the recover job */	
	if(rootfs_checked != 1 && userdata_checked !=1){
		AppendInfo("No partition selected.\n");
		goto EXIT;
	}
	if(IsDeviceOffLine()){
		retval = ERROR_DEVICE_OFFLINE;
		goto EXIT;
	}
	/*this macro used just for here,do not use it in other place
	*it do that if it will do actual recover job,then you will get 1(TRUE),
	*otherwise,you will get 0(FALSE) */
#if 0
 #define NEED_WARNING	((rootfs_checked == 1 && bb_num_rootfs != -1 && bb_num_rootfs > 20)\
 || (userdata_checked == 1 && bb_num_userdata != -1 && bb_num_userdata > 30))
	if(NEED_WARNING && IDOK != MessageBox(hwndMain,"Warning:This operation maybe will lost your partition data.Are you sure to do this?","Warning",MB_OKCANCEL | MB_ICONWARNING))
		goto EXIT;
#undef NEED_WARNING
#endif
	memset(command, 0, sizeof(struct Network_command));	
rootfs:
	if(rootfs_checked == 1){		
		if(bb_num_rootfs <= 20){
			if(bb_num_rootfs == 0)
				AppendInfo("ROOTFS: No bad block!\n");
			else
				AppendInfo("ROOTFS: There's only %u bad block(s), it's NOT necessary to recover it!\n", 
                            bb_num_rootfs);			
			goto userdata;
		}
		AppendInfo("Recovering ROOTFS partition..\n");		
		command->command_id = RECOVER_BB_ROOT_PATITION;		
		retval=windows_send_command(command);
		if(retval < 0){
			AppendInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));
			goto EXIT;
		}
		/*save the position of information */
		pos = lstrlen(lpszInformation);
		do /*Polling every 5sec, to get current inform.*/
        {
            Sleep(5000); 
            retval=recv_from_server(command);
            recover_bb_num=(unsigned int)command->data;
			/*truncate the processing information */
			lpszInformation[pos] = TEXT('\0');
            AppendInfo("ROOTFS: Recovered bad block(s): %u, processing: %2.2f%%\n", 
            recover_bb_num, ( (float)recover_bb_num / (float)bb_num_rootfs ) *100);
			
		}while(command->command_id != RECOVER_BB_FINISH_ACK && retval > 0);
		if(command->command_id == RECOVER_BB_FINISH_ACK && retval >0)
        {
            /*All the Bad Blocks are recovered*/
            if(recover_bb_num%bb_num_rootfs==0)
                ShowInfo("ROOTFS: Recovered %u bad block(s)!\n",recover_bb_num);
            else /*Leave some TURE Bad Block(s), that is not recoverable*/
                ShowInfo("ROOTFS: Recovered %u bad block(s), remain %d TURE Bad Block(s)!\n", 
                    recover_bb_num, bb_num_rootfs -recover_bb_num);
			rootfs_recovered = 1;
			retval = 0;
        }
        else {
			/*not clear the processing information*/
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE RECOVER_BB_ROOT_PATITION FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("RECOVER_BB_ROOT_PATITION FAILL!!!\n");
			retval = COMMAND_OPERATION_ERROR_ACK;
			goto EXIT;
		}		
	}
userdata:
	/*save the position of information */
	old_pos = lstrlen(lpszInformation);
	if(userdata_checked == 1){			
		if(bb_num_userdata <= 30){
			if(bb_num_userdata == 0)
				AppendInfo("USERDATA: No bad block!\n");
			else
				AppendInfo("USERDATA: There's only %d bad block(s), it's NOT necessary to recover it！\n", 
				bb_num_userdata);
			goto EXIT;
		}
		AppendInfo("Recovering USERDATA partition..\n");		
		command->command_id = RECOVER_BB_USER_PATITION;
		retval=windows_send_command(command);
		if(retval < 0){
			AppendInfo("Error: windows_send_command().\nError code=%s.\n", errcode2_string(retval));
			goto EXIT;
		}
		/*save the position of information */
		pos = lstrlen(lpszInformation);
		do /*Polling every 5sec, to get current inform.*/
        {
            Sleep(5000); 
            retval=recv_from_server(command);
            recover_bb_num=(unsigned int)command->data;
			/*truncate the processing information */
			lpszInformation[pos] = TEXT('\0');
            AppendInfo("USERDATA: Recovered bad block(s): %u, processing: %2.2f%%\n", 
                    recover_bb_num, ( (float)recover_bb_num / (float)bb_num_userdata ) *100);
			
		}while(command->command_id != RECOVER_BB_FINISH_ACK && retval > 0);

        /*Recover process terminated, get result*/
        if(command->command_id == RECOVER_BB_FINISH_ACK && retval >0)
        {
			/*truncate the processing information */
			lpszInformation[old_pos] = TEXT('\0');
            /*All the Bad Blocks are recovered*/
            if(recover_bb_num%bb_num_userdata==0)
                AppendInfo("USERDATA: Recovered %u bad block(s).\n",recover_bb_num);
            else /*Leave some TURE Bad Block(s), that is not recoverable*/
                AppendInfo("USERDATA: Recovered %u bad block(s), remain %d TURE Bad Block(s)!\n", 
                        recover_bb_num, bb_num_userdata - recover_bb_num);
			userdata_recovered = 1;
			retval = 0;
        }
        else{
			/*not clear the processing information*/			
			if(command->command_id == COMMAND_OPERATION_ERROR_ACK)
				AppendInfo("EXECUTE RECOVER_BB_USER_PATITION FAILED!\nERROR CODE:%s\n", errcode2_string(command->data));
			else
				AppendInfo("RECOVER_BB_USER_PATITION FAILL!!!\n");
			retval = COMMAND_OPERATION_ERROR_ACK;
			goto EXIT;
		}       
		
	}		
		
EXIT:
	
	if(retval != 0){/*FAILED*/
		PostMessage(hwndMain,WM_ERROR,retval,0);
	}
	else if(rootfs_recovered == 0 && userdata_recovered == 0){ /*have not done any real work*/
		PostMessage(hwndMain,WM_SCANED,1,0);
	}
	else{/*SUCCESS*/		
		PostMessage(hwndMain,WM_RECOVERED,0,0);
	}
    return  (DWORD)retval;
}

//callback function for main window
LRESULT CALLBACK WndProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	static int scan_error_cnt = 0;	/*device online but scan failed times */
	static int recover_error_cnt = 0;	/*device online but recover failed times*/
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
			/*make 6th controller as a information window*/
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
					SendMessage(controllerLayout[1].hwnd,BM_SETCHECK,rootfs_checked,0);
					break;
					//userdata checked
					case 2:userdata_checked = userdata_checked == 1 ? 0 : 1;
					SendMessage(controllerLayout[2].hwnd,BM_SETCHECK,userdata_checked,0);
					break;
					//scan button click
					case 4:
					assert(stage == S_INIT || stage == S_CONNECTED);
					if(stage == S_INIT){
						if(IDOK == MessageBox(hwndMain,TEXT("This operation will reboot your device,Are you sure want to do this?"),\
							TEXT("Notice"),MB_OKCANCEL)){							
							SendMessage(hwndMain,WM_REBOOTING,0,0);							
						}
					}else if(stage == S_CONNECTED){
						SendMessage(hwndMain,WM_SCANING,0,0);
					}else{
						ERROR_MESSAGE(hwndMain,"%s:%s:%d:error ocurr!",__FILE__,__func__,__LINE__);
					}						
					break;
					//recover button click
					case 5:			
					SendMessage(hwndMain,WM_RECOVERING,0,0);	
					break;
					default:					
					WINDOWS_DEBUG("Unhandled Message:%s:%d",__func__,__LINE__);					
					break;
				}
			}			
						
		}				
		break;
		case WM_CLOSE:		
		if(stage == S_INIT)
			break;
		else if(stage == S_CONNECTED || stage == S_SCANED){			
			if(IDYES == MessageBox(hwndMain,TEXT("Are you sure you want quit now?"),TEXT("Notice"),MB_YESNO)){
				/*reboot the target */
				memset(command, 0, sizeof(struct Network_command));
                command->command_id=COMMAND_REBOOT;                
                int retval=windows_send_command(command);
				if(retval < 0)					
					ERROR_MESSAGE(hwndMain,"Reboot failed! error code is %s\nYou need reboot your device manually.",errcode2_string(retval));
				break;
			}
			else
				return 0;	/*ignore this close message*/
		}		
		else	/*forbid to close window in other case */
			return 0;
		
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
			if(0 == wParam){/*normally entry S_INIT*/			
				ClearInfoBuff();
				AppendInfo("Tips: \tIf error keep occurs when device connected correctly, do this procedure:\n");
				AppendInfo("\tStep1: Reboot your device by pull battary off.\n");
				AppendInfo("\tStep2: Wait a second,upload your battary again.\n");
				AppendInfo("\tStep3: Wait about 10 seconds for device loading system.\n");
				AppendInfo("\tStep4: Close this window,and then open it again.\n");
				AppendInfo("\tStep5: Click \"Scan\" button to retry.\n");				
			}
			else{/*jump in S_INIT*/
				/*restore scan_error_cnt and recover_error_cnt */
				scan_error_cnt = 0;
				recover_error_cnt = 0;
				ClearInfoBuff();							
			}			
			/*make rootfs and userdata checked*/
			rootfs_checked = 0;
			userdata_checked = 0;
			SendMessage(hwndMain,WM_COMMAND,MAKEWPARAM(1,BN_CLICKED),0);/*check rootfs */
			SendMessage(hwndMain,WM_COMMAND,MAKEWPARAM(2,BN_CLICKED),0);/*check userdata */
			/*disable rootfs ,userdata and recover,enable scan*/
			SendMessage(hwndMain,WM_CTLENABLE,1<<4,0);			
			stage = S_INIT;				
		break;
		
		case WM_REBOOTING:
		assert(stage == S_INIT);
		stage = S_REBOOTING;		
		SendMessage(hwndMain,WM_CTLENABLE,0,0);		
		if(NULL == CreateThread(NULL,0,RebootTarget,NULL,0,NULL)){
			ClearInfoBuff();
			PostMessage(hwndMain,WM_ERROR,ERROR_THREAD_CREATE_FAILED,0);
		}
		break;
		
		case WM_DOWNLOAD:
		assert(stage == S_REBOOTING);
		stage = S_DOWNLOAD;
		/*download recover tool to target*/
		if(NULL == CreateThread(NULL,0,DownloadTool,NULL,0,NULL)){
			ClearInfoBuff();
			PostMessage(hwndMain,WM_ERROR,ERROR_THREAD_CREATE_FAILED,0);
		}
		break;
		case WM_REBOOTED:
		assert(stage == S_DOWNLOAD);
		stage = S_REBOOTED;
		/*display reboot success infomation*/
		ShowInfo("Reboot success!\n");		
		SendMessage(hwndMain,WM_CONNECTING,0,0);		
		break;		
		
		case WM_CONNECTING:
		assert(stage == S_REBOOTED);
		stage = S_CONNECTING;
		if(NULL == CreateThread(NULL,0,ConnectDevice,NULL,0,NULL)){
			ClearInfoBuff();
			PostMessage(hwndMain,WM_ERROR,ERROR_THREAD_CREATE_FAILED,0);
		}
		break;
		
		case WM_CONNECTED:
		stage = S_CONNECTED;
		/*we need do some init work ,such as global variables,etc..*/
		bb_num_rootfs=-1; /*rootfs's bad block number */
		bb_num_userdata=-1; /*userdata's bad block number */
		recover_bb_num=-1; /*bad block number that currently recovered */
		if(wParam){	/*error occur and jump in */
			/*enable scan button,disable others*/			
			SendMessage(hwndMain,WM_CTLENABLE,1<<4,0);
		}
		else{	/*normally step in*/
			ShowInfo("Connect target success.\n");
			PostMessage(hwndMain,WM_SCANING,0,0);
		}
		
		break;
		case WM_SCANING:
		assert(stage == S_CONNECTED);
		stage = S_SCANING;
		SendMessage(hwndMain,WM_CTLENABLE,0,0);	
		if(NULL == CreateThread(NULL,0,ScanDevice,NULL,0,NULL)){
			ClearInfoBuff();
			PostMessage(hwndMain,WM_ERROR,ERROR_THREAD_CREATE_FAILED,0);
		}
		break;
		case WM_SCANED:
		stage = S_SCANED;
		if(wParam == 0){/*normally entry*/
			POP_MESSAGE(hwndMain,"Scan complete!","Notice");
		}
		/*restore some global variables*/
		rootfs_recovered=0;	/*stand for rootfs option is recovered? */
		userdata_recovered=0;	/*stand for userdata option is recovered? */
		
		/*enable recover button and rootfs,userdata partition,disable scan button*/
		SendMessage(hwndMain,WM_CTLENABLE,(1<<1)|(1<<2)|(1<<5),0);		
		break;
		
		case WM_RECOVERING:
		assert(stage == S_SCANED);
		stage = S_RECOVERING;
		SendMessage(hwndMain,WM_CTLENABLE,0,0);
		if(NULL == CreateThread(NULL,0,RecoverDevice,NULL,0,NULL)){
			ClearInfoBuff();
			PostMessage(hwndMain,WM_ERROR,ERROR_THREAD_CREATE_FAILED,0);
		}
		break;
		
		case WM_RECOVERED:
		assert(stage == S_RECOVERING);
		stage = S_RECOVERED;
		/*give users a pop message and then reboot target*/
		POP_MESSAGE(hwndMain,"Recover complete! target will be rebooted.","Notice");
		memset(command, 0, sizeof(struct Network_command));
        command->command_id=COMMAND_REBOOT;                
        int retval=windows_send_command(command);
		if(retval < 0)					
			ERROR_MESSAGE(hwndMain,"Reboot failed! error code is %s\nYou need reboot your device manually.",errcode2_string(retval));
		/*jump to S_INIT*/
		PostMessage(hwndMain,WM_INIT,1,0);
		break;
		
		case WM_ERROR:
		switch(stage){			
			case S_REBOOTING:			
			AppendInfo("Technical information: %s\n",errcode2_string(wParam));
			ERROR_MESSAGE(hwndMain,"Reboot failed! please check your device if's connect correctly.\n");
			ClearInfoBuff();
			/*jump to S_INIT*/
			PostMessage(hwndMain,WM_INIT,1,0);
			break;
			case S_DOWNLOAD:			
			case S_CONNECTING:			
			AppendInfo("Technical information: %s\n",errcode2_string(wParam));
			/*Users don't care about download stage，make these two stage as one*/
			ERROR_MESSAGE(hwndMain,"Connect target failed! please push \"Scan\" button to retry.\n");
			ClearInfoBuff();
			/*jump to S_INIT*/
			PostMessage(hwndMain,WM_INIT,1,0);
			break;			
						
			case S_SCANING:
			AppendInfo("Technical information: %s\n",errcode2_string(wParam));
			/*IsDeviceOffLine may block 25 seconds most,but return immediately if device removed,if connection not stable,
			**it can be blocks long time,in this case,main window will sleep for x(max:25) seconds*/
			if(wParam == ERROR_DEVICE_OFFLINE || IsDeviceOffLine()){	/*jump to S_INIT*/
				ERROR_MESSAGE(hwndMain,"Can't detected target device! please check your device if's connect correctly, and push \"Scan\" button to start it over.");
				PostMessage(hwndMain,WM_INIT,1,0);
			}
			else{	/*jump to S_CONNECTED*/				
				++scan_error_cnt;
				if(scan_error_cnt <= 3){
					ERROR_MESSAGE(hwndMain,"Scan failed! please retry.");					
				}else if(IDOK == MessageBox(hwndMain,TEXT("Scan failed too many times! quit?"),TEXT("Error"),MB_ICONERROR | MB_OKCANCEL)){
					/*reboot target*/
					memset(command, 0, sizeof(struct Network_command));
					command->command_id=COMMAND_REBOOT;                
					int retval=windows_send_command(command);
					if(retval < 0)					
						ERROR_MESSAGE(hwndMain,"Reboot failed! error code is %s\nYou need reboot your device manually.",errcode2_string(retval));
					PostMessage(hwndMain,WM_DESTROY,0,0);
					break;
				}
				PostMessage(hwndMain,WM_CONNECTED,1,0);
			}
			ClearInfoBuff();			
			break;
			
			case S_RECOVERING:
			/*judge if's offline*/
			AppendInfo("Technical information: %s\n",errcode2_string(wParam));
			
			if(wParam == ERROR_DEVICE_OFFLINE || IsDeviceOffLine()){	/*jump to S_INIT*/
				ERROR_MESSAGE(hwndMain,"Can't detected target device! please check your device if's connect correctly, and push \"Scan\" button to start it over.");
				PostMessage(hwndMain,WM_INIT,1,0);
			}
			else{	/*jump to S_SCANED*/			
				++recover_error_cnt;
				if(recover_error_cnt <= 3){
					ERROR_MESSAGE(hwndMain,"Recover failed! please retry.");					
				}else if(IDOK == MessageBox(hwndMain,TEXT("Recover failed too many times! quit?"),TEXT("Error"),MB_ICONERROR | MB_OKCANCEL)){
					/*reboot target*/
					memset(command, 0, sizeof(struct Network_command));
					command->command_id=COMMAND_REBOOT;                
					int retval=windows_send_command(command);
					if(retval < 0)					
						ERROR_MESSAGE(hwndMain,"Reboot failed! error code is %s\nYou need reboot your device manually.",errcode2_string(retval));
					PostMessage(hwndMain,WM_DESTROY,0,0);
					break;
				}
				PostMessage(hwndMain,WM_SCANED,1,0);				
			}
			ClearInfoBuff();
			break;
			default:
				ERROR_MESSAGE(hwndMain,"%s:%s%d:Unkown error",__FILE__,__func__,__LINE__);
			break;
		}		
		break;
		
		/*enable/disable user contoller */
		case WM_CTLENABLE:
		if(wParam&(1<<1))	/*enable rootfs*/
			EnableWindow(controllerLayout[1].hwnd,1);
		else	/*disable rootfs*/
			EnableWindow(controllerLayout[1].hwnd,0);
		if(wParam&(1<<2))	/*enable userdata*/
			EnableWindow(controllerLayout[2].hwnd,1);
		else	/*disable userdata*/
			EnableWindow(controllerLayout[2].hwnd,0);
		if(wParam&(1<<4))	/*enable scan button*/
			EnableWindow(controllerLayout[4].hwnd,1);
		else	/*disable scan button*/
			EnableWindow(controllerLayout[4].hwnd,0);
		if(wParam&(1<<5))	/*enable recover button*/
			EnableWindow(controllerLayout[5].hwnd,1);
		else	/*disable recover button*/
			EnableWindow(controllerLayout[5].hwnd,0);		
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
		MessageBox(NULL,MUTEX_NAME TEXT(" is running!"),TEXT(szAppName),MB_ICONERROR);
		return FALSE;
	}

	/*check the files that we need is available? */
	if(CheckUnitils())
		return 1;
	
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
            MessageBox(NULL, TEXT("GetMessage error"), TEXT(szAppName), MB_ICONERROR);
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



