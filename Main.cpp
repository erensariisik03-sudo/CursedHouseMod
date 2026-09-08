#include <list>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include <cstdlib>
#include <cmath>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"

struct Vector3 { float x, y, z; };

// ============ TOGGLES ============
bool GodMode = false, Invisible = false, noclipMode = false;
float noclipSpeed = 3.0f, walkSpeedValue = 0, crouchSpeedValue = 0;
bool UnlimitedAmmo = false, UnlimitedDart = false, UnlimitedSpray = false;
bool GiveShotgun = false, GiveCrossbow = false, GiveSpray = false, GiveFreezeTrap = false;
bool PlayAsGranny = false, PlayAsMomSpider = false, PlayAsSpider = false, PlayAsCrow = false, PlayAsRat = false;
int spawnItemSpinner = 0; bool spawnItemToggle = false, AutoSpawnItems = false, Spawnallitems = false, GiveAllItems = false;
int spawnItemsNumber = 1, difficultyLevel = 0, grannySpawnTime = 0;
int teleportSpinner = 0; float customPosX = 0, customPosY = 0, customPosZ = 0;
bool teleportToGranny = false, teleportGrannyToMe = false, teleportMeToMomSpider = false, teleportMomSpiderToMe = false;
bool teleportMeToSpider = false, teleportSpiderToMe = false, teleportMeToCrow = false, teleportCrowToMe = false;
bool teleportRight = false, teleportLeft = false, teleportUp = false, teleportDown = false;
bool teleportMeToRandomPlayer = false, teleportRandomPlayerToMe = false, teleportAllPlayersToMe = false, teleportMeToAllPlayers = false;
bool CloneGranny = false, DestroyCloneGranny = false, KillGranny = false, StopGranny = false, noDropBearTrap = false, noHearingGranny = false;
float grannyAttackDist = 1, grannyVarSpeed = 1, grannyAnimSpeed = 1, grannySize = 1;
bool CloneMomSpider = false, DestroyCloneMomSpider = false, KillMomSpider = false, StopMomSpider = false, noHearingMom = false;
float momAttackDist = 1, momVarSpeed = 1, momAnimSpeed = 1, momSpiderSize = 1;
bool CloneSpider = false, DestroyCloneSpider = false, KillSpider = false, StopSpider = false, noHearingSpider = false;
float spiderAttackDist = 1, spiderVarSpeed = 1, spiderAnimSpeed = 1, spiderSize = 1;
bool CloneCrow = false, DestroyCloneCrow = false, KillCrow = false, StopCrow = false, noHearingCrow = false;
float crowAttackDist = 1, crowVarSpeed = 1, crowAnimSpeed = 1, crowSize = 1;
bool CloneRat = false, DestroyCloneRat = false, KillRat = false, StopRat = false, noHearingRat = false;
float ratAttackDist = 1, ratVarSpeed = 1, ratAnimSpeed = 1, ratSize = 1;
bool ForceMaster = false, OptimizedPing = true, AntiTimeout = true, AntiKick = true, AntiDisconnect = true, AntiBan = true, AutoFix = true, masterKicked = false;
bool KickPlayerButton = false; int customPingValue = 0, customMaxPlayers = 0, customPlayerCount = 0, kickPlayerSpinner = 0;

void *grannyInstance = nullptr, *momSpiderInstance = nullptr, *spiderInstance = nullptr, *crowInstance = nullptr, *ratInstance = nullptr;
void *instanceBtn = nullptr, *activePlayerControllerInstance = nullptr, *launcherInstance = nullptr;
void *playerTransformInstance = NULL, *grannyTransformInstance = NULL, *crowTransformInstance = NULL;
void *clonedGrannyInstance = nullptr, *clonedMomSpiderInstance = nullptr, *clonedSpiderInstance = nullptr, *clonedCrowInstance = nullptr, *clonedRatInstance = nullptr;

void (*set_position)(void*, Vector3) = NULL;
void (*get_position)(void*, Vector3*) = NULL;
void* (*get_transform)(void*) = NULL;
Vector3 (*get_localScale)(void*) = nullptr;
void (*set_localScale)(void*, Vector3) = nullptr;
void* (*get_gameObject)(void*) = nullptr;
void* (*Object_Instantiate)(void*) = nullptr;
void* (*Object_Instantiate_PosRot)(void*, Vector3, void*) = nullptr;
void (*Object_Destroy)(void*) = nullptr;
void* (*Component_get_transform)(void*) = nullptr;
Vector3 (*get_position_V3)(void*) = nullptr;
void (*set_position_V3)(void*, Vector3) = nullptr;

// ============ FUNCTION TYPEDEFS ============
typedef void(*toggleShootButtonShotgun_t)(void* instance, bool setTo);
typedef void(*toggleShootButtonArrow_t)(void* instance, bool setTo);
typedef void(*toggleMasterClientOptionsWindow_t)(void* instance, bool isOn);

toggleShootButtonShotgun_t orig_toggleShootButtonShotgun;
toggleShootButtonArrow_t orig_toggleShootButtonArrow;
toggleMasterClientOptionsWindow_t orig_toggleMasterClientOptionsWindow;

void (*checkInventory_Func)(void*);
void (*RPC_askMasterToKillGrannyWithShotgun)(void*);
void (*RemovePlayer)(void*, int) = nullptr;

void (*old_playerController_Update)(void*);
int (*old_CharacterController_Move)(void*, Vector3);
void (*old_Update_ShootButton)(void*);
void (*old_checkSprayUpdate)(void*);
void (*old_ShootArrowUpdate)(void*);
float (*old_manageGrannyAI_GetGrannyDeathTime)(void*, bool);
void (*old_GrannyFixedUpdate)(void*);
void (*old_MomFixedUpdate)(void*);
void (*old_SpiderControlUpdate)(void*);
void (*old_CrowUpdate)(void*);
void (*old_RatUpdate)(void*);
void (*old_die)(void*);
void (*old_RPC_playerDies)(void*, void*);
void (*old_RPC_playerDiesBySpider)(void*);
void (*old_RPC_playerKilledByAI)(void*);
void (*old_RPC_playerDiesBySpiderAIGranny)(void*);
void (*old_dieByLava)(void*);
void (*old_RPC_playerExplosionDeath)(void*);
void (*old_onDiedByOpeningBox)(void*);
void (*old_RPC_onSpiderMomKilledYou)(void*);
void (*old_crowAttackPlayer)(void*);
void (*old_playerHitBearTrap)(void*);
void (*old_hitByArrow)(void*);
void (*old_RPC_playPlayerBurningOnDeath)(void*);
void (*old_playerSpiderBittenDead)(void*);
void (*old_playerSpiderBittenDeadAIGranny)(void*);
void (*old_AIDie)(void*);
void (*old_playerManager_die)(void*);
void (*old_dieAI)(void*);
void (*old_ratBite_OnTriggerEnter)(void*, void*);
void (*old_momKillPlayer)(void*);
void (*old_playerExplode)(void*);
void (*old_playerFallDead)(void*);
void (*old_playerFalling)(void*);
void (*old_noFall)(void*);
void (*old_waitToResetBack)(void*);
void (*old_checkFall)(void*);
void (*old_PlayerLandBad)(void*);
void (*old_afterPlayerFallHurt)(void*);
void (*old_fallingDead)(void*);
void (*old_RPC_sendMessageFallen)(void*, void*);
void (*old_triggerFellEvent)(void*);
void (*old_triggerFallFloor_OnTriggerEnter)(void*, void*);
void (*old_EnemyEyeUpdate)(void*);
void (*old_momOnPlayerSeen)(void*);
void (*old_momCheckForPlayers)(void*);
void (*old_RPC_playMomRoar)(void*);
void (*old_momParseForPlayers)(void*);
void (*old_spider_funcHuntPlayer)(void*);
void (*old_RoomInfo_InternalCacheProperties)(void*, void*);
bool (*old_get_IsMasterClient)(void*);
void (*old_OnMasterClientSwitched)(void*, void*);
int (*old_GetPing)();
void (*old_set_KeepAliveInBackground)(float);
void (*old_Disconnect)();
bool (*old_CloseConnectionHook)(void*);
void (*old_AntiCheatAwake)(void*);
void (*old_ExitGamePunisher_Punish)(void*);
bool (*old_get_CrcCheckEnabled)();
int (*old_get_MaxResendsBeforeDisconnect)();
int (*old_get_PacketLossByCrcCheck)();
int (*old_get_CountOfPlayers)();
void (*old_launcher_Update)(void*);
void (*old_launcher_OnJoinedRoom)(void*);
void (*old_launcher_OnPlayerLeftRoom)(void*, void*);
void (*old_changeRoomVisibility)(void*, bool);

