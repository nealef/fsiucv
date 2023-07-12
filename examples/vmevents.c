#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fsiucv.h>
#include "example.h"

#define BUFLEN          1024

#define CLASS_EVENT     0
#define CLASS_INIT      1
#define CLASS_SSICFG    2
#define CLASS_SSIEVT    3
#define CLASS_NETEVT    4

#define EV_LOGON        0
#define EV_LOGOFF       1
#define EV_FAILURE      2
#define EV_LOGTIMEOUT   3
#define EV_SLEEP        4
#define EV_RUNNABLE     5
#define EV_FREELIMIT    6
#define EV_RELOUTSTA    9
#define EV_RELINSTA     10
#define EV_RELOUTEND    11
#define EV_RELINEND     12 
#define EV_RELOUTTRM    13
#define EV_RELINTRM     14
#define EV_TIMEBOMB     15

#define IN_FAILURE      2
#define IN_TIMEOUT      3
#define IN_SLEEP        4
#define IN_FREELIMIT    6
#define IN_RELOUTACT    9
#define IN_RELINACT     10

#define SC_MODE         7
#define SC_STATE        8

#define SE_MODE         7
#define SE_STATE        8

#define NE_DEVACT       16
#define NE_DEVADD       17
#define NE_DEVDACTOP    18
#define NE_DEVDACTNOP   19
#define NE_DEVACTBOP    20
#define NE_DEVACTBNOP   21
#define NE_VSWGLBERR    22
#define NE_VSWGLBFIX    23
#define NE_IVLJOIN      24
#define NE_IVLLEFT      25

/* 
 * This sample assumes we're playing with the *EVENTS service but it should be
 * able to drive any connection
 */

typedef struct logonMsg {
    char userid[8];
} logonMsg_t;

typedef struct logoffMsg {
    char userid[8];
} logoffMsg_t;

typedef struct failureMsg {
    char userid[8];
    uint8_t reason;
} failureMsg_t;

typedef struct timeOutMsg {
    char userid[8];
    uint16_t interval;
} timeOutMsg_t;

typedef struct sleepMsg {
    char userid[8];
} sleepMsg_t;

typedef struct runnableMsg {
    char userid[8];
} runnableMsg_t;

typedef struct freeLimitMsg {
    char userid[8];
} freeLimitMsg_t;

typedef struct relOutStartMsg {
    char userid[8];
    char destination[8];
} relOutStartMsg_t;

typedef struct relInStartMsg {
    char userid[8];
    char source[8];
} relInStartMsg_t;

typedef struct relOutEndMsg {
    char userid[8];
    char destination[8];
} relOutEndMsg_t;

typedef struct relInEndMsg {
    char userid[8];
    char source[8];
} relInEndMsg_t;

typedef struct relOutTermMsg {
    char userid[8];
    uint8_t reason;
} relOutTermMsg_t;

typedef struct relInTermMsg {
    char userid[8];
    uint8_t reason;
} relInTermMsg_t;

typedef struct timeBombMsg {
    char userid[8];
} timeBombMsg_t;

typedef struct ssiModeMsg {
    char ssiName[8];
    char curMode[10];
    char prevMode[10];
} ssiModeMsg_t;

typedef struct ssiStateMsg {
    char ssiName[8];
    char curState[18];
    char prevState[18];
} ssiStateMsg_t;

typedef struct devActMsg {
    char vswName[8];
    char portGroup[8];
    uint16_t rdev;
    uint8_t port;
    uint8_t portStatus;
    uint16_t portReason;
    uint8_t vswStatus;
    uint8_t rdevStatus;
} devActMsg_t;

typedef struct devAddMsg {
    char vswName[8];
    char portGroup[8];
    uint16_t rdev;
    uint8_t port;
    uint8_t portStatus;
    uint16_t portReason;
    uint8_t vswStatus;
    uint8_t rdevStatus;
} devAddMsg_t;

