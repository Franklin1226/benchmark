void encode(const char* buf, int size);
void backup(const char* path);
void decode(int disk_id, char* buf, long long size, long long offset, int* dm_ids);
void recover(int disk_id, const char* newdevice);