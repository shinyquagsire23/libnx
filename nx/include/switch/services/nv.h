typedef enum {
	NVSERVTYPE_Default = -1,
	NVSERVTYPE_Application = 0,
	NVSERVTYPE_Applet = 1,
	NVSERVTYPE_Sysmodule = 2,
	NVSERVTYPE_T = 3,
} nvServiceType;

Result nvInitialize(nvServiceType servicetype, size_t sharedmem_size);
void nvExit(void);

Result nvOpen(u32 *fd, const char *devicepath);
Result nvIoctl(u32 fd, u32 request, void* argp);
Result nvClose(u32 fd);