typedef struct devDOpMsg {
    char vswName[8];
    char portGroup[8];
    uint16_t rdev;
    uint8_t port;
    uint8_t portStatus;
    uint16_t portReason;
    uint8_t vswStatus;
    uint8_t rdevStatus;
} devDOpMsg_t;

typedef struct devDNOMsg {
    char vswName[8];
    char portGroup[8];
    uint16_t rdev;
    uint8_t port;
    uint8_t portStatus;
    uint16_t portReason;
    uint8_t vswStatus;
    uint8_t rdevStatus;
} devDNOMsg_t;

typedef struct devBOpMsg {
    char vswName[8];
    uint16_t rdev;
    uint8_t portStatus;
    uint8_t rdevStatus;
} devBOpMsg_t;

typedef struct devBNOMsg {
    char vswName[8];
    uint16_t rdev;
    uint8_t portStatus;
    uint8_t rdevStatus;
} devBNOMsg_t;

typedef struct vswGlEMsg {
    char vswName[8];
    char system[8];
    char localVSW[8];
    char localId[8];
    char mac[6];
    char portGroup[8];
    uint16_t pchid;
    uint8_t  flags;
    uint8_t  port;
} vswGlEMsg_t;

typedef struct vswGlFMsg {
    char vswName[8];
    char system[8];
    char localVSW[8];
    char localId[8];
    char mac[6];
    char portGroup[8];
    uint16_t pchid;
    uint8_t  flags;
    uint8_t  port;
} vswGlFMsg_t;

typedef struct ivlJoinMsg {
    char system[8];
    char domain[8];
    uint8_t flags;
} ivlJoinMsg_t;

typedef struct ivlLeftMsg {
    char system[8];
    char domain[8];
    uint8_t flags;
} ivlLeftMsg_t;

static char *failure[] = { "",
                           "Disabled wait PSW",
                           "External interrupt loop",
                           "Paging error",
                           "Program interrupt loop",
                           "",
                           "",
                           "Complex interrupt loop",
                           "System soft abend",
                           "Check-stop state entered",
                           "Page zero damaged",
                           "An error occurred but CP was unable to provide a message because of a paging error"
                         };
                          
static char *relCancel[] = { "",
                             "Canceled by VMRELOCATE CANCEL command",
                             "Canceled by CPHX command",
                             "Canceled due to lost ISFC connection",
                             "Canceled due to MAXTOTAL time limit exceeded",
                             "Canceled due to MAXQUIESCE time limit exceeded",
                             "Canceled due to eligibility violation",
                             "Canceled due to virtual machine action",
                             "Canceled due to an internal processing error",
                             "Canceled because the CP exit rejected the command",
                             "",
                             "Canceled because the CP exit gave a return code that is not valid"
                           };

static char *evType[] = { "[LOGON        ] ",
                          "[LOGOFF       ] ",
                          "[FAILURE      ] ",
                          "[LOGOFFTIMEOUT] ",
                          "[FORCEDSLEEP  ] ",
                          "[RUNNABLE     ] ",
                          "[FREELIMIT    ] ",
                          "[XXXXXXXXXXXXX] ",
                          "[XXXXXXXXXXXXX] ",
                          "[RELOUTSTART  ] ",
                          "[RELINSTART   ] ",
                          "[RELOUTEND    ] ",
                          "[RELINEND     ] ",
                          "[RELOUTTERM   ] ",
                          "[RELINTERM    ] ",
                          "[TIMEBOMB     ] "
                        };

static char *inType[] = { "[YYYYYYYYYYYYY] ",
                          "[YYYYYYYYYYYYY] ",
                          "[FAILURE      ] ",
                          "[LOGOFFTIMEOUT] ",
                          "[FORCEDSLEEP  ] ",
                          "[YYYYYYYYYYYYY] ",
                          "[FREELIMIT    ] ",
                          "[YYYYYYYYYYYYY] ",
                          "[YYYYYYYYYYYYY] ",
                          "[RELOUTACTIVE ] ",
                          "[RELINACTIVE  ] "
                        };

