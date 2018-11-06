#ifndef NANDRECOVERYTOOL_H
#define NANDRECOVERYTOOL_H

#define IDI_ICON                        1010
typedef struct tagLAYOUT{
	HWND hwnd;
	TCHAR *lpszType;	
	TCHAR *lpszString;	
	DWORD dwStyle;
	int x;
	int y;
	int nWidth;
	int nHeight;	
}LAYOUT;
enum STAGE{
	S_INIT=0,
	S_REBOOTING,	
	S_DOWNLOAD,
	S_REBOOTED,	
	S_CONNECTING,
	S_CONNECTED,
	S_SCANING,
	S_SCANED,
	S_RECOVERING,
	S_RECOVERED,
	S_MAXSTAGE
};
#endif