// ============ HELPERS ============
int getRandomItemOffset() { int ri=(rand()%49)+1; return (ri==31)?0x500:((ri>31)?0x4CF+ri+1:0x4CF+ri); }
int getSpinnerItemOffset(int s){switch(s){case 1:return 0x500;case 2:return getRandomItemOffset();case 3:return 0x4D0;case 4:return 0x4D1;case 5:return 0x4D2;case 6:return 0x4D3;case 7:return 0x4D4;case 8:return 0x4D5;case 9:return 0x4D6;case 10:return 0x4D7;case 11:return 0x4D8;case 12:return 0x4D9;case 13:return 0x4DA;case 14:return 0x4DB;case 15:return 0x4DC;case 16:return 0x4DD;case 17:return 0x4DE;case 18:return 0x4DF;case 19:return 0x4E0;case 20:return 0x4E1;case 21:return 0x4E2;case 22:return 0x4E3;case 23:return 0x4E4;case 24:return 0x4E5;case 25:return 0x4E6;case 26:return 0x4E7;case 27:return 0x4E8;case 28:return 0x4E9;case 29:return 0x4EA;case 30:return 0x4EB;case 31:return 0x4EC;case 32:return 0x4ED;case 33:return 0x500;case 34:return 0x4EE;case 35:return 0x4EF;case 36:return 0x4F0;case 37:return 0x4F1;case 38:return 0x4F2;case 39:return 0x4F3;case 40:return 0x4F4;case 41:return 0x4F5;case 42:return 0x4F6;case 43:return 0x4F7;case 44:return 0x4F8;case 45:return 0x4F9;case 46:return 0x4FA;case 47:return 0x4FB;case 48:return 0x4FC;case 49:return 0x4FD;case 50:return 0x4FE;case 51:return 0x4FF;default:return -1;}}
void spawnItemBySpinner(void* i, int sp) {if(sp==2){*(bool*)((uint64_t)i+getRandomItemOffset())=true;}else{int o=getSpinnerItemOffset(sp);if(o!=-1)*(bool*)((uint64_t)i+o)=true;}}
void TeleportPlayerToLocation(void* pt, int loc) {if(!pt||!set_position)return;Vector3 p;switch(loc){case 1:p.x=0;p.y=6;p.z=0;break;case 2:p.x=0;p.y=7;p.z=9;break;case 3:p.x=0;p.y=7;p.z=5;break;case 4:p.x=5;p.y=12;p.z=0;break;case 5:p.x=0;p.y=12;p.z=9;break;case 6:p.x=7;p.y=5;p.z=5;break;case 7:p.x=0;p.y=20;p.z=0;break;case 8:p.x=0;p.y=30;p.z=12;break;case 9:p.x=0;p.y=35;p.z=6;break;case 10:p.x=7;p.y=0;p.z=5;break;case 11:p.x=0;p.y=0;p.z=8;break;case 12:p.x=-15;p.y=6;p.z=-10;break;case 13:p.x=5;p.y=6;p.z=-17;break;case 14:p.x=2;p.y=6;p.z=-17;break;case 15:p.x=6;p.y=1;p.z=-17;break;case 16:p.x=27;p.y=1;p.z=-25;break;case 17:p.x=17;p.y=1;p.z=-33;break;case 18:p.x=17;p.y=2;p.z=0;break;case 19:p.x=25;p.y=2;p.z=-34;break;case 20:p.x=25;p.y=2;p.z=-12;break;case 21:p.x=15;p.y=20;p.z=-7;break;case 22:p.x=-5;p.y=20;p.z=-5;break;case 23:p.x=-16;p.y=20;p.z=12;break;case 24:p.x=0;p.y=2;p.z=-30;break;case 25:p.x=-3;p.y=20;p.z=-25;break;case 26:p.x=-3;p.y=15;p.z=-10;break;case 27:p.x=-3;p.y=-7;p.z=-40;break;case 28:p.x=5;p.y=15;p.z=-40;break;case 29:p.x=12;p.y=12;p.z=12;break;case 30:p.x=25;p.y=12;p.z=-20;break;case 31:p.x=30;p.y=12;p.z=-5;break;default:return;}set_position(pt,p);}
void TeleportPlayerToCustom(void* pt, float x, float y, float z) {if(!pt||!set_position)return;Vector3 p;p.x=x;p.y=y;p.z=z;set_position(pt,p);}

// ============ SERVER HOOKS ============
void RoomInfo_InternalCacheProperties(void* i, void* p) {old_RoomInfo_InternalCacheProperties(i,p);if(ForceMaster&&i)*(int*)((uint64_t)i+0x48)=1;}
bool get_IsMasterClient(void* i) {if(ForceMaster)return true;return old_get_IsMasterClient(i);}
void OnMasterClientSwitched(void* i, void* n) {if(ForceMaster){if(!masterKicked){int pm=*(int*)((uint64_t)i+0x48);if(pm!=1&&pm>0&&RemovePlayer)RemovePlayer(i,pm);masterKicked=true;}return;}masterKicked=false;old_OnMasterClientSwitched(i,n);}
int GetPing() {if(OptimizedPing)return(rand()%2)+1;if(customPingValue>0)return customPingValue;return old_GetPing();}
void set_KeepAliveInBackground(float v) {if(AntiTimeout){old_set_KeepAliveInBackground(2147483647);return;}old_set_KeepAliveInBackground(v);}
void Disconnect() {if(AntiKick)return;old_Disconnect();}
bool CloseConnectionHook(void* k) {if(AntiDisconnect)return false;return old_CloseConnectionHook(k);}
void AntiCheatAwake(void* i) {if(AntiBan){*(int*)((uint64_t)i+0x20)=-2147483647;*(float*)((uint64_t)i+0x24)=-2147483647.0f;}old_AntiCheatAwake(i);}
void ExitGamePunisher_Punish(void* i) {if(AntiBan)return;old_ExitGamePunisher_Punish(i);}
bool get_CrcCheckEnabled() {if(AutoFix)return false;return old_get_CrcCheckEnabled();}
int get_MaxResendsBeforeDisconnect() {if(AutoFix)return 2147483647;return old_get_MaxResendsBeforeDisconnect();}
int get_PacketLossByCrcCheck() {if(AutoFix)return 0;return old_get_PacketLossByCrcCheck();}
int get_CountOfPlayers() {if(customPlayerCount>0)return customPlayerCount;return old_get_CountOfPlayers();}
void launcher_OnJoinedRoom(void* i) {old_launcher_OnJoinedRoom(i);launcherInstance=i;if(ForceMaster&&i){*(bool*)((uint64_t)i+0x1E)=true;*(int*)((uint64_t)i+0x24)=1;masterKicked=false;}}
void launcher_OnPlayerLeftRoom(void* i, void* o) {if(ForceMaster&&i){*(bool*)((uint64_t)i+0x1E)=true;*(int*)((uint64_t)i+0x24)=1;masterKicked=false;}old_launcher_OnPlayerLeftRoom(i,o);}
void changeRoomVisibility(void* i, bool j) {if(ForceMaster){old_changeRoomVisibility(i,true);return;}old_changeRoomVisibility(i,j);}
void launcher_Update_Hook(void* i) {if(i){launcherInstance=i;if(ForceMaster){orig_toggleMasterClientOptionsWindow(i,true);}}old_launcher_Update(i);}

