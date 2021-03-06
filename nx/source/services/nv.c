#include <string.h>
#include <switch.h>

static Handle g_nvServiceSession = INVALID_HANDLE;
static size_t g_nvIpcBufferSize = 0;
static u32 g_nvServiceType = -1;
static TransferMemory g_nvTransfermem;

static Result _nvInitialize(Handle proc, Handle sharedmem, u32 transfermem_size);
static Result _nvSetClientPID(u64 AppletResourceUserId);

Result nvInitialize(nvServiceType servicetype, size_t transfermem_size) {
    if(g_nvServiceType!=-1)return MAKERESULT(MODULE_LIBNX, LIBNX_ALREADYINITIALIZED);

    Result rc = 0;
    u64 AppletResourceUserId = 0;

    if (servicetype==NVSERVTYPE_Default || servicetype==NVSERVTYPE_Application) {
        rc = smGetService(&g_nvServiceSession, "nvdrv");
        g_nvServiceType = 0;
    }

    if ((servicetype==NVSERVTYPE_Default && R_FAILED(rc)) || servicetype==NVSERVTYPE_Applet) {
        rc = smGetService(&g_nvServiceSession, "nvdrv:a");
        g_nvServiceType = 1;
    }

    if ((servicetype==NVSERVTYPE_Default && R_FAILED(rc)) || servicetype==NVSERVTYPE_Sysmodule)
    {
        rc = smGetService(&g_nvServiceSession, "nvdrv:s");
        g_nvServiceType = 2;
    }

    if ((servicetype==NVSERVTYPE_Default && R_FAILED(rc)) || servicetype==NVSERVTYPE_T)
    {
        rc = smGetService(&g_nvServiceSession, "nvdrv:t");
        g_nvServiceType = 3;
    }

    if (R_SUCCEEDED(rc)) {
        g_nvIpcBufferSize = 0;
        rc = ipcQueryPointerBufferSize(g_nvServiceSession, &g_nvIpcBufferSize);

        if (R_SUCCEEDED(rc)) rc = tmemCreate(&g_nvTransfermem, transfermem_size, PERM_NONE);

        if (R_SUCCEEDED(rc)) rc = _nvInitialize(CUR_PROCESS_HANDLE, g_nvTransfermem.MemHandle, transfermem_size);

        //Officially ipc control DuplicateSessionEx would be used here.

        if (R_SUCCEEDED(rc)) rc = appletGetAppletResourceUserId(&AppletResourceUserId);//TODO: How do sysmodules handle this?

        if (R_SUCCEEDED(rc)) rc = _nvSetClientPID(AppletResourceUserId);
    }

    if (R_FAILED(rc)) {
        g_nvServiceType = -1;

        if(g_nvServiceSession != INVALID_HANDLE)
        {
            svcCloseHandle(g_nvServiceSession);
            g_nvServiceSession = INVALID_HANDLE;
        }

        tmemClose(&g_nvTransfermem);
    }

    return rc;
}

void nvExit(void)
{
    if(g_nvServiceType==-1)return;

    g_nvServiceType = -1;

    if(g_nvServiceSession != INVALID_HANDLE)
    {
        svcCloseHandle(g_nvServiceSession);
        g_nvServiceSession = INVALID_HANDLE;
    }

    tmemClose(&g_nvTransfermem);
}

static Result _nvInitialize(Handle proc, Handle sharedmem, u32 transfermem_size) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u32 transfermem_size;
    } *raw;

    ipcSendHandleCopy(&c, proc);
    ipcSendHandleCopy(&c, sharedmem);

    raw = ipcPrepareHeader(&c, sizeof(*raw));
    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 3;
    raw->transfermem_size = transfermem_size;

    Result rc = ipcDispatch(g_nvServiceSession);

    if (R_SUCCEEDED(rc)) {
        IpcCommandResponse r;
        ipcParseResponse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        rc = resp->result;
    }

    return rc;
}

static Result _nvSetClientPID(u64 AppletResourceUserId) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u64 AppletResourceUserId;
    } *raw;

    ipcSendPid(&c);

    raw = ipcPrepareHeader(&c, sizeof(*raw));
    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 8;
    raw->AppletResourceUserId = AppletResourceUserId;

    Result rc = ipcDispatch(g_nvServiceSession);

    if (R_SUCCEEDED(rc)) {
        IpcCommandResponse r;
        ipcParseResponse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        rc = resp->result;
    }

    return rc;
}

Result nvOpen(u32 *fd, const char *devicepath) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    ipcAddSendBuffer(&c, devicepath, strlen(devicepath), 0);

    raw = ipcPrepareHeader(&c, sizeof(*raw));
    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 0;

    Result rc = ipcDispatch(g_nvServiceSession);

    if (R_SUCCEEDED(rc)) {
        IpcCommandResponse r;
        ipcParseResponse(&r);

        struct {
            u64 magic;
            u64 result;
            u32 fd;
            u32 error;
        } *resp = r.Raw;

        rc = resp->result;
        if (R_SUCCEEDED(rc)) rc = resp->error;
        if (R_SUCCEEDED(rc)) *fd = resp->fd;
    }

    return rc;
}

Result nvIoctl(u32 fd, u32 request, void* argp) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u32 fd;
        u32 request;
    } *raw;

    size_t bufsize = _IOC_SIZE(request);

    void* buf_static = argp, *buf_transfer = argp;
    size_t buf_static_size = bufsize, buf_transfer_size = bufsize;

    if(g_nvIpcBufferSize!=0 && bufsize <= g_nvIpcBufferSize) {
        buf_transfer = NULL;
        buf_transfer_size = 0;
    }
    else {
        buf_static = NULL;
        buf_static_size = 0;
    }

    ipcAddSendBuffer(&c, buf_transfer, buf_transfer_size, 0);
    ipcAddRecvBuffer(&c, buf_transfer, buf_transfer_size, 0);

    ipcAddSendStatic(&c, buf_static, buf_static_size, 0);
    ipcAddRecvStatic(&c, buf_static, buf_static_size, 0);

    raw = ipcPrepareHeader(&c, sizeof(*raw));
    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->fd = fd;
    raw->request = request;

    Result rc = ipcDispatch(g_nvServiceSession);

    if (R_SUCCEEDED(rc)) {
        IpcCommandResponse r;
        ipcParseResponse(&r);

        struct {
            u64 magic;
            u64 result;
            u32 error;
        } *resp = r.Raw;

        rc = resp->result;
        if (R_SUCCEEDED(rc)) rc = resp->error;
    }

    return rc;
}

Result nvClose(u32 fd) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        u32 fd;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));
    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 2;
    raw->fd = fd;

    Result rc = ipcDispatch(g_nvServiceSession);

    if (R_SUCCEEDED(rc)) {
        IpcCommandResponse r;
        ipcParseResponse(&r);

        struct {
            u64 magic;
            u64 result;
            u32 error;
        } *resp = r.Raw;

        rc = resp->result;
        if (R_SUCCEEDED(rc)) rc = resp->error;
    }

    return rc;
}

