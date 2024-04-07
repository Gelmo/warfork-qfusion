/*
Copyright (C) 2023 coolelectronics

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <chrono>
#include <thread>
#include <stdlib.h>

#include "child_private.h"
#include "../steamshim_private.h"
#include "../os.h"
#include "../steamshim.h"
#include "steam/isteamfriends.h"
#include "steam/isteammatchmaking.h"
#include "steam/isteamutils.h"
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
#include "ServerBrowser.h"


static bool GRunServer = false;
static bool GRunClient = false;

static ISteamUserStats *GSteamStats = NULL;
static ISteamUtils *GSteamUtils = NULL;
static ISteamUser *GSteamUser = NULL;
static AppId_t GAppID = 0;
static uint64 GUserID = 0;
static ISteamGameServer *GSteamGameServer = NULL;
ServerBrowser *GServerBrowser = NULL;
static time_t time_since_last_pump = 0;

static SteamCallbacks *GSteamCallbacks;

static bool processCommand(PipeBuffer cmd, ShimCmd cmdtype, unsigned int len)
{

    if (debug){
        // dbgprintf("Processing command %d\n", cmdtype);
    }
    PipeBuffer msg;
    switch (cmdtype)
    {
        case CMD_PUMP:
            time(&time_since_last_pump);
            if (GRunServer)
                SteamGameServer_RunCallbacks();
            if (GRunClient)
                SteamAPI_RunCallbacks();
            break;

        case CMD_CL_REQUESTSTEAMID:
            {
                unsigned long long id = SteamUser()->GetSteamID().ConvertToUint64();

                msg.WriteByte(EVT_CL_STEAMIDRECIEVED);
                msg.WriteLong(id);
                msg.Transmit();
            }
            break;

        case CMD_CL_REQUESTPERSONANAME:
            {
                const char* name = SteamFriends()->GetPersonaName();

                msg.WriteByte(EVT_CL_PERSONANAMERECIEVED);
                msg.WriteData((void*)name, strlen(name));
                msg.Transmit();
            }
            break;

        case CMD_CL_SETRICHPRESENCE:
            {
                int num = cmd.ReadInt();

                for (int i=0; i < num;i++){
                    const char *key = cmd.ReadString();
                    const char *val = cmd.ReadString();
                    SteamFriends()->SetRichPresence(key,val);
                }
            }
            break;
        case CMD_CL_REQUESTAUTHSESSIONTICKET:
            {
                char pTicket[AUTH_TICKET_MAXSIZE];
                uint32 pcbTicket;
                GSteamUser->GetAuthSessionTicket(pTicket,AUTH_TICKET_MAXSIZE, &pcbTicket);

                msg.WriteByte(EVT_CL_AUTHSESSIONTICKETRECIEVED);
                msg.WriteLong(pcbTicket);
                msg.WriteData(pTicket, AUTH_TICKET_MAXSIZE);
                msg.Transmit();
            }
            break;
        case CMD_SV_BEGINAUTHSESSION:
            {
                uint64 steamID = cmd.ReadLong();
                long long cbAuthTicket = cmd.ReadLong();
                void* pAuthTicket = cmd.ReadData(AUTH_TICKET_MAXSIZE);


                int result = GSteamGameServer->BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);

                msg.WriteByte(EVT_SV_AUTHSESSIONVALIDATED);
                msg.WriteInt(result);
                msg.Transmit();
            }
            break;
        case CMD_CL_REQUESTAVATAR:
            {
                uint64 id = cmd.ReadLong();
                SteamAvatarSize size = (SteamAvatarSize)cmd.ReadInt();

                if (!SteamFriends()->RequestUserInformation(id, false)){
                    TransmitAvatar(id, size);
                }
            }
            break;
        case CMD_SV_ENDAUTHSESSION:
            {
                uint64 steamID = cmd.ReadLong();
                GSteamGameServer->EndAuthSession(steamID);
            }
            break;
        case CMD_CL_OPENPROFILE:
            {
                uint64 steamID = cmd.ReadLong();
                SteamFriends()->ActivateGameOverlayToUser("steamid", steamID);
            }
            break;
        case CMD_CL_REQUESTCOMMANDLINE:
            {
                char cmdline[1024] = {0};
                SteamApps()->GetLaunchCommandLine(cmdline, 1024);
                msg.WriteByte(EVT_CL_COMMANDLINERECIEVED);
                msg.WriteString(cmdline);
                msg.Transmit();
            }
            break;
        case CMD_SV_UPDATESERVERINFO:
            {
                bool advertise = cmd.ReadByte();
                GSteamGameServer->SetAdvertiseServerActive(advertise);
                int botplayercount = cmd.ReadInt();
                GSteamGameServer->SetBotPlayerCount(botplayercount);
                bool dedicatedserver = cmd.ReadByte();
                GSteamGameServer->SetDedicatedServer(dedicatedserver);
                const char *gamedata = cmd.ReadString();
                if (gamedata[0])
                    GSteamGameServer->SetGameData(gamedata);
                const char *gamedescription = cmd.ReadString();
                if (gamedescription[0])
                    GSteamGameServer->SetGameDescription(gamedescription);
                const char *gametags = cmd.ReadString();
                if (gametags[0])
                    GSteamGameServer->SetGameTags(gametags);
                int heartbeatinterval = cmd.ReadInt();
                // GSteamGameServer->SetHeartbeatInterval(heartbeatinterval);
                const char *mapname = cmd.ReadString();
                if (mapname[0])
                    GSteamGameServer->SetMapName(mapname);
                int maxplayercount = cmd.ReadInt();
                GSteamGameServer->SetMaxPlayerCount(maxplayercount);
                const char *moddir = cmd.ReadString();
                if (moddir[0])
                    GSteamGameServer->SetModDir(moddir);
                bool passwordprotected = cmd.ReadByte();
                GSteamGameServer->SetPasswordProtected(passwordprotected);
                const char *product = cmd.ReadString();
                if (product[0])
                    GSteamGameServer->SetProduct(product);
                const char *region = cmd.ReadString();
                if (region[0])
                    GSteamGameServer->SetRegion(region);
                const char *servername = cmd.ReadString();
                if (servername[0])
                    GSteamGameServer->SetServerName(servername);
            }
            break;
        case CMD_CL_REQUESTSERVERS:
            {
                GServerBrowser->RefreshInternetServers();
            }
            break;
        case CMD_CL_REQUESTFRIENDSINFO:
            {
                int flag = cmd.ReadInt();
                int count = SteamFriends()->GetFriendCount(flag);
                msg.WriteByte(EVT_CL_FRIENDSINFORECIEVED);
                msg.WriteInt(count);
                for (int i=0; i < count; i++){
                    CSteamID id = SteamFriends()->GetFriendByIndex(i, flag);
                    msg.WriteLong(id.ConvertToUint64());
                    int relationship = SteamFriends()->GetFriendRelationship(id);
                    msg.WriteInt(relationship);
                    const char *name = SteamFriends()->GetFriendPersonaName(id);
                    msg.WriteString(name);
                    
                    int rpkeycount = SteamFriends()->GetFriendRichPresenceKeyCount(id);
                    msg.WriteInt(rpkeycount);
                    for (int j=0; j < rpkeycount; j++){
                        const char *key = SteamFriends()->GetFriendRichPresenceKeyByIndex(id, j);
                        const char *val = SteamFriends()->GetFriendRichPresence(id, key);
                        msg.WriteString(key);
                        msg.WriteString(val);
                    }
                }
                msg.Transmit();
            }
            break;
    } // switch

    return true;
}


static void processCommands()
{
  PipeBuffer buf;
  while (1){
    if (time_since_last_pump != 0){
        time_t delta = time(NULL) - time_since_last_pump;
        if (delta > 5) // we haven't gotten a pump in 5 seconds, safe to assume the main process is either dead or unresponsive and we can terminate
            return;
    }

    if (!buf.Recieve())
      continue;

    if (buf.hasmsg){

        volatile unsigned int evlen =buf.ReadInt();

        ShimCmd cmd = (ShimCmd)buf.ReadByte();

        if (!processCommand(buf, cmd, evlen))
            return; // we were told to exit
    } else {
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
  }
} 


static bool initSteamworks(PipeType fd)
{
    if (GRunClient){
        // this can fail for many reasons:
        //  - you forgot a steam_appid.txt in the current working directory.
        //  - you don't have Steam running
        //  - you don't own the game listed in steam_appid.txt
        if (!SteamAPI_Init())
            return 0;

        GSteamStats = SteamUserStats();
        GSteamUtils = SteamUtils();
        GSteamUser = SteamUser();

        GAppID = GSteamUtils ? GSteamUtils->GetAppID() : 0;
	    GUserID = GSteamUser ? GSteamUser->GetSteamID().ConvertToUint64() : 0;


        GServerBrowser = new ServerBrowser();

        GServerBrowser->RefreshInternetServers();
    }

    if (GRunServer) {
        // this will fail if port is in use
        if (!SteamGameServer_Init(0, 27015, 27016, eServerModeAuthenticationAndSecure,"0.0.0.0"))
            return 0;
        GSteamGameServer = SteamGameServer();
        if (!GSteamGameServer)
            return 0;

        GSteamGameServer->LogOnAnonymous();
        
    }
    
    GSteamCallbacks = new SteamCallbacks();
    return 1;
} 

static void deinitSteamworks() {
    if (GRunServer) {
        SteamGameServer_Shutdown();
    }

    if (GRunClient) {
        SteamAPI_Shutdown();
    }
}

static int initPipes(void)
{
    char buf[64];
    unsigned long long val;

    if (!getEnvVar("STEAMSHIM_READHANDLE", buf, sizeof (buf)))
        return 0;
    else if (sscanf(buf, "%llu", &val) != 1)
        return 0;
    else
        GPipeRead = (PipeType) val;

    if (!getEnvVar("STEAMSHIM_WRITEHANDLE", buf, sizeof (buf)))
        return 0;
    else if (sscanf(buf, "%llu", &val) != 1)
        return 0;
    else
        GPipeWrite = (PipeType) val;
    
    return ((GPipeRead != NULLPIPE) && (GPipeWrite != NULLPIPE));
} /* initPipes */