// ============ NO CLIP ============
void playerController_Update_Hook(void* i) {
    if(i){
        activePlayerControllerInstance=i;instanceBtn=i;
        if(get_transform)playerTransformInstance=get_transform(i);
        if(noclipMode){*(float*)((uint64_t)i+0x270)=0;*(float*)((uint64_t)i+0x458)=0;*(float*)((uint64_t)i+0x45C)=0;*(float*)((uint64_t)i+0x460)=0;}
        else{float cg=*(float*)((uint64_t)i+0x270);if(cg==0)*(float*)((uint64_t)i+0x270)=1;}
        if(PlayAsGranny&&grannyTransformInstance&&playerTransformInstance&&get_position&&set_position){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(grannyTransformInstance,pp);}
        if(PlayAsMomSpider&&momSpiderInstance&&playerTransformInstance&&get_transform&&get_position&&set_position){void* mt=get_transform(momSpiderInstance);if(mt){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(mt,pp);}}
        if(PlayAsSpider&&spiderInstance&&playerTransformInstance&&get_transform&&get_position&&set_position){void* st=get_transform(spiderInstance);if(st){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(st,pp);}}
        if(PlayAsCrow&&crowTransformInstance&&playerTransformInstance&&get_position&&set_position){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(crowTransformInstance,pp);}
        if(PlayAsRat&&ratInstance&&playerTransformInstance&&get_transform&&get_position&&set_position){void* rt=get_transform(ratInstance);if(rt){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(rt,pp);}}
        if(teleportToGranny&&playerTransformInstance&&grannyTransformInstance&&get_position&&set_position){Vector3 gp;get_position(grannyTransformInstance,&gp);set_position(playerTransformInstance,gp);teleportToGranny=false;}
        if(teleportMeToMomSpider&&playerTransformInstance&&momSpiderInstance&&get_transform&&get_position&&set_position){void* mt=get_transform(momSpiderInstance);if(mt){Vector3 mp;get_position(mt,&mp);set_position(playerTransformInstance,mp);}teleportMeToMomSpider=false;}
        if(teleportMeToSpider&&playerTransformInstance&&spiderInstance&&get_transform&&get_position&&set_position){void* st=get_transform(spiderInstance);if(st){Vector3 sp;get_position(st,&sp);set_position(playerTransformInstance,sp);}teleportMeToSpider=false;}
        if(teleportMeToCrow&&playerTransformInstance&&crowTransformInstance&&get_position&&set_position){Vector3 cp;get_position(crowTransformInstance,&cp);set_position(playerTransformInstance,cp);teleportMeToCrow=false;}
        if(teleportRight&&playerTransformInstance&&get_position&&set_position){Vector3 p;get_position(playerTransformInstance,&p);p.x+=5;set_position(playerTransformInstance,p);teleportRight=false;}
        if(teleportLeft&&playerTransformInstance&&get_position&&set_position){Vector3 p;get_position(playerTransformInstance,&p);p.x-=5;set_position(playerTransformInstance,p);teleportLeft=false;}
        if(teleportUp&&playerTransformInstance&&get_position&&set_position){Vector3 p;get_position(playerTransformInstance,&p);p.y+=5;set_position(playerTransformInstance,p);teleportUp=false;}
        if(teleportDown&&playerTransformInstance&&get_position&&set_position){Vector3 p;get_position(playerTransformInstance,&p);p.y-=5;set_position(playerTransformInstance,p);teleportDown=false;}
        if(teleportMeToRandomPlayer&&playerTransformInstance&&get_position&&set_position){Vector3 p;get_position(playerTransformInstance,&p);p.x+=(rand()%20)-10;p.z+=(rand()%20)-10;set_position(playerTransformInstance,p);teleportMeToRandomPlayer=false;}
        if(teleportRandomPlayerToMe&&playerTransformInstance&&get_position&&set_position){Vector3 mp;get_position(playerTransformInstance,&mp);int r=rand()%4;if(r==0&&grannyTransformInstance)set_position(grannyTransformInstance,mp);else if(r==1&&momSpiderInstance){void* mt=get_transform(momSpiderInstance);if(mt)set_position(mt,mp);}else if(r==2&&spiderInstance){void* st=get_transform(spiderInstance);if(st)set_position(st,mp);}else if(r==3&&crowTransformInstance)set_position(crowTransformInstance,mp);teleportRandomPlayerToMe=false;}
        if(teleportAllPlayersToMe&&playerTransformInstance&&get_position&&set_position){Vector3 mp;get_position(playerTransformInstance,&mp);if(grannyTransformInstance)set_position(grannyTransformInstance,mp);if(momSpiderInstance){void* mt=get_transform(momSpiderInstance);if(mt)set_position(mt,mp);}if(spiderInstance){void* st=get_transform(spiderInstance);if(st)set_position(st,mp);}if(crowTransformInstance)set_position(crowTransformInstance,mp);teleportAllPlayersToMe=false;}
        if(teleportMeToAllPlayers&&playerTransformInstance&&get_position&&set_position){Vector3 avg={0,0,0};int c=0;if(grannyTransformInstance){Vector3 p;get_position(grannyTransformInstance,&p);avg.x+=p.x;avg.y+=p.y;avg.z+=p.z;c++;}if(momSpiderInstance){void* mt=get_transform(momSpiderInstance);if(mt){Vector3 p;get_position(mt,&p);avg.x+=p.x;avg.y+=p.y;avg.z+=p.z;c++;}}if(spiderInstance){void* st=get_transform(spiderInstance);if(st){Vector3 p;get_position(st,&p);avg.x+=p.x;avg.y+=p.y;avg.z+=p.z;c++;}}if(crowTransformInstance){Vector3 p;get_position(crowTransformInstance,&p);avg.x+=p.x;avg.y+=p.y;avg.z+=p.z;c++;}if(c>0){avg.x/=c;avg.y/=c;avg.z/=c;set_position(playerTransformInstance,avg);}teleportMeToAllPlayers=false;}
        if(KillGranny){if(RPC_askMasterToKillGrannyWithShotgun)RPC_askMasterToKillGrannyWithShotgun(i);KillGranny=false;}
        if(UnlimitedAmmo)*(bool*)((uint64_t)i+0x500)=true;
        if(GiveShotgun){*(bool*)((uint64_t)i+0x4EA)=true;if(checkInventory_Func)checkInventory_Func(i);GiveShotgun=false;}
        if(GiveCrossbow){*(bool*)((uint64_t)i+0x4D8)=true;if(checkInventory_Func)checkInventory_Func(i);GiveCrossbow=false;}
        if(GiveSpray){*(bool*)((uint64_t)i+0x4F7)=true;if(checkInventory_Func)checkInventory_Func(i);GiveSpray=false;}
        if(GiveFreezeTrap){*(bool*)((uint64_t)i+0x4FA)=true;if(checkInventory_Func)checkInventory_Func(i);GiveFreezeTrap=false;}
        if(spawnItemToggle&&spawnItemSpinner>0){for(int n=0;n<spawnItemsNumber;n++)spawnItemBySpinner(i,spawnItemSpinner);if(checkInventory_Func)checkInventory_Func(i);spawnItemToggle=false;}
        if(AutoSpawnItems&&spawnItemSpinner>0){for(int n=0;n<spawnItemsNumber;n++)spawnItemBySpinner(i,spawnItemSpinner);if(checkInventory_Func)checkInventory_Func(i);}
        if(Spawnallitems){for(int r=0;r<5;r++)*(bool*)((uint64_t)i+getRandomItemOffset())=true;if(checkInventory_Func)checkInventory_Func(i);}
        if(GiveAllItems){for(int j=0x4D0;j<=0x500;j++)*(bool*)((uint64_t)i+j)=true;}
        if(GodMode){*(bool*)((uint64_t)i+0x98)=true;*(bool*)((uint64_t)i+0x99)=false;*(bool*)((uint64_t)i+0x38B)=false;*(bool*)((uint64_t)i+0x39E)=false;*(bool*)((uint64_t)i+0x3A2)=false;*(bool*)((uint64_t)i+0x3A7)=false;*(bool*)((uint64_t)i+0x399)=false;*(bool*)((uint64_t)i+0x119)=false;*(bool*)((uint64_t)i+0x11A)=false;*(bool*)((uint64_t)i+0xB5)=false;void* bt=*(void**)((uint64_t)i+0x48);if(bt){*(bool*)((uint64_t)bt+0x20)=false;*(bool*)((uint64_t)bt+0x21)=false;*(bool*)((uint64_t)bt+0x7C)=false;*(bool*)((uint64_t)bt+0x7D)=false;*(float*)((uint64_t)bt+0x78)=-2147483647;}}
        if(walkSpeedValue>0)*(float*)((uint64_t)i+0x284)=walkSpeedValue;
        if(crouchSpeedValue>0)*(float*)((uint64_t)i+0x288)=crouchSpeedValue;
    }
    old_playerController_Update(i);
}
int CharacterController_Move_Hook(void* i, Vector3 m) {
    if(i&&noclipMode){void* t=Component_get_transform(i);if(t){Vector3 c=get_position_V3(t);c.x+=m.x*noclipSpeed;c.z+=m.z*noclipSpeed;if(activePlayerControllerInstance){float va=*(float*)((uint64_t)activePlayerControllerInstance+0x68C);float iz=*(float*)((uint64_t)activePlayerControllerInstance+0x61C);while(va>180)va-=360;while(va<-180)va+=360;float ar=va*M_PI/180.0f;float fm=sqrtf(m.x*m.x+m.z*m.z);if(fabs(iz)>0.01f&&fm>0.001f){float ddm=(iz<-0.01f)?-1:1;c.y+=sinf(ar)*fm*noclipSpeed*ddm;}}else c.y+=m.y*noclipSpeed;set_position_V3(t,c);return 0;}}
    return old_CharacterController_Move(i,m);
}

// ============ WEAPON ============
void Update_ShootButton(void* i) {if(i){if(UnlimitedAmmo)orig_toggleShootButtonShotgun(i,true);if(UnlimitedDart)orig_toggleShootButtonArrow(i,true);}old_Update_ShootButton(i);}
void checkSprayUpdate(void* i) {if(i&&UnlimitedSpray){*(int*)((uint64_t)i+0x30)=2147483647;*(bool*)((uint64_t)i+0x34)=false;*(float*)((uint64_t)i+0x38)=-2147483647;}old_checkSprayUpdate(i);}
void ShootArrowUpdate(void* i) {old_ShootArrowUpdate(i);}

// ============ DIFFICULTY ============
float manageGrannyAI_GetGrannyDeathTime(void* i, bool s) {if(grannySpawnTime>0)return(float)grannySpawnTime;if(difficultyLevel>0){switch(difficultyLevel){case 1:return 180;case 2:return 150;case 3:return 120;case 4:return 90;case 5:return 60;case 6:return 45;case 7:return 30;case 8:return 20;case 9:return 10;case 10:return 7;case 11:return 7;case 12:return 7;case 13:return 6;case 14:return 6;case 15:return 5;case 16:return 5;case 17:return 5;case 18:return 4;case 19:return 4;case 20:return 4;case 21:return 3;case 22:return 3;case 23:return 3;case 24:return 2;case 25:return 2;case 26:return 2;case 27:return 1;case 28:return 1;case 29:return 1;case 30:return 1;}}return old_manageGrannyAI_GetGrannyDeathTime(i,s);}

