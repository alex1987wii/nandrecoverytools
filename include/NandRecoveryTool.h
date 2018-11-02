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
	S_INIT,
	S_CONNECTING,
	S_CONNECTED,	
	S_SCANING,
	S_RECOVERING
};
#endif