static char *scType[] = { "[ZZZZZZZZZZZZZ] ",
                          "[ZZZZZZZZZZZZZ] ",
                          "[ZZZZZZZZZZZZZ] ",
                          "[ZZZZZZZZZZZZZ] ",
                          "[ZZZZZZZZZZZZZ] ",
                          "[ZZZZZZZZZZZZZ] ",
                          "[ZZZZZZZZZZZZZ] ",
                          "[SSIMODE      ] ",
                          "[SSISTATE     ] ",
                        };

static char *seType[] = { "[AAAAAAAAAAAAA] ",
                          "[AAAAAAAAAAAAA] ",
                          "[AAAAAAAAAAAAA] ",
                          "[AAAAAAAAAAAAA] ",
                          "[AAAAAAAAAAAAA] ",
                          "[AAAAAAAAAAAAA] ",
                          "[AAAAAAAAAAAAA] ",
                          "[SSIMODE      ] ",
                          "[SSISTATE     ] "
                        };

static char *neType[] = { "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[BBBBBBBBBBBBB] ",
                          "[DEVACTIVATED ] ",
                          "[ADDACTIVATED ] ",
                          "[DEVDEACTOP   ] ",
                          "[DEVACTPORTOP ] ",
                          "[DEVACTPORTNOP] ",
                          "[GLOBALCONNERR] ",
                          "[GLOBALCONNFIX] ",
                          "[IVLJOINED    ] ",
                          "[IVLLEFT      ] "
                       };

static char *ssiMode[] = { "",
                           "Stable",
                           "Influx",
                           "Safe"
                         };

static char *ssiState[] = { "Down",
                            "Joining",
                            "Joined",
                            "Leaving",
                            "Isolated",
                            "Suspended",
                            "Unknown"
                         };

static char *vswStatus[] = { "",
                             "VSwitch defined",
                             "Controller not available",
                             "Operator intervention required",
                             "Disconnected",
                             "VDEVs attached to controller",
                             "OSA initialization in progress",
                             "OSA device not ready",
                             "OSA device ready",
                             "OSA devices ebing detached",
                             "VSwitch delete pending",
                             "VSwitch failover recovering",
                             "Autorestart in progress"
                           };

static char *portStatus[] = { "",
                              "Bridge port defined",
                              "Controller not available",
                              "Operator intervention required",
                              "Disconnected",
                              "VDEVs attached to controller",
                              "Initialization in progress",
                              "Device not ready",
                              "Device is active bridge port",
                              "Devices being detached",
                              "VSwitch delete pending",
                              "VSwitch failover recovering",
                              "Autorestart in progress"
                            };