// ============ GRANNY ============
void GrannyFixedUpdate(void* i) {
    grannyInstance=i;if(get_transform)grannyTransformInstance=get_transform(i);
    if(i){
        if(teleportGrannyToMe&&playerTransformInstance&&grannyTransformInstance&&get_position&&set_position){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(grannyTransformInstance,pp);teleportGrannyToMe=false;}
        if(CloneGranny&&grannyInstance&&Object_Instantiate_PosRot){void* go=get_gameObject?get_gameObject(grannyInstance):nullptr;if(go){Vector3 pos;get_position(grannyTransformInstance,&pos);float q[4]={0,0,0,1};clonedGrannyInstance=Object_Instantiate_PosRot(go,pos,q);}CloneGranny=false;}
        if(DestroyCloneGranny&&clonedGrannyInstance&&Object_Destroy){Object_Destroy(clonedGrannyInstance);clonedGrannyInstance=nullptr;DestroyCloneGranny=false;}
        if(StopGranny){*(float*)((uint64_t)i+0x368)=0;*(float*)((uint64_t)i+0x36C)=0;*(float*)((uint64_t)i+0x154)=0;*(float*)((uint64_t)i+0x1EC)=0;}
        else{float cs=*(float*)((uint64_t)i+0x368);if(cs==0){*(float*)((uint64_t)i+0x368)=50;*(float*)((uint64_t)i+0x36C)=25;*(float*)((uint64_t)i+0x154)=5;*(float*)((uint64_t)i+0x1EC)=1;}}
        if(grannySize!=1&&grannyTransformInstance&&set_localScale){Vector3 s;s.x=s.y=s.z=grannySize;set_localScale(grannyTransformInstance,s);}
        if(noDropBearTrap){*(bool*)((uint64_t)i+0x2FF)=false;*(bool*)((uint64_t)i+0x400)=false;*(bool*)((uint64_t)i+0x401)=false;}
        if(noHearingGranny){*(bool*)((uint64_t)i+0x20D)=false;*(bool*)((uint64_t)i+0x21A)=false;*(bool*)((uint64_t)i+0x1D8)=false;*(bool*)((uint64_t)i+0x1D9)=false;}
        if(grannyAttackDist!=1)*(float*)((uint64_t)i+0x154)=grannyAttackDist;
        if(grannyVarSpeed!=1)*(float*)((uint64_t)i+0x368)=grannyVarSpeed;
        if(grannyAnimSpeed!=1)*(float*)((uint64_t)i+0x1EC)=grannyAnimSpeed;
        if(GodMode){*(bool*)((uint64_t)i+0x2FF)=false;*(bool*)((uint64_t)i+0x400)=false;*(bool*)((uint64_t)i+0x401)=false;*(bool*)((uint64_t)i+0x20D)=false;*(bool*)((uint64_t)i+0x21A)=false;}
        if(Invisible){*(bool*)((uint64_t)i+0x1D8)=false;*(bool*)((uint64_t)i+0x1D9)=false;*(bool*)((uint64_t)i+0x20E)=false;*(bool*)((uint64_t)i+0x20F)=false;*(bool*)((uint64_t)i+0x20A)=false;*(bool*)((uint64_t)i+0x20B)=false;*(float*)((uint64_t)i+0x380)=2147483647;void* hm=*(void**)((uint64_t)i+0x340);if(hm)*(bool*)((uint64_t)hm+0x18)=false;void* gs=*(void**)((uint64_t)i+0x290);if(gs)*(bool*)((uint64_t)gs+0x18)=false;void* tm=*(void**)((uint64_t)i+0x338);if(tm)*(bool*)((uint64_t)tm+0x18)=false;}
        if(difficultyLevel>0){float gw,ga,gf,gat;switch(difficultyLevel){case 1:gw=50;ga=25;gf=5;gat=1;break;case 2:gw=100;ga=40;gf=8;gat=1.5f;break;case 3:gw=200;ga=55;gf=10;gat=2;break;case 4:gw=300;ga=65;gf=12;gat=2;break;case 5:gw=400;ga=75;gf=15;gat=2.5f;break;case 6:gw=500;ga=85;gf=18;gat=2.5f;break;case 7:gw=600;ga=95;gf=20;gat=3;break;case 8:gw=700;ga=105;gf=22;gat=3;break;case 9:gw=800;ga=115;gf=25;gat=3.5f;break;case 10:gw=1000;ga=125;gf=28;gat=3.5f;break;case 11:gw=1200;ga=135;gf=30;gat=4;break;case 12:gw=1400;ga=145;gf=32;gat=4;break;case 13:gw=1600;ga=155;gf=34;gat=4.5f;break;case 14:gw=1800;ga=165;gf=36;gat=4.5f;break;case 15:gw=2000;ga=175;gf=38;gat=5;break;case 16:gw=2300;ga=185;gf=40;gat=5;break;case 17:gw=2600;ga=195;gf=42;gat=5.5f;break;case 18:gw=2900;ga=205;gf=44;gat=5.5f;break;case 19:gw=3200;ga=215;gf=46;gat=6;break;case 20:gw=3500;ga=225;gf=48;gat=6;break;case 21:gw=4000;ga=235;gf=50;gat=6.5f;break;case 22:gw=4500;ga=245;gf=52;gat=6.5f;break;case 23:gw=5000;ga=255;gf=54;gat=7;break;case 24:gw=5500;ga=265;gf=56;gat=7;break;case 25:gw=6000;ga=275;gf=58;gat=7.5f;break;case 26:gw=6500;ga=285;gf=60;gat=7.5f;break;case 27:gw=7000;ga=295;gf=62;gat=8;break;case 28:gw=8000;ga=305;gf=64;gat=8.5f;break;case 29:gw=9000;ga=315;gf=66;gat=9;break;case 30:gw=9999;ga=350;gf=70;gat=10;break;}*(float*)((uint64_t)i+0x368)=gw;*(float*)((uint64_t)i+0x36C)=ga;*(float*)((uint64_t)i+0x154)=gf;*(float*)((uint64_t)i+0x314)=gf;*(float*)((uint64_t)i+0x1EC)=gat;}
    }
    old_GrannyFixedUpdate(i);
}

// ============ MOMSPIDER ============
void MomFixedUpdate(void* i) {
    momSpiderInstance=i;
    if(i){
        if(teleportMomSpiderToMe&&playerTransformInstance&&get_transform&&get_position&&set_position){void* mt=get_transform(i);if(mt){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(mt,pp);}teleportMomSpiderToMe=false;}
        if(CloneMomSpider&&momSpiderInstance&&Object_Instantiate_PosRot){void* go=get_gameObject?get_gameObject(momSpiderInstance):nullptr;if(go){void* mt=get_transform(momSpiderInstance);if(mt){Vector3 pos;get_position(mt,&pos);float q[4]={0,0,0,1};clonedMomSpiderInstance=Object_Instantiate_PosRot(go,pos,q);}}CloneMomSpider=false;}
        if(DestroyCloneMomSpider&&clonedMomSpiderInstance&&Object_Destroy){Object_Destroy(clonedMomSpiderInstance);clonedMomSpiderInstance=nullptr;DestroyCloneMomSpider=false;}
        if(KillMomSpider&&momSpiderInstance&&Object_Destroy){void* go=get_gameObject?get_gameObject(momSpiderInstance):nullptr;if(go)Object_Destroy(go);KillMomSpider=false;}
        if(StopMomSpider){*(float*)((uint64_t)i+0x58)=0;*(float*)((uint64_t)i+0x60)=0;*(float*)((uint64_t)i+0x54)=0;}
        else{float cs=*(float*)((uint64_t)i+0x58);if(cs==0){*(float*)((uint64_t)i+0x58)=25;*(float*)((uint64_t)i+0x60)=15;*(float*)((uint64_t)i+0x54)=5;}}
        if(momSpiderSize!=1){void* mt=get_transform(i);if(mt&&set_localScale){Vector3 s;s.x=s.y=s.z=momSpiderSize;set_localScale(mt,s);}}
        if(noHearingMom){*(bool*)((uint64_t)i+0x74)=false;*(bool*)((uint64_t)i+0x75)=false;}
        if(momAttackDist!=1)*(float*)((uint64_t)i+0x54)=momAttackDist;
        if(momVarSpeed!=1)*(float*)((uint64_t)i+0x58)=momVarSpeed;
        if(momAnimSpeed!=1)*(float*)((uint64_t)i+0x60)=momAnimSpeed;
        if(Invisible){*(bool*)((uint64_t)i+0x74)=false;*(bool*)((uint64_t)i+0x75)=false;*(bool*)((uint64_t)i+0xC8)=false;*(float*)((uint64_t)i+0x88)=-2147483647;*(float*)((uint64_t)i+0xC4)=-2147483647;*(float*)((uint64_t)i+0xC0)=2147483647;}
        if(difficultyLevel>0){float mc,ma,mw;switch(difficultyLevel){case 1:mc=25;ma=15;mw=5;break;case 2:mc=50;ma=25;mw=8;break;case 3:mc=100;ma=40;mw=10;break;case 4:mc=150;ma=55;mw=12;break;case 5:mc=200;ma=65;mw=15;break;case 6:mc=250;ma=75;mw=18;break;case 7:mc=300;ma=85;mw=20;break;case 8:mc=350;ma=95;mw=22;break;case 9:mc=400;ma=105;mw=25;break;case 10:mc=500;ma=115;mw=28;break;case 11:mc=600;ma=125;mw=30;break;case 12:mc=700;ma=135;mw=32;break;case 13:mc=800;ma=145;mw=34;break;case 14:mc=900;ma=155;mw=36;break;case 15:mc=1000;ma=165;mw=38;break;case 16:mc=1200;ma=175;mw=40;break;case 17:mc=1400;ma=185;mw=42;break;case 18:mc=1600;ma=195;mw=44;break;case 19:mc=1800;ma=205;mw=46;break;case 20:mc=2000;ma=215;mw=48;break;case 21:mc=2500;ma=225;mw=50;break;case 22:mc=3000;ma=235;mw=52;break;case 23:mc=3500;ma=245;mw=54;break;case 24:mc=4000;ma=255;mw=56;break;case 25:mc=4500;ma=265;mw=58;break;case 26:mc=5000;ma=275;mw=60;break;case 27:mc=6000;ma=285;mw=62;break;case 28:mc=7000;ma=295;mw=64;break;case 29:mc=8000;ma=305;mw=66;break;case 30:mc=9999;ma=350;mw=70;break;}*(float*)((uint64_t)i+0x58)=mc;*(float*)((uint64_t)i+0x60)=ma;*(float*)((uint64_t)i+0x54)=mw;}
    }
    old_MomFixedUpdate(i);
}

