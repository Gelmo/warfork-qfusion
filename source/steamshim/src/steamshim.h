#ifndef STEAMSHIM_H
#define STEAMSHIM_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define AUTH_TICKET_MAXSIZE 1024

typedef enum STEAMSHIM_EventType
{
    SHIMEVENT_BYE,
    SHIMEVENT_STATSRECEIVED,
    SHIMEVENT_STATSSTORED,
    SHIMEVENT_SETACHIEVEMENT,
    SHIMEVENT_GETACHIEVEMENT,
    SHIMEVENT_RESETSTATS,
    SHIMEVENT_SETSTATI,
    SHIMEVENT_GETSTATI,
    SHIMEVENT_SETSTATF,
    SHIMEVENT_GETSTATF,
    SHIMEVENT_STEAMIDRECIEVED,
    SHIMEVENT_PERSONANAMERECIEVED,
    SHIMEVENT_AUTHSESSIONTICKETRECIEVED,
    SHIMEVENT_AUTHSESSIONVALIDATED,
} STEAMSHIM_EventType;

/* not all of these fields make sense in a given event. */
typedef struct STEAMSHIM_Event
{
    STEAMSHIM_EventType type;
    int okay;
    int ivalue;
    float fvalue;
    unsigned long long lvalue;
    char name[1024];
} STEAMSHIM_Event;

#ifdef __cplusplus
}
#endif

#endif