static void
processEvent(char *buffer, int type)
{
	char userName[9], source[9], destination[9];

	userName[8] = 0;
	source[8] = 0;
	destination[8] = 0;
    switch (type) {
        case EV_LOGON: {
            logonMsg_t *msg = (logonMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s\n",userName,evType[type]);
            break;
        }
        case EV_LOGOFF: {
            logoffMsg_t *msg = (logoffMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s\n",userName,evType[type]);
            break;
        }
        case EV_FAILURE: {
            failureMsg_t *msg = (failureMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s Reason: %s\n",userName,evType[type],failure[msg->reason]);
            break;
        }
        case EV_LOGTIMEOUT : {
            timeOutMsg_t *msg = (timeOutMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s Interval: %d\n",userName,evType[type],msg->interval);
            break;
        }
        case EV_SLEEP: {
            sleepMsg_t *msg = (sleepMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s\n",userName,evType[type]);
            break;
        }
        case EV_RUNNABLE: {
            runnableMsg_t *msg = (runnableMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s\n",userName,evType[type]);
            break;
        }
        case EV_FREELIMIT : {
            freeLimitMsg_t *msg = (freeLimitMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s\n",userName,evType[type]);
            break;
        }
        case EV_RELOUTSTA : {
            relOutStartMsg_t *msg = (relOutStartMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            memcpy(destination, msg->destination, sizeof(msg->destination));
            e2a(userName,sizeof(msg->userid));
            e2a(destination,sizeof(msg->destination));
            printf("%s %s Destination: %s\n",userName,evType[type],destination);
            break;
        }
        case EV_RELINSTA : {
            relInStartMsg_t *msg = (relInStartMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            memcpy(source, msg->source, sizeof(msg->source));
            e2a(userName,sizeof(msg->userid));
            e2a(source,sizeof(msg->source));
            printf("%s %s Source: %s\n",userName,evType[type],source);
            break;
        }
        case EV_RELOUTEND : {
            relOutEndMsg_t *msg = (relOutEndMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            memcpy(destination, msg->destination, sizeof(msg->destination));
            e2a(userName,sizeof(msg->userid));
            e2a(destination,sizeof(msg->destination));
            printf("%s %s Destination: %s\n",userName,evType[type],destination);
            break;
        }
        case EV_RELINEND : {
            relInEndMsg_t *msg = (relInEndMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            memcpy(source, msg->source, sizeof(msg->source));
            e2a(userName,sizeof(msg->userid));
            e2a(source,sizeof(msg->source));
            printf("%s %s Source: %s\n",userName,evType[type],source);
            break;
        }
        case EV_RELOUTTRM : {
            relOutTermMsg_t *msg = (relOutTermMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s Reason: %s\n",userName,evType[type],msg->reason);
            break;
        }
        case EV_RELINTRM : {
            relInTermMsg_t *msg = (relInTermMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s Reason: %s\n",userName,evType[type],source);
            break;
        }
    }
}

static void
processInit(char *buffer, int type)
{
    char userName[9],
         source[9],
         destination[9];

    userName[8] = 0;
    source[8] = 0;
    destination[8] = 0;

    switch (type) {
        case IN_FAILURE: {
            failureMsg_t *msg = (failureMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s Reason: %s\n",userName,inType[type],failure[msg->reason]);
            break;
        }
        case IN_TIMEOUT : {
            timeOutMsg_t *msg = (timeOutMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s Interval: %d\n",userName,inType[type],msg->interval);
            break;
        }
        case IN_SLEEP: {
            sleepMsg_t *msg = (sleepMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s\n",userName,inType[type]);
            break;
        }
        case IN_FREELIMIT: {
            sleepMsg_t *msg = (sleepMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            e2a(userName,sizeof(msg->userid));
            printf("%s %s\n",userName,inType[type]);
            break;
        }
        case IN_RELOUTACT : {
            relOutStartMsg_t *msg = (relOutStartMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            memcpy(destination, msg->destination, sizeof(msg->destination));
            e2a(userName,sizeof(msg->userid));
            e2a(destination,sizeof(msg->userid));
            printf("%s %s Destination: %s\n",userName,inType[type],destination);
            break;
        }
        case IN_RELINACT : {
            relInStartMsg_t *msg = (relInStartMsg_t *) buffer;
            memcpy(userName, msg->userid, sizeof(msg->userid));
            memcpy(source, msg->source, sizeof(msg->source));
            e2a(userName,sizeof(msg->userid));
            e2a(source,sizeof(msg->userid));
            printf("%s %s Source: %s\n",userName,inType[type],source);
            break;
        }
    }
}

static void
processSSICfg(char *buffer, int type)
{
    char ssiName[9],
         mode[11],
         state[19];

    ssiName[8] = 0;

    switch (type) {
        case SC_MODE : {
            ssiModeMsg_t *msg = (ssiModeMsg_t *) buffer;
            mode[10] = 0;
            memcpy(ssiName, msg->ssiName, sizeof(msg->ssiName));
            memcpy(mode, msg->curMode, sizeof(msg->curMode));
            e2a(ssiName,sizeof(msg->ssiName));
            e2a(mode,sizeof(msg->curMode));
            printf("%s %s Mode: %s\n",ssiName,scType[type],mode);
            break;
        }
        case SC_STATE : {
            ssiStateMsg_t *msg = (ssiStateMsg_t *) buffer;
            state[18] = 0;
            memcpy(ssiName, msg->ssiName, sizeof(msg->ssiName));
            memcpy(state, msg->curState, sizeof(msg->curState));
            e2a(ssiName,sizeof(msg->ssiName));
            e2a(state,sizeof(msg->curState));
            printf("%s %s Mode: %s\n",ssiName,scType[type],state);
            break;
        }
    }
}

static void
processSSIEvent(char *buffer, int type)
{
    char ssiName[9],
         curMode[11],
         prevMode[11],
         curState[19],
         prevState[19];

    ssiName[8] = 0;

    switch (type) {
        case SE_MODE : {
            ssiModeMsg_t *msg = (ssiModeMsg_t *) buffer;
            curMode[10] = 0;
            prevMode[10] = 0;
            memcpy(ssiName, msg->ssiName, sizeof(msg->ssiName));
            memcpy(curMode, msg->curMode, sizeof(msg->curMode));
            memcpy(prevMode, msg->prevMode, sizeof(msg->prevMode));
            e2a(ssiName,sizeof(msg->ssiName));
            e2a(curMode,sizeof(msg->curMode));
            e2a(prevMode,sizeof(msg->prevMode));
            printf("%s %s Current Mode: %s Previous Mode: %s\n",ssiName,seType[type],curMode,prevMode);
            break;
        }
        case SE_STATE : {
            ssiStateMsg_t *msg = (ssiStateMsg_t *) buffer;
            curState[18] = 0;
            prevState[18] = 0;
            memcpy(ssiName, msg->ssiName, sizeof(msg->ssiName));
            memcpy(curState, msg->curState, sizeof(msg->curState));
            memcpy(prevState, msg->prevState, sizeof(msg->prevState));
            e2a(ssiName,sizeof(msg->ssiName));
            e2a(curState,sizeof(msg->curState));
            e2a(prevState,sizeof(msg->prevState));
            printf("%s %s Current State: %s Previous State: %s\n",ssiName,seType[type],curState,prevState);
            break;
        }
    }
}

static void
processNetEvent(char *buffer, int type)
{
    char vswName[9];
    char portGroup[9];
    char system[9];
    char localVSW[9];
    char localId[9];
    char domain[9];

    switch (type) {
        case NE_DEVACT : {
            devActMsg_t *msg = (devActMsg_t *) buffer;
            vswName[8] = 0;
            portGroup[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            memcpy(portGroup, msg->portGroup, sizeof(msg->portGroup));
            e2a(vswName, sizeof(msg->vswName));
            e2a(portGroup, sizeof(msg->portGroup));
            printf("%s %s portGroup: %s rdev: 0x%04x port: %d portStatus: %s portReason: %d vswStatus: %s rdevStatus: 0x%02x\n",
                   vswName, neType[type], portGroup, msg->rdev, msg->port, portStatus[msg->portStatus], msg->portReason, 
                   vswStatus[msg->vswStatus], msg->rdevStatus);
            break;
        }
        case NE_DEVADD : {
            devAddMsg_t *msg = (devAddMsg_t *) buffer;
            vswName[8] = 0;
            portGroup[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            memcpy(portGroup, msg->portGroup, sizeof(msg->portGroup));
            e2a(vswName, sizeof(msg->vswName));
            e2a(portGroup, sizeof(msg->portGroup));
            printf("%s %s portGroup: %s rdev: 0x%04x port: %d portStatus: %s portReason: %d vswStatus: %s rdevStatus: 0x%02x\n",
                   vswName, neType[type], portGroup, msg->rdev, msg->port, portStatus[msg->portStatus], msg->portReason, 
                   vswStatus[msg->vswStatus], msg->rdevStatus);
            break;
        }
        case NE_DEVDACTOP : {
            devDOpMsg_t *msg = (devDOpMsg_t *) buffer;
            vswName[8] = 0;
            portGroup[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            memcpy(portGroup, msg->portGroup, sizeof(msg->portGroup));
            e2a(vswName, sizeof(msg->vswName));
            e2a(portGroup, sizeof(msg->portGroup));
            printf("%s %s portGroup: %s rdev: 0x%04x port: %d portStatus: %s portReason: %d vswStatus: %s rdevStatus: 0x%02x\n",
                   vswName, neType[type], portGroup, msg->rdev, msg->port, portStatus[msg->portStatus], msg->portReason, 
                   vswStatus[msg->vswStatus], msg->rdevStatus);
            break;
        }
        case NE_DEVDACTNOP : {
            devDNOMsg_t *msg = (devDNOMsg_t *) buffer;
            vswName[8] = 0;
            portGroup[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            memcpy(portGroup, msg->portGroup, sizeof(msg->portGroup));
            e2a(vswName, sizeof(msg->vswName));
            e2a(portGroup, sizeof(msg->portGroup));
            printf("%s %s portGroup: %s rdev: 0x%04x port: %d portStatus: %s portReason: %d vswStatus: %s rdevStatus: 0x%02x\n",
                   vswName, neType[type], portGroup, msg->rdev, msg->port, portStatus[msg->portStatus], msg->portReason, 
                   vswStatus[msg->vswStatus], msg->rdevStatus);
            break;
        }
        case NE_DEVACTBOP : {
            devBOpMsg_t *msg = (devBOpMsg_t *) buffer;
            vswName[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            e2a(vswName, sizeof(msg->vswName));
            printf("%s %s rdev: 0x%04x portStatus: %s rdevStatus: 0x%02x\n",
                   vswName, neType[type], msg->rdev, portStatus[msg->portStatus], msg->rdevStatus);
            break;
        }
        case NE_DEVACTBNOP : {
            devBNOMsg_t *msg = (devBNOMsg_t *) buffer;
            vswName[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            e2a(vswName, sizeof(msg->vswName));
            printf("%s %s rdev: 0x%04x %d portStatus: %s %s rdevStatus: 0x%02x\n",
                   vswName, neType[type], msg->rdev, portStatus[msg->portStatus], msg->rdevStatus);
            break;
        }
        case NE_VSWGLBERR: {
            vswGlEMsg_t *msg = (vswGlEMsg_t *) buffer;
            vswName[8] = 0;
            system[8] = 0;
            localVSW[8] = 0;
            localId[8] = 0;
            portGroup[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            e2a(vswName, sizeof(msg->vswName));
            memcpy(system, msg->system, sizeof(msg->system));
            e2a(system, sizeof(msg->system));
            memcpy(localVSW, msg->localVSW, sizeof(msg->localVSW));
            e2a(localVSW, sizeof(msg->localVSW));
            memcpy(localId, msg->localId, sizeof(msg->localId));
            e2a(localId, sizeof(msg->localId));
            memcpy(portGroup, msg->portGroup, sizeof(msg->portGroup));
            e2a(portGroup, sizeof(msg->portGroup));
            printf("%s %s system: %s local VSwitch: %s local ID: %s mac: %02x:%02x:%02x:%02x:%02x:%02x portGroup: %s "
                   "pchid: 0x%04x flags: 0x%02x port: %d\n",
                   vswName, neType[type], system, localVSW, localId, 
                   msg->mac[0], msg->mac[1], msg->mac[2], msg->mac[3], msg->mac[4], msg->mac[5], portGroup,
                   (msg->flags & 0x80 != 0 ? msg->pchid : -1), msg->flags, msg->port);
            break;
        }
        case NE_VSWGLBFIX: {
            vswGlFMsg_t *msg = (vswGlFMsg_t *) buffer;
            vswName[8] = 0;
            system[8] = 0;
            localVSW[8] = 0;
            localId[8] = 0;
            portGroup[8] = 0;
            memcpy(vswName, msg->vswName, sizeof(msg->vswName));
            e2a(vswName, sizeof(msg->vswName));
            memcpy(system, msg->system, sizeof(msg->system));
            e2a(system, sizeof(msg->system));
            memcpy(localVSW, msg->localVSW, sizeof(msg->localVSW));
            e2a(localVSW, sizeof(msg->localVSW));
            memcpy(localId, msg->localId, sizeof(msg->localId));
            e2a(localId, sizeof(msg->localId));
            memcpy(portGroup, msg->portGroup, sizeof(msg->portGroup));
            e2a(portGroup, sizeof(msg->portGroup));
            printf("%s %s system: %s local VSwitch: %s local ID: %s mac: %02x:%02x:%02x:%02x:%02x:%02x portGroup: %s "
                   "pchid: 0x%04x flags: 0x%02x port: %d\n",
                   vswName, neType[type], system, localVSW, localId, 
                   msg->mac[0], msg->mac[1], msg->mac[2], msg->mac[3], msg->mac[4], msg->mac[5], portGroup,
                   (msg->flags & 0x80 != 0 ? msg->pchid : -1), msg->flags, msg->port);
            break;
        }
        case NE_IVLJOIN: {
            ivlJoinMsg_t *msg = (ivlJoinMsg_t *) buffer;
            system[8] = 0;
            domain[8] = 0;
            memcpy(system, msg->system, sizeof(msg->system));
            e2a(system, sizeof(msg->system));
            memcpy(domain, msg->domain, sizeof(msg->domain));
            e2a(domain, sizeof(msg->domain));
            printf("%s %s domain: %s flags: 0x%02x\n",
                   system, neType[type], domain, msg->flags);
            break;
        }
        case NE_IVLLEFT: {
            ivlLeftMsg_t *msg = (ivlLeftMsg_t *) buffer;
            system[8] = 0;
            domain[8] = 0;
            memcpy(system, msg->system, sizeof(msg->system));
            e2a(system, sizeof(msg->system));
            memcpy(domain, msg->domain, sizeof(msg->domain));
            e2a(domain, sizeof(msg->domain));
            printf("%s %s domain: %s flags: 0x%02x\n",
                   system, neType[type], domain, msg->flags);
            break;
        }
    }
}

int
main(int argc, char **argv)
{
	int fd, trgcls, class, type;
	char *buffer = malloc(BUFLEN);
	size_t count;

	printf("ID       Type            Parameters\n"
	       "-------- --------------- ------------------------------------------------------------------------------\n");
	fd = open("/dev/iucv0", O_RDONLY);
	if (fd >= 0) {
		count = read(fd, buffer, BUFLEN);
		while (count > 0) {
			ioctl(fd, FSIUCVTCG, (char *) &trgcls);
            class = trgcls >> 16;
            type = trgcls & 0xff;
			switch (class) {
                case CLASS_EVENT : 
                    processEvent(buffer, type);
                    break;

                case CLASS_INIT :
                    processInit(buffer, type);
                    break;

                case CLASS_SSICFG : 
                    processSSICfg(buffer, type);
                    break;
                    
                case CLASS_SSIEVT : 
                    processSSIEvent(buffer, type);
                    break;
                    
                case CLASS_NETEVT : 
                    processNetEvent(buffer, type);
                    break;

            }
			count = read(fd, buffer, BUFLEN);
		}
		close(fd);
	} else {
		perror("open");
    }
    free(buffer);
}