// ============ SPIDER ============
void SpiderControlUpdate(void* i) {
    spiderInstance=i;
    if(i){
        if(teleportSpiderToMe&&playerTransformInstance&&get_transform&&get_position&&set_position){void* st=get_transform(i);if(st){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(st,pp);}teleportSpiderToMe=false;}
        if(CloneSpider&&spiderInstance&&Object_Instantiate_PosRot){void* go=get_gameObject?get_gameObject(spiderInstance):nullptr;if(go){void* st=get_transform(spiderInstance);if(st){Vector3 pos;get_position(st,&pos);float q[4]={0,0,0,1};clonedSpiderInstance=Object_Instantiate_PosRot(go,pos,q);}}CloneSpider=false;}
        if(DestroyCloneSpider&&clonedSpiderInstance&&Object_Destroy){Object_Destroy(clonedSpiderInstance);clonedSpiderInstance=nullptr;DestroyCloneSpider=false;}
        if(KillSpider&&spiderInstance&&Object_Destroy){void* go=get_gameObject?get_gameObject(spiderInstance):nullptr;if(go)Object_Destroy(go);KillSpider=false;}
        if(StopSpider){*(float*)((uint64_t)i+0x40)=0;*(float*)((uint64_t)i+0x44)=0;}
        else{float cs=*(float*)((uint64_t)i+0x40);if(cs==0){*(float*)((uint64_t)i+0x40)=25;*(float*)((uint64_t)i+0x44)=15;}}
        if(spiderSize!=1){void* st=get_transform(i);if(st&&set_localScale){Vector3 s;s.x=s.y=s.z=spiderSize;set_localScale(st,s);}}
        if(noHearingSpider){*(bool*)((uint64_t)i+0x90)=false;*(bool*)((uint64_t)i+0x91)=false;}
        if(spiderAttackDist!=1)*(float*)((uint64_t)i+0x44)=spiderAttackDist;
        if(spiderVarSpeed!=1)*(float*)((uint64_t)i+0x40)=spiderVarSpeed;
        if(spiderAnimSpeed!=1)*(float*)((uint64_t)i+0x40)=spiderAnimSpeed;
        if(Invisible){*(bool*)((uint64_t)i+0x90)=false;*(bool*)((uint64_t)i+0x91)=false;*(bool*)((uint64_t)i+0x92)=false;*(bool*)((uint64_t)i+0x28)=false;*(float*)((uint64_t)i+0x40)=-2147483647;}
        if(difficultyLevel>0){float ss,sa;switch(difficultyLevel){case 1:ss=25;sa=15;break;case 2:ss=50;sa=25;break;case 3:ss=100;sa=40;break;case 4:ss=150;sa=55;break;case 5:ss=200;sa=65;break;case 6:ss=250;sa=75;break;case 7:ss=300;sa=85;break;case 8:ss=350;sa=95;break;case 9:ss=400;sa=105;break;case 10:ss=500;sa=115;break;case 11:ss=600;sa=125;break;case 12:ss=700;sa=135;break;case 13:ss=800;sa=145;break;case 14:ss=900;sa=155;break;case 15:ss=1000;sa=165;break;case 16:ss=1200;sa=170;break;case 17:ss=1400;sa=172;break;case 18:ss=1600;sa=174;break;case 19:ss=1800;sa=175;break;case 20:ss=2000;sa=175;break;case 21:ss=2500;sa=175;break;case 22:ss=3000;sa=175;break;case 23:ss=3500;sa=175;break;case 24:ss=4000;sa=175;break;case 25:ss=4500;sa=175;break;case 26:ss=5000;sa=175;break;case 27:ss=6000;sa=175;break;case 28:ss=7000;sa=175;break;case 29:ss=8000;sa=175;break;case 30:ss=9999;sa=175;break;}*(float*)((uint64_t)i+0x40)=ss;*(float*)((uint64_t)i+0x44)=sa;}
    }
    old_SpiderControlUpdate(i);
}

// ============ CROW ============
void CrowUpdate_Hook(void* i) {
    crowInstance=i;if(get_transform)crowTransformInstance=get_transform(i);
    if(i){
        if(teleportCrowToMe&&playerTransformInstance&&crowTransformInstance&&get_position&&set_position){Vector3 pp;get_position(playerTransformInstance,&pp);set_position(crowTransformInstance,pp);teleportCrowToMe=false;}
        if(CloneCrow&&crowInstance&&Object_Instantiate_PosRot){void* go=get_gameObject?get_gameObject(crowInstance):nullptr;if(go){Vector3 pos;get_position(crowTransformInstance,&pos);float q[4]={0,0,0,1};clonedCrowInstance=Object_Instantiate_PosRot(go,pos,q);}CloneCrow=false;}
        if(DestroyCloneCrow&&clonedCrowInstance&&Object_Destroy){Object_Destroy(clonedCrowInstance);clonedCrowInstance=nullptr;DestroyCloneCrow=false;}
        if(KillCrow&&crowInstance&&Object_Destroy){void* go=get_gameObject?get_gameObject(crowInstance):nullptr;if(go)Object_Destroy(go);KillCrow=false;}
        if(StopCrow){*(float*)((uint64_t)i+0x48)=2147483647;}
        else{float cs=*(float*)((uint64_t)i+0x48);if(cs>=2147483647){*(float*)((uint64_t)i+0x48)=1;}}
        if(crowSize!=1&&crowTransformInstance&&set_localScale){Vector3 s;s.x=s.y=s.z=crowSize;set_localScale(crowTransformInstance,s);}
        if(noHearingCrow){*(float*)((uint64_t)i+0x48)=-2147483647;}
        if(crowAttackDist!=1)*(float*)((uint64_t)i+0x48)=crowAttackDist;
        if(crowVarSpeed!=1)*(float*)((uint64_t)i+0x48)=crowVarSpeed;
        if(crowAnimSpeed!=1)*(float*)((uint64_t)i+0x48)=crowAnimSpeed;
    }
    old_CrowUpdate(i);
}

// ============ RAT ============
void RatUpdate_Hook(void* i) {
    ratInstance=i;
    if(i){
        if(CloneRat&&ratInstance&&Object_Instantiate_PosRot){void* go=get_gameObject?get_gameObject(ratInstance):nullptr;if(go){void* rt=get_transform(ratInstance);if(rt){Vector3 pos;get_position(rt,&pos);float q[4]={0,0,0,1};clonedRatInstance=Object_Instantiate_PosRot(go,pos,q);}}CloneRat=false;}
        if(DestroyCloneRat&&clonedRatInstance&&Object_Destroy){Object_Destroy(clonedRatInstance);clonedRatInstance=nullptr;DestroyCloneRat=false;}
        if(KillRat&&ratInstance&&Object_Destroy){void* go=get_gameObject?get_gameObject(ratInstance):nullptr;if(go)Object_Destroy(go);KillRat=false;}
        if(StopRat){*(float*)((uint64_t)i+0x48)=2147483647;*(float*)((uint64_t)i+0x4C)=2147483647;*(bool*)((uint64_t)i+0x50)=true;}
        else{float ct=*(float*)((uint64_t)i+0x48);if(ct>=2147483647){*(float*)((uint64_t)i+0x48)=0;*(float*)((uint64_t)i+0x4C)=0;*(bool*)((uint64_t)i+0x50)=false;}}
        if(ratSize!=1){void* rt=get_transform(ratInstance);if(rt&&set_localScale){Vector3 s;s.x=s.y=s.z=ratSize;set_localScale(rt,s);}}
        if(noHearingRat){*(float*)((uint64_t)i+0x48)=-2147483647;}
        if(ratAttackDist!=1)*(float*)((uint64_t)i+0x54)=ratAttackDist;
        if(ratVarSpeed!=1)*(float*)((uint64_t)i+0x48)=ratVarSpeed;
        if(ratAnimSpeed!=1)*(float*)((uint64_t)i+0x24)=ratAnimSpeed;
    }
    old_RatUpdate(i);
}

