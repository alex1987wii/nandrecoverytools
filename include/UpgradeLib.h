#ifndef _UPGRADE_LIB_H_
#define _UPGRADE_LIB_H_
#ifdef __cplusplus
extern "C"
{
#endif
int open_debug_log();

int WinUpgradeLibInit(char *Image_buffer_pointer, unsigned long ImageLen);
int burnImage();
int burnpartition(int parts_selected);
int progress_reply_status_get (char *index, unsigned char *percent, unsigned short *status );
int file_upload(char *write_file_to_pc, char *read_file_from_target);
int file_download(char *read_file_from_pc, const char *save_file_to_target);
int exec_file_in_tg(const char *special_file);
#ifdef __cplusplus
}
#endif
#endif