int main(int argc, char **argv)
{
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);

    if( argc > 1 && !strcmp(argv[1], "steamdebug") ) {
        debug = true;
    }
#endif

    dbgprintf("Child starting mainline.\n");


    char buf[64];
    if (!initPipes())
        fail("Child init failed.\n");
    
    if (getEnvVar("STEAMSHIM_RUNCLIENT", buf, sizeof(buf)))
        GRunClient = true;
    if (getEnvVar("STEAMSHIM_RUNSERVER", buf, sizeof(buf)))
        GRunServer = true;

    if (!initSteamworks(GPipeWrite)) {
        char failure = 0;
        writePipe(GPipeWrite, &failure, sizeof failure);
        fail("Failed to initialize Steamworks");
    }

    char success = 1;
    writePipe(GPipeWrite, &success, sizeof success);


    dbgprintf("Child in command processing loop.\n");

    // Now, we block for instructions until the pipe fails (child closed it or
    //  terminated/crashed).
    processCommands();

    dbgprintf("Child shutting down.\n");

    // Close our ends of the pipes.
    closePipe(GPipeRead);
    closePipe(GPipeWrite);

    deinitSteamworks();

    return 0;
}

#ifdef _WIN32

// from win_sys
#define MAX_NUM_ARGVS 128
int argc;
char *argv[MAX_NUM_ARGVS];

int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    if( strstr( GetCommandLineA(), "steamdebug" ) ) {
        debug = true;
        FreeConsole();
        AllocConsole();
        freopen( "CONOUT$", "w", stdout );
    }
	return main( argc, argv );
} // WinMain
#endif