// ============ GODMODE DEATH ============
void die(void* i) {if(GodMode)return;old_die(i);}
void RPC_playerDies(void* i, void* k) {if(GodMode)return;old_RPC_playerDies(i,k);}
void RPC_playerDiesBySpider(void* i) {if(GodMode)return;old_RPC_playerDiesBySpider(i);}
void RPC_playerKilledByAI(void* i) {if(GodMode)return;old_RPC_playerKilledByAI(i);}
void RPC_playerDiesBySpiderAIGranny(void* i) {if(GodMode)return;old_RPC_playerDiesBySpiderAIGranny(i);}
void dieByLava(void* i) {if(GodMode)return;old_dieByLava(i);}
void RPC_playerExplosionDeath(void* i) {if(GodMode)return;old_RPC_playerExplosionDeath(i);}
void onDiedByOpeningBox(void* i) {if(GodMode)return;old_onDiedByOpeningBox(i);}
void RPC_onSpiderMomKilledYou(void* i) {if(GodMode)return;old_RPC_onSpiderMomKilledYou(i);}
void crowAttackPlayer(void* i) {if(GodMode)return;old_crowAttackPlayer(i);}
void playerHitBearTrap(void* i) {if(GodMode)return;old_playerHitBearTrap(i);}
void hitByArrow(void* i) {if(GodMode)return;old_hitByArrow(i);}
void RPC_playPlayerBurningOnDeath(void* i) {if(GodMode)return;old_RPC_playPlayerBurningOnDeath(i);}
void playerSpiderBittenDead(void* i) {if(GodMode)return;old_playerSpiderBittenDead(i);}
void playerSpiderBittenDeadAIGranny(void* i) {if(GodMode)return;old_playerSpiderBittenDeadAIGranny(i);}
void AIDie(void* i) {if(GodMode)return;old_AIDie(i);}
void playerManager_die(void* i) {if(GodMode)return;old_playerManager_die(i);}
void dieAI(void* i) {if(GodMode)return;old_dieAI(i);}
void ratBite_OnTriggerEnter(void* i, void* o) {if(GodMode)return;old_ratBite_OnTriggerEnter(i,o);}
void momKillPlayer(void* i) {if(GodMode)return;old_momKillPlayer(i);}
void playerExplode(void* i) {if(GodMode)return;old_playerExplode(i);}

// ============ GODMODE FALL ============
void playerFallDead(void* i) {if(GodMode)return;old_playerFallDead(i);}
void playerFalling(void* i) {if(GodMode)return;old_playerFalling(i);}
void noFall(void* i) {if(GodMode)return;old_noFall(i);}
void waitToResetBack(void* i) {if(GodMode)return;old_waitToResetBack(i);}
void checkFall_Hook(void* i) {if(GodMode)return;old_checkFall(i);}
void PlayerLandBad_Hook(void* i) {if(GodMode)return;old_PlayerLandBad(i);}
void afterPlayerFallHurt_Hook(void* i) {if(GodMode)return;old_afterPlayerFallHurt(i);}
void fallingDead(void* i) {if(GodMode)return;old_fallingDead(i);}
void RPC_sendMessageFallen(void* i, void* v) {if(GodMode)return;old_RPC_sendMessageFallen(i,v);}
void triggerFellEvent(void* i) {if(GodMode)return;old_triggerFellEvent(i);}
void triggerFallFloor_OnTriggerEnter(void* i, void* o) {if(GodMode)return;old_triggerFallFloor_OnTriggerEnter(i,o);}

// ============ INVISIBLE ============
void EnemyEyeUpdate(void* i) {if(Invisible)*(float*)((uint64_t)i+0x48)=-2147483647;old_EnemyEyeUpdate(i);}
void momOnPlayerSeen(void* i) {if(Invisible)return;old_momOnPlayerSeen(i);}
void momCheckForPlayers(void* i) {if(Invisible)return;old_momCheckForPlayers(i);}
void RPC_playMomRoar(void* i) {if(Invisible)return;old_RPC_playMomRoar(i);}
void momParseForPlayers(void* i) {if(Invisible)return;old_momParseForPlayers(i);}
void spider_funcHuntPlayer(void* i) {if(Invisible)return;old_spider_funcHuntPlayer(i);}

// ============ FEATURE LIST ============
jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;
    const char *features[] = {
        OBFUSCATE("Category_Player"),OBFUSCATE("1_ButtonOnOff_GodMode"),OBFUSCATE("2_ButtonOnOff_Invisible"),
        OBFUSCATE("3_ButtonOnOff_NoClip"),OBFUSCATE("4_InputValue_NoClip Speed"),OBFUSCATE("5_InputValue_Walk Speed"),OBFUSCATE("6_InputValue_Crouch Speed"),
        OBFUSCATE("Category_Weapon"),OBFUSCATE("7_ButtonOnOff_Infinity Ammo"),OBFUSCATE("8_ButtonOnOff_Infinity Dart"),OBFUSCATE("9_ButtonOnOff_Infinity Spray"),
        OBFUSCATE("10_Button_Give Shotgun"),OBFUSCATE("11_Button_Give Crossbow"),OBFUSCATE("12_Button_Give Spray"),OBFUSCATE("13_Button_Give FreezeTrap"),
        OBFUSCATE("Collapse_Play As"),OBFUSCATE("CollapseAdd_14_ButtonOnOff_Play As Granny"),OBFUSCATE("CollapseAdd_15_ButtonOnOff_Play As MomSpider"),
        OBFUSCATE("CollapseAdd_16_ButtonOnOff_Play As Spider"),OBFUSCATE("CollapseAdd_17_ButtonOnOff_Play As Crow"),OBFUSCATE("CollapseAdd_18_ButtonOnOff_Play As Rat"),
        OBFUSCATE("Category_Inventory"),OBFUSCATE("19_Spinner_Spawn Item_Off,Game Logic [OldShotgunLoaded],Random,Cutting Pliers,Vase,Hammer,Vase 2,Safe Key,House Key,Hang Lock Key,Dpad Lock Code,Crossbow,Arrow,ArrowOK,Weapon Key,Screwdriver,Planka Walk,Battery,Painting Piece 1,Painting Piece 2,Painting Piece 3,Painting Piece 4,Play House Key,Melon,Teddy,Cog 1,Cog 2,Message,Winch Handle,Shotgun,Shotgun Part 1,Shotgun Part 2,Shotgun Part 3,Shotgun Loaded,Car Key,Top Plock,Car Battery,Gas Can,Wrench,Spark Plug,Meat,Special Key,Book,Pepper Spray,Remote Control,Bird Seed,Freeze Trap,Spider Key,Chain Cutter,Rusty Padlock Key,Wheel Crank,Wooden Stick"),
        OBFUSCATE("20_Button_Spawn Item"),OBFUSCATE("21_Toggle_Auto Spawn Items"),OBFUSCATE("22_Toggle_Spawn All Items"),OBFUSCATE("23_Toggle_Give All Items"),OBFUSCATE("24_InputValue_Spawn Items Number"),
        OBFUSCATE("Category_Difficulty"),OBFUSCATE("25_Spinner_Difficulty_Off,Practice,Very Easy Chill,Easy,Normal,Hard,Extreme,Extreme+,Insane,Impossible,Impossible+,Impossible+++,Madness,Crazy,Grandpossible,GrandImpossible,Hellpossible,Furios,Hallow,Prime,PrimePlus,Prime+++,Prime X,Death,Death forius,Death Walk,Abyss,Hellimonios,Death stare,SuperFast,(MostHarder) Infernus"),OBFUSCATE("26_InputValue_Granny Spawn Time"),
        OBFUSCATE("Collapse_Teleportation"),OBFUSCATE("CollapseAdd_27_Spinner_Location_Select Location,Basement 1,Basement 2,Basement 3,Spray Room,Main Door,Garage,Start Room Outside,Attic,Spider Room,Sewer,Smoke Room,Sewer 2,Water Tunnel,Under Stairs,Hall Ways Sewer,Sewer Door Escape,Elevator,Garage Exit,Sewer Escape Outside,Hall Way 2 Sewer,Bedroom 1,Bedroom 2,Bedroom 3,Sewer Extrans,Weapon Room,Hall Way House,Water Tunnel Room,Crow Room,Kitchen,Backyard,Room Backyard"),OBFUSCATE("CollapseAdd_28_Button_Teleport"),
        OBFUSCATE("CollapseAdd_Category_Custom Teleport"),OBFUSCATE("CollapseAdd_29_InputValue_Pos X"),OBFUSCATE("CollapseAdd_30_InputValue_Pos Y"),OBFUSCATE("CollapseAdd_31_InputValue_Pos Z"),OBFUSCATE("CollapseAdd_32_Button_Teleport To Custom"),
        OBFUSCATE("CollapseAdd_Category_Teleport Entity"),OBFUSCATE("CollapseAdd_33_Button_Teleport Me To Granny"),OBFUSCATE("CollapseAdd_34_Button_Teleport Granny To Me"),
        OBFUSCATE("CollapseAdd_35_Button_Teleport Me To MomSpider"),OBFUSCATE("CollapseAdd_36_Button_Teleport MomSpider To Me"),
        OBFUSCATE("CollapseAdd_37_Button_Teleport Me To Spider"),OBFUSCATE("CollapseAdd_38_Button_Teleport Spider To Me"),
        OBFUSCATE("CollapseAdd_39_Button_Teleport Me To Crow"),OBFUSCATE("CollapseAdd_40_Button_Teleport Crow To Me"),
        OBFUSCATE("CollapseAdd_Category_Teleport Directional"),OBFUSCATE("CollapseAdd_41_Button_Teleport Right (+5)"),OBFUSCATE("CollapseAdd_42_Button_Teleport Left (-5)"),
        OBFUSCATE("CollapseAdd_43_Button_Teleport Up (+5)"),OBFUSCATE("CollapseAdd_44_Button_Teleport Down (-5)"),
        OBFUSCATE("CollapseAdd_Category_Teleport Player"),OBFUSCATE("CollapseAdd_45_Button_Teleport Me To Random Player"),OBFUSCATE("CollapseAdd_46_Button_Teleport Random Player To Me"),
        OBFUSCATE("CollapseAdd_47_Button_Teleport All Players To Me"),OBFUSCATE("CollapseAdd_48_Button_Teleport Me To All Players"),
        OBFUSCATE("Collapse_Enemy"),
        OBFUSCATE("CollapseAdd_Category_Granny"),OBFUSCATE("CollapseAdd_49_Button_Clone"),OBFUSCATE("CollapseAdd_50_Button_Destroy Clone"),OBFUSCATE("CollapseAdd_51_Button_Kill"),
        OBFUSCATE("CollapseAdd_52_ButtonOnOff_Stop"),OBFUSCATE("CollapseAdd_53_ButtonOnOff_No Drop BearTrap"),OBFUSCATE("CollapseAdd_54_ButtonOnOff_Doesn't Hear"),
        OBFUSCATE("CollapseAdd_55_InputValue_Attack Distance"),OBFUSCATE("CollapseAdd_56_InputValue_Var Speed"),OBFUSCATE("CollapseAdd_57_InputValue_Anim Speed"),OBFUSCATE("CollapseAdd_58_InputValue_Size"),
        OBFUSCATE("CollapseAdd_Category_MomSpider"),OBFUSCATE("CollapseAdd_59_Button_Clone"),OBFUSCATE("CollapseAdd_60_Button_Destroy Clone"),OBFUSCATE("CollapseAdd_61_Button_Kill"),
        OBFUSCATE("CollapseAdd_62_ButtonOnOff_Stop"),OBFUSCATE("CollapseAdd_63_ButtonOnOff_Doesn't Hear"),
        OBFUSCATE("CollapseAdd_64_InputValue_Attack Distance"),OBFUSCATE("CollapseAdd_65_InputValue_Var Speed"),OBFUSCATE("CollapseAdd_66_InputValue_Anim Speed"),OBFUSCATE("CollapseAdd_67_InputValue_Size"),
        OBFUSCATE("CollapseAdd_Category_Spider"),OBFUSCATE("CollapseAdd_68_Button_Clone"),OBFUSCATE("CollapseAdd_69_Button_Destroy Clone"),OBFUSCATE("CollapseAdd_70_Button_Kill"),
        OBFUSCATE("CollapseAdd_71_ButtonOnOff_Stop"),OBFUSCATE("CollapseAdd_72_ButtonOnOff_Doesn't Hear"),
        OBFUSCATE("CollapseAdd_73_InputValue_Attack Distance"),OBFUSCATE("CollapseAdd_74_InputValue_Var Speed"),OBFUSCATE("CollapseAdd_75_InputValue_Anim Speed"),OBFUSCATE("CollapseAdd_76_InputValue_Size"),
        OBFUSCATE("CollapseAdd_Category_Crow"),OBFUSCATE("CollapseAdd_77_Button_Clone"),OBFUSCATE("CollapseAdd_78_Button_Destroy Clone"),OBFUSCATE("CollapseAdd_79_Button_Kill"),
        OBFUSCATE("CollapseAdd_80_ButtonOnOff_Stop"),OBFUSCATE("CollapseAdd_81_ButtonOnOff_Doesn't Hear"),
        OBFUSCATE("CollapseAdd_82_InputValue_Attack Distance"),OBFUSCATE("CollapseAdd_83_InputValue_Var Speed"),OBFUSCATE("CollapseAdd_84_InputValue_Anim Speed"),OBFUSCATE("CollapseAdd_85_InputValue_Size"),
        OBFUSCATE("CollapseAdd_Category_Rat"),OBFUSCATE("CollapseAdd_86_Button_Clone"),OBFUSCATE("CollapseAdd_87_Button_Destroy Clone"),OBFUSCATE("CollapseAdd_88_Button_Kill"),
        OBFUSCATE("CollapseAdd_89_ButtonOnOff_Stop"),OBFUSCATE("CollapseAdd_90_ButtonOnOff_Doesn't Hear"),
        OBFUSCATE("CollapseAdd_91_InputValue_Attack Distance"),OBFUSCATE("CollapseAdd_92_InputValue_Var Speed"),OBFUSCATE("CollapseAdd_93_InputValue_Anim Speed"),OBFUSCATE("CollapseAdd_94_InputValue_Size"),
        OBFUSCATE("Collapse_Server"),
        OBFUSCATE("95_CollapseAdd_Toggle_Force Master Client"),OBFUSCATE("96_CollapseAdd_Toggle_True_Optimized Ping (1-2ms)"),
        OBFUSCATE("97_CollapseAdd_Toggle_True_Anti-Timeout"),OBFUSCATE("98_CollapseAdd_Toggle_True_Anti-Kick"),
        OBFUSCATE("99_CollapseAdd_Toggle_True_Anti-Disconnect"),OBFUSCATE("100_CollapseAdd_Toggle_True_Anti-Ban"),
        OBFUSCATE("101_CollapseAdd_Toggle_True_Auto-Fix Network"),OBFUSCATE("102_CollapseAdd_Spinner_Kick Player_None"),OBFUSCATE("103_CollapseAdd_Button_Kick Player"),
        OBFUSCATE("104_CollapseAdd_InputValue_Custom Ping"),OBFUSCATE("105_CollapseAdd_InputValue_Max Players"),OBFUSCATE("106_CollapseAdd_InputValue_Spoof Player Count"),
        OBFUSCATE("RichTextView_Game: Cursed House Multiplayer"),OBFUSCATE("RichTextView_Version: v1.7.4"),
        OBFUSCATE("RichTextView_Developer: Sussy Baka LLC / DVloper"),OBFUSCATE("RichTextView_Mod Menu: LGL Team v4.0"),
        OBFUSCATE("RichTextView_Date: 2026"),OBFUSCATE("RichTextView_Credit: Dae4ks5aeb"),OBFUSCATE("ButtonLink_Youtube_https://youtube.com/@dae4ks5aeb")
    };
    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")), env->NewStringUTF(""));
    for (int i = 0; i < Total_Feature; i++) env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));
    return (ret);
}

