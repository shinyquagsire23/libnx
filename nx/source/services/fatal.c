// Copyright 2017 plutoo
#include <switch.h>

void fatalSimple(Result err) {
    Result rc = 0;

    if (!smHasInitialized()) {
        rc = smInitialize();
    }

    if (R_SUCCEEDED(rc)) {
        Handle srv;
        rc = smGetService(&srv, "fatal:u");

        if (R_SUCCEEDED(rc)) {
            IpcCommand c;
            ipcInitialize(&c);
            ipcSendPid(&c);

            struct {
                u64 magic;
                u64 cmd_id;
                u64 result;
                u64 unknown;
            } *raw;

            raw = ipcPrepareHeader(&c, sizeof(*raw));

            raw->magic = SFCI_MAGIC;
            raw->cmd_id = 1;
            raw->result = err;
            raw->unknown = 0;

            ipcDispatch(srv);
        }
    }

    ((void(*)())0xBADC0DE)();
}