// ============ CHANGES HANDLER ============
void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value, jlong Lvalue, jboolean boolean, jstring text) {
    switch (featNum) {
        case 1: GodMode = boolean; break; case 2: Invisible = boolean; break;
        case 3: noclipMode = boolean; break; case 4: noclipSpeed = (float)value; if (noclipSpeed < 1) noclipSpeed = 1; if (noclipSpeed > 5) noclipSpeed = 5; break;
        case 5: walkSpeedValue = (float)value; break; case 6: crouchSpeedValue = (float)value; break;
        case 7: UnlimitedAmmo = boolean; break; case 8: UnlimitedDart = boolean; break; case 9: UnlimitedSpray = boolean; break;
        case 10: GiveShotgun = true; break; case 11: GiveCrossbow = true; break; case 12: GiveSpray = true; break; case 13: GiveFreezeTrap = true; break;
        case 14: PlayAsGranny = boolean; break; case 15: PlayAsMomSpider = boolean; break; case 16: PlayAsSpider = boolean; break;
        case 17: PlayAsCrow = boolean; break; case 18: PlayAsRat = boolean; break;
        case 19: spawnItemSpinner = value; break; case 20: spawnItemToggle = true; break;
        case 21: AutoSpawnItems = boolean; break; case 22: Spawnallitems = boolean; break;
        case 23: GiveAllItems = boolean; break; case 24: spawnItemsNumber = value; if (spawnItemsNumber < 1) spawnItemsNumber = 1; break;
        case 25: difficultyLevel = value; break; case 26: grannySpawnTime = value; if (grannySpawnTime < 0) grannySpawnTime = 0; break;
        case 27: teleportSpinner = value; break;
        case 28: if (playerTransformInstance != NULL && teleportSpinner > 0) TeleportPlayerToLocation(playerTransformInstance, teleportSpinner); break;
        case 29: customPosX = (float)value; break; case 30: customPosY = (float)value; break; case 31: customPosZ = (float)value; break;
        case 32: if (playerTransformInstance != NULL) TeleportPlayerToCustom(playerTransformInstance, customPosX, customPosY, customPosZ); break;
        case 33: teleportToGranny = true; break; case 34: teleportGrannyToMe = true; break;
        case 35: teleportMeToMomSpider = true; break; case 36: teleportMomSpiderToMe = true; break;
        case 37: teleportMeToSpider = true; break; case 38: teleportSpiderToMe = true; break;
        case 39: teleportMeToCrow = true; break; case 40: teleportCrowToMe = true; break;
        case 41: teleportRight = true; break; case 42: teleportLeft = true; break;
        case 43: teleportUp = true; break; case 44: teleportDown = true; break;
        case 45: teleportMeToRandomPlayer = true; break; case 46: teleportRandomPlayerToMe = true; break;
        case 47: teleportAllPlayersToMe = true; break; case 48: teleportMeToAllPlayers = true; break;
        case 49: CloneGranny = true; break; case 50: DestroyCloneGranny = true; break; case 51: KillGranny = true; break;
        case 52: StopGranny = boolean; break; case 53: noDropBearTrap = boolean; break; case 54: noHearingGranny = boolean; break;
        case 55: grannyAttackDist = (float)value; break; case 56: grannyVarSpeed = (float)value; break;
        case 57: grannyAnimSpeed = (float)value; break; case 58: grannySize = (float)value; if (grannySize < 1) grannySize = 1; if (grannySize > 100) grannySize = 100; break;
        case 59: CloneMomSpider = true; break; case 60: DestroyCloneMomSpider = true; break; case 61: KillMomSpider = true; break;
        case 62: StopMomSpider = boolean; break; case 63: noHearingMom = boolean; break;
        case 64: momAttackDist = (float)value; break; case 65: momVarSpeed = (float)value; break;
        case 66: momAnimSpeed = (float)value; break; case 67: momSpiderSize = (float)value; if (momSpiderSize < 1) momSpiderSize = 1; if (momSpiderSize > 100) momSpiderSize = 100; break;
        case 68: CloneSpider = true; break; case 69: DestroyCloneSpider = true; break; case 70: KillSpider = true; break;
        case 71: StopSpider = boolean; break; case 72: noHearingSpider = boolean; break;
        case 73: spiderAttackDist = (float)value; break; case 74: spiderVarSpeed = (float)value; break;
        case 75: spiderAnimSpeed = (float)value; break; case 76: spiderSize = (float)value; if (spiderSize < 1) spiderSize = 1; if (spiderSize > 100) spiderSize = 100; break;
        case 77: CloneCrow = true; break; case 78: DestroyCloneCrow = true; break; case 79: KillCrow = true; break;
        case 80: StopCrow = boolean; break; case 81: noHearingCrow = boolean; break;
        case 82: crowAttackDist = (float)value; break; case 83: crowVarSpeed = (float)value; break;
        case 84: crowAnimSpeed = (float)value; break; case 85: crowSize = (float)value; if (crowSize < 1) crowSize = 1; if (crowSize > 100) crowSize = 100; break;
        case 86: CloneRat = true; break; case 87: DestroyCloneRat = true; break; case 88: KillRat = true; break;
        case 89: StopRat = boolean; break; case 90: noHearingRat = boolean; break;
        case 91: ratAttackDist = (float)value; break; case 92: ratVarSpeed = (float)value; break;
        case 93: ratAnimSpeed = (float)value; break; case 94: ratSize = (float)value; if (ratSize < 1) ratSize = 1; if (ratSize > 100) ratSize = 100; break;
        case 95: ForceMaster = boolean; break; case 96: OptimizedPing = boolean; break;
        case 97: AntiTimeout = boolean; break; case 98: AntiKick = boolean; break;
        case 99: AntiDisconnect = boolean; break; case 100: AntiBan = boolean; break;
        case 101: AutoFix = boolean; break; case 102: kickPlayerSpinner = value; break;
        case 103: KickPlayerButton = boolean; break; case 104: customPingValue = value; break;
        case 105: customMaxPlayers = value; break; case 106: customPlayerCount = value; break;
    }
}

// ============ TARGET LIB ============
#define targetLibName OBFUSCATE("libil2cpp.so")
ElfScanner g_il2cppELF;

// ============ HACK THREAD ============
void *hack_thread(void *) {
    LOGI(OBFUSCATE("pthread created"));
    do { sleep(1); g_il2cppELF = ElfScanner::createWithPath(targetLibName); } while (!g_il2cppELF.isValid());
    LOGI(OBFUSCATE("%s has been loaded"), (const char *) targetLibName);

#if defined(__aarch64__)
    set_position = (void (*)(void*, Vector3))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FF20DC")));
    get_position = (void (*)(void*, Vector3*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FF20A8")));
    get_transform = (void* (*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FE0348")));
    get_localScale = (Vector3 (*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FF27CC")));
    set_localScale = (void (*)(void*, Vector3))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FF28A4")));
    get_gameObject = (void*(*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FE0418")));
    Object_Instantiate = (void*(*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FE9E60")));
    Object_Instantiate_PosRot = (void*(*)(void*, Vector3, void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FE982C")));
    Object_Destroy = (void (*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FEA3A8")));
    Component_get_transform = (void*(*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FE0348")));
    get_position_V3 = (Vector3 (*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FF2004")));
    set_position_V3 = (void (*)(void*, Vector3))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x3FF20DC")));
    orig_toggleShootButtonShotgun = (toggleShootButtonShotgun_t)getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x20546CC")));
    orig_toggleShootButtonArrow = (toggleShootButtonArrow_t)getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x20546E8")));
    orig_toggleMasterClientOptionsWindow = (toggleMasterClientOptionsWindow_t)getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x200D17C")));
    checkInventory_Func = (void(*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x201EDEC")));
    RPC_askMasterToKillGrannyWithShotgun = (void(*)(void*))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x20316BC")));
    RemovePlayer = (void(*)(void*, int))getAbsoluteAddress(targetLibName, str2Offset(OBFUSCATE("0x372C720")));

    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202E294")), playerController_Update_Hook, old_playerController_Update);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x405A018")), CharacterController_Move_Hook, old_CharacterController_Move);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2054EB0")), Update_ShootButton, old_Update_ShootButton);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x204991C")), checkSprayUpdate, old_checkSprayUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2070B1C")), ShootArrowUpdate, old_ShootArrowUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1FF8F80")), manageGrannyAI_GetGrannyDeathTime, old_manageGrannyAI_GetGrannyDeathTime);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1FE50D4")), GrannyFixedUpdate, old_GrannyFixedUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x201523C")), MomFixedUpdate, old_MomFixedUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x206A944")), SpiderControlUpdate, old_SpiderControlUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1F45F20")), CrowUpdate_Hook, old_CrowUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x204512C")), RatUpdate_Hook, old_RatUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x372C004")), RoomInfo_InternalCacheProperties, old_RoomInfo_InternalCacheProperties);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3728CAC")), get_IsMasterClient, old_get_IsMasterClient);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x205C970")), OnMasterClientSwitched, old_OnMasterClientSwitched);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3739AE8")), GetPing, old_GetPing);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3736908")), set_KeepAliveInBackground, old_set_KeepAliveInBackground);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3739414")), Disconnect, old_Disconnect);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3739C7C")), CloseConnectionHook, old_CloseConnectionHook);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3371E9C")), AntiCheatAwake, old_AntiCheatAwake);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3372BF8")), ExitGamePunisher_Punish, old_ExitGamePunisher_Punish);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3736EF4")), get_CrcCheckEnabled, old_get_CrcCheckEnabled);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3737130")), get_MaxResendsBeforeDisconnect, old_get_MaxResendsBeforeDisconnect);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x37370C4")), get_PacketLossByCrcCheck, old_get_PacketLossByCrcCheck);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x3736CD8")), get_CountOfPlayers, old_get_CountOfPlayers);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2008A98")), launcher_OnJoinedRoom, old_launcher_OnJoinedRoom);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x200B23C")), launcher_OnPlayerLeftRoom, old_launcher_OnPlayerLeftRoom);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x200CAE0")), changeRoomVisibility, old_changeRoomVisibility);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x200E294")), launcher_Update_Hook, old_launcher_Update);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202F4A8")), die, old_die);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x201E160")), RPC_playerDies, old_RPC_playerDies);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x20289F8")), RPC_playerDiesBySpider, old_RPC_playerDiesBySpider);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x20317D8")), RPC_playerKilledByAI, old_RPC_playerKilledByAI);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x203232C")), RPC_playerDiesBySpiderAIGranny, old_RPC_playerDiesBySpiderAIGranny);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202AFDC")), dieByLava, old_dieByLava);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2033250")), RPC_playerExplosionDeath, old_RPC_playerExplosionDeath);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2033188")), onDiedByOpeningBox, old_onDiedByOpeningBox);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2031764")), RPC_onSpiderMomKilledYou, old_RPC_onSpiderMomKilledYou);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202CD60")), crowAttackPlayer, old_crowAttackPlayer);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202CB64")), playerHitBearTrap, old_playerHitBearTrap);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202CF4C")), hitByArrow, old_hitByArrow);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2032E18")), RPC_playPlayerBurningOnDeath, old_RPC_playPlayerBurningOnDeath);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2028F94")), playerSpiderBittenDead, old_playerSpiderBittenDead);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2032A0C")), playerSpiderBittenDeadAIGranny, old_playerSpiderBittenDeadAIGranny);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2032D50")), AIDie, old_AIDie);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x205B804")), playerManager_die, old_playerManager_die);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x205CC04")), dieAI, old_dieAI);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x201584C")), momKillPlayer, old_momKillPlayer);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1F865C0")), playerExplode, old_playerExplode);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202F360")), playerFallDead, old_playerFallDead);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1F8C368")), playerFalling, old_playerFalling);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1FFDBC0")), noFall, old_noFall);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1FFDDAC")), waitToResetBack, old_waitToResetBack);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202E880")), checkFall_Hook, old_checkFall);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x852C28")), PlayerLandBad_Hook, old_PlayerLandBad);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x852F8C")), afterPlayerFallHurt_Hook, old_afterPlayerFallHurt);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1F8652C")), fallingDead, old_fallingDead);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x202F3D4")), RPC_sendMessageFallen, old_RPC_sendMessageFallen);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1FF89E8")), triggerFellEvent, old_triggerFellEvent);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1F2E470")), triggerFallFloor_OnTriggerEnter, old_triggerFallFloor_OnTriggerEnter);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1F52F84")), EnemyEyeUpdate, old_EnemyEyeUpdate);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x20161C8")), momOnPlayerSeen, old_momOnPlayerSeen);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x2015A1C")), momCheckForPlayers, old_momCheckForPlayers);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x201639C")), RPC_playMomRoar, old_RPC_playMomRoar);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x20144EC")), momParseForPlayers, old_momParseForPlayers);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x206AF30")), spider_funcHuntPlayer, old_spider_funcHuntPlayer);
#endif
    LOGI(OBFUSCATE("Done"));
    return NULL;
}

__attribute__((constructor))
void lib_main() {
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
}
