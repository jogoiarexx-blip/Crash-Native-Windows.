#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <limits>
#include <sstream>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "hd_assets.hpp"
#include "category0.hpp"
#include "run_state.hpp"
#include "frontend.hpp"
#include "audio.hpp"
#include "audio_win32.hpp"
#include "gba_art.hpp"
#include "special_room.hpp"
namespace fs=std::filesystem;
static std::unique_ptr<crash::GameData> data;
static crash::Scene scene;
static std::unique_ptr<crash::Terrain> terrain;
static std::unique_ptr<crash::Player> player;
static std::unique_ptr<crash::EntityWorld> world;
static std::unique_ptr<crash::Category0Stage> categoryStage;
static crash::RunState runState;
enum class UiMode { Title, Map, Game };
static UiMode uiMode=UiMode::Title;
static unsigned mapLevel=0;
static int titleChoice=0;
static bool savePresent=false;
static const char* SavePath="save/crash-native-v1.sav";
static unsigned currentLevel=0,currentRoomSlot=0,currentCategory=0;
enum class RoomFlow { Main, Bonus, GemPath };
static RoomFlow roomFlow=RoomFlow::Main;
static constexpr int BonusStateSlot=100,GemPathStateSlot=101;
static unsigned specialReturnSlot=0,specialStartRespawns=0,specialBackupAku=0,specialBackupLives=0,specialBackupGemFlags=0,specialBackupPowerFlags=0;
static RoomFlow pendingSpecialFlow=RoomFlow::Main;
static int specialTransitionTicks=0;
static double specialTransitionStartY=0.0,specialTransitionTargetY=0.0;
static unsigned specialReturnPadId=std::numeric_limits<unsigned>::max();
static bool specialReturnEntryLocked=false;
static RoomFlow specialReturnLockFlow=RoomFlow::Main;
static bool categoryMode=false;
static int transitionOut=0,fadeIn=0;
static HWND hwnd;
static HDC backDc=nullptr;
static HBITMAP backBitmap=nullptr,backOldBitmap=nullptr;
static int backWidth=0,backHeight=0;
static crash::ExternalSprites external;
static crash::DisplaySettings display;
static bool isFullscreen=false;
static std::vector<uint32_t> pixels;
static crash::Win32Audio audio;
static crash::AudioCounters audioCounters{};
static crash::RomUiArt romUiArt;
static bool keys[256]{},paused=false,debug=false;
static double accumulator=0;
static std::chrono::steady_clock::time_point last;

void ensureBackBuffer(HDC screen,int width,int height){
 width=std::max(1,width);height=std::max(1,height);
 if(!backDc){backDc=CreateCompatibleDC(screen);if(!backDc)throw std::runtime_error("Back-buffer DC creation failed");}
 if(backBitmap&&backWidth==width&&backHeight==height)return;
 if(backBitmap){SelectObject(backDc,backOldBitmap);DeleteObject(backBitmap);backBitmap=nullptr;}
 backBitmap=CreateCompatibleBitmap(screen,width,height);if(!backBitmap)throw std::runtime_error("Back-buffer bitmap creation failed");
 HGDIOBJ old=SelectObject(backDc,backBitmap);if(!backOldBitmap)backOldBitmap=reinterpret_cast<HBITMAP>(old);
 backWidth=width;backHeight=height;
}
void destroyBackBuffer(){
 if(backDc){if(backBitmap){SelectObject(backDc,backOldBitmap);DeleteObject(backBitmap);}DeleteDC(backDc);}
 backDc=nullptr;backBitmap=nullptr;backOldBitmap=nullptr;backWidth=backHeight=0;
}
crash::AudioCounters currentAudioCounters(){
 crash::AudioCounters c;
 if(uiMode!=UiMode::Game)return c;
 if(categoryMode&&categoryStage){if(!categoryStage->jetpack()){c.wumpa=categoryStage->wumpa;c.crates=categoryStage->crates;c.checkpoints=categoryStage->checkpointHits;c.hazards=categoryStage->hazards;c.lives=categoryStage->lives;c.enemies=categoryStage->enemiesDestroyed;c.clock=runState.timeTrialActive;c.complete=categoryStage->complete||transitionOut!=0;}}
 else if(world){c.wumpa=world->wumpaCollected;c.crates=world->crateBroken;c.checkpoints=world->checkpointHits;c.hazards=world->hazardHits;c.lives=world->extraLives;c.gems=world->gemsCollected;c.enemies=world->enemyKills;c.clock=runState.timeTrialActive;c.complete=world->levelExitTriggered||transitionOut!=0;}
 return c;
}
void resetAudioCounters(){audioCounters=currentAudioCounters();}
void syncAudioCounters(){auto now=currentAudioCounters();for(auto cue:crash::audioDelta(audioCounters,now))audio.sfx(cue);audioCounters=now;}
void syncCategorySfx(){if(!categoryMode||!categoryStage||!categoryStage->jetpack())return;for(const auto& e:categoryStage->takeSfxEvents())audio.sfxId(e.id,e.volumeParam,e.forcedVoice);}
void syncMusic(){if(uiMode==UiMode::Title)audio.setMusic(crash::MusicCue::MainMenu);else if(uiMode==UiMode::Map)audio.setMusic(crash::MusicCue::WarpRoom);else if(roomFlow==RoomFlow::Bonus)audio.setMusic(crash::MusicCue::Bonus);else audio.setMusic(crash::levelMusic(currentLevel,currentCategory,categoryMode));}
int currentStateSlot(){return roomFlow==RoomFlow::Bonus?BonusStateSlot:roomFlow==RoomFlow::GemPath?GemPathStateSlot:int(currentRoomSlot);}
void captureCurrent(){
 if(uiMode==UiMode::Game&&!categoryMode&&world&&player)runState.capture(int(currentLevel),currentStateSlot(),*world,*player);
}
void saveState(bool capture=true){
 if(capture){captureCurrent();if(uiMode==UiMode::Game)runState.lastLevel=std::min(currentLevel,24u);}fs::create_directories("save");savePresent=runState.saveFile(SavePath);
}
void enterMap(){
 specialTransitionTicks=0;pendingSpecialFlow=RoomFlow::Main;specialReturnEntryLocked=false;
 captureCurrent();runState.lastLevel=std::min(currentLevel,24u);saveState(false);uiMode=UiMode::Map;paused=false;transitionOut=0;fadeIn=0;accumulator=0;mapLevel=std::min(runState.lastLevel,24u);syncMusic();resetAudioCounters();
}
void loadTileEntry(unsigned level,const crash::RoomRouteEntry& entry,int stateSlot,RoomFlow flow,bool forceExit){
 crash::Scene next=data->sceneFromRoom(entry);
 auto nextTerrain=std::make_unique<crash::Terrain>(*data,next);
 auto nextPlayer=std::make_unique<crash::Player>(*data,next,*nextTerrain);
 auto nextWorld=std::make_unique<crash::EntityWorld>(*data,next);
 nextWorld->armExitMarker(forceExit||nextWorld->canAutoExit());
 runState.restore(int(level),stateSlot,*nextWorld,*nextPlayer);
 nextWorld->setTimeTrialAvailable(flow==RoomFlow::Main&&runState.clockAvailable(int(level)));
 scene=std::move(next);terrain=std::move(nextTerrain);player=std::move(nextPlayer);world=std::move(nextWorld);categoryStage.reset();categoryMode=false;currentCategory=0;currentLevel=level;roomFlow=flow;accumulator=0;transitionOut=0;fadeIn=24;resetAudioCounters();
}
void loadRoom(unsigned level,unsigned slot){
 specialReturnEntryLocked=false;
 auto route=data->roomRoute(level);if(slot>=route.size())throw std::runtime_error("Room slot");
 loadTileEntry(level,route[slot],int(slot),RoomFlow::Main,false);currentRoomSlot=slot;
}
bool alignPlayerToSpecialPad(crash::SpecialPadKind kind,unsigned preferredId=std::numeric_limits<unsigned>::max()){
 if(!world||!player)return false;
 auto spot=crash::specialPadPlacement(*world,*player,kind,preferredId);if(!spot)return false;
 crash::snapPlayerToSpecialPad(*player,*spot);world->bind(*player);
 specialReturnPadId=spot->entityId;return true;
}
bool startSpecialTransition(RoomFlow flow){
 if(categoryMode||!world||!player||flow==RoomFlow::Main||specialTransitionTicks)return false;
 const auto kind=flow==RoomFlow::Bonus?crash::ExtraRoomKind::Bonus:crash::ExtraRoomKind::GemPath;
 auto extra=data->extraRoom(currentLevel,kind);if(!extra||!data->nativePlayable(*extra))return false;
 alignPlayerToSpecialPad(flow==RoomFlow::Bonus?crash::SpecialPadKind::Bonus:crash::SpecialPadKind::GemPath);
 captureCurrent();specialReturnSlot=currentRoomSlot;specialStartRespawns=player->respawns;
 specialBackupAku=runState.akuMasks;specialBackupLives=runState.extraLives;specialBackupGemFlags=runState.gemFlags;specialBackupPowerFlags=runState.powerFlags;
 specialReturnEntryLocked=false;pendingSpecialFlow=flow;specialTransitionTicks=1;specialTransitionStartY=player->y;specialTransitionTargetY=player->y+18.0;return true;
}
void stepSpecialTransition(){
 if(!player||specialTransitionTicks<=0)return;
 constexpr int duration=18;
 const double t=std::clamp(double(specialTransitionTicks)/duration,0.0,1.0);
 player->vx=0;player->vy=0;player->grounded=true;player->crouching=true;player->bodySlamming=false;player->slideTicks=0;player->spinTicks=0;player->animation=4;player->animationTicks=0;
 player->y=specialTransitionStartY+(specialTransitionTargetY-specialTransitionStartY)*(t*t*(3.0-2.0*t));
 if(++specialTransitionTicks>duration){
  const RoomFlow flow=pendingSpecialFlow;pendingSpecialFlow=RoomFlow::Main;specialTransitionTicks=0;
  const auto kind=flow==RoomFlow::Bonus?crash::ExtraRoomKind::Bonus:crash::ExtraRoomKind::GemPath;
  auto extra=data->extraRoom(currentLevel,kind);if(!extra||!data->nativePlayable(*extra))return;
  loadTileEntry(currentLevel,*extra,flow==RoomFlow::Bonus?BonusStateSlot:GemPathStateSlot,flow,true);syncMusic();
 }
}
bool enterSpecial(RoomFlow flow){return startSpecialTransition(flow);}
void finishSpecial(bool success){
 if(roomFlow==RoomFlow::Main)return;
 const RoomFlow completedFlow=roomFlow;const unsigned level=currentLevel,returnSlot=specialReturnSlot;
 pendingSpecialFlow=RoomFlow::Main;specialTransitionTicks=0;
 if(success){captureCurrent();if(completedFlow==RoomFlow::Bonus)runState.markBonusDone(int(level));else runState.markGemPathDone(int(level));}
 else{runState.akuMasks=specialBackupAku;runState.extraLives=specialBackupLives;runState.gemFlags=specialBackupGemFlags;runState.powerFlags=specialBackupPowerFlags;runState.respawns=player?player->respawns:specialStartRespawns;runState.rooms.erase({int(level),completedFlow==RoomFlow::Bonus?BonusStateSlot:GemPathStateSlot});}
 loadRoom(level,returnSlot);
 alignPlayerToSpecialPad(completedFlow==RoomFlow::Bonus?crash::SpecialPadKind::Bonus:crash::SpecialPadKind::GemPath,specialReturnPadId);
 specialReturnLockFlow=completedFlow;specialReturnEntryLocked=true;
 syncMusic();saveState(false);
}
void loadCategory(unsigned level,unsigned slot,unsigned category){
 specialReturnEntryLocked=false;specialTransitionTicks=0;pendingSpecialFlow=RoomFlow::Main;
 if(category>6)throw std::runtime_error("Actor category not native yet");
 categoryStage=std::make_unique<crash::Category0Stage>(*data,category);
 categoryStage->setTimeTrialVariant(runState.timeTrialActive&&runState.timeTrialLevel==int(level));
 world.reset();player.reset();terrain.reset();scene={};roomFlow=RoomFlow::Main;categoryMode=true;currentCategory=category;currentLevel=level;currentRoomSlot=slot;accumulator=0;transitionOut=0;fadeIn=24;resetAudioCounters();
}
bool supportedEntry(const crash::RoomRouteEntry& room){return data->nativePlayable(room)||(room.kind==3&&room.category<=6);}
void loadEntry(unsigned level,const crash::RoomRouteEntry& room){if(data->nativePlayable(room))loadRoom(level,room.slot);else if(room.kind==3&&room.category<=6)loadCategory(level,room.slot,room.category);else throw std::runtime_error("Route entry not native yet");}
void loadLevel(unsigned id){
 auto route=data->roomRoute(id);for(const auto& room:route)if(supportedEntry(room)){loadEntry(id,room);return;}
 throw std::runtime_error("No supported native entry in this level");
}
void changeLevel(int delta){
 captureCurrent();int base=int(currentLevel);for(int n=1;n<=25;n++){int candidate=(base+delta*n)%25;if(candidate<0)candidate+=25;try{loadLevel(unsigned(candidate));return;}catch(const std::exception&){}}
}
void changeProgression(int delta){
 if(delta<0){changeLevel(-1);return;}
 if(roomFlow!=RoomFlow::Main){finishSpecial(true);return;}
 captureCurrent();auto route=data->roomRoute(currentLevel);
 int base=-1;for(size_t i=0;i<route.size();i++)if(route[i].slot==currentRoomSlot){base=int(i);break;}
 for(size_t i=size_t(std::max(0,base+1));i<route.size();i++)if(supportedEntry(route[i])){loadEntry(currentLevel,route[i]);return;}
 runState.finishLevel(int(currentLevel));runState.lastLevel=runState.suggestedLevelAfter(int(currentLevel));saveState(false);mapLevel=runState.lastLevel;uiMode=UiMode::Map;paused=false;transitionOut=0;fadeIn=0;accumulator=0;syncMusic();audio.sfx(crash::SfxCue::MapOpen);resetAudioCounters();
}

void refresh(){
 if(uiMode==UiMode::Title){pixels=crash::scaleUi(crash::renderTitleScreen(savePresent,titleChoice),display.scale);}
 else if(uiMode==UiMode::Map){pixels=crash::scaleUi(crash::renderLevelMap(runState,mapLevel,&romUiArt),display.scale);}
 else if(paused){pixels=crash::scaleUi(crash::renderPauseScreen(&romUiArt,currentLevel),display.scale);}
 else if(categoryMode){pixels=categoryStage->render(display.scale,debug);}
 else{pixels=crash::renderHD(*data,scene,*player,external,display.scale,display.external,debug,world.get());}
 int shade=0;
 if(uiMode==UiMode::Game){
  if(transitionOut)shade=std::min(255,transitionOut*255/30);
  else if(specialTransitionTicks)shade=std::min(255,specialTransitionTicks*255/18);
  else if(fadeIn)shade=std::min(255,fadeIn*255/24);
 }
 if(shade){const unsigned keep=255u-unsigned(shade);for(auto& px:pixels){unsigned r=(px>>16)&255,g=(px>>8)&255,b=px&255;r=r*keep/255;g=g*keep/255;b=b*keep/255;px=(px&0xff000000u)|(r<<16)|(g<<8)|b;}}
 std::ostringstream title;title<<"Crash Native v0.68.0";
 if(uiMode==UiMode::Title)title<<" | MENU PRINCIPAL";
 else if(uiMode==UiMode::Map){title<<" | MAPA | "<<crash::levelNames()[mapLevel]<<(runState.levelUnlocked(int(mapLevel))?"":" BLOQUEADA")<<" | Concluidas "<<runState.completedLevels.size()<<"/25";unsigned best=runState.bestTime(int(mapLevel));if(best)title<<" | Melhor "<<(best/3600)<<':'<<((best/60)%60)/10<<((best/60)%10)<<'.'<<((best%60)*100/60)/10;}
 else{title<<" | "<<crash::levelNames()[currentLevel];if(roomFlow==RoomFlow::Bonus)title<<" | BONUS ROUND";else if(roomFlow==RoomFlow::GemPath)title<<" | GEM PATH";else title<<" | Etapa "<<(currentRoomSlot+1)<<"/"<<data->roomRoute(currentLevel).size();title<<" | "<<240*display.scale<<"x"<<160*display.scale;if(categoryMode){title<<" | ActorCat "<<currentCategory<<" | Percurso "<<int(categoryStage->scroll);if(categoryStage->rom.routeEndThreshold)title<<'/'<<categoryStage->rom.routeEndThreshold;if(categoryStage->jetpack()){title<<" | HP "<<categoryStage->hp<<" | Inimigos "<<categoryStage->enemiesDestroyed<<" | Tiros "<<categoryStage->shotsFired<<" | Vidas "<<categoryStage->lives;if(categoryStage->rom.type==2)title<<" | Hover "<<categoryStage->hovercraftPartsLeft<<"/4";}else title<<" | Wumpa "<<categoryStage->wumpa<<" | Caixas "<<categoryStage->crates<<" | Vidas "<<categoryStage->lives;}else{title<<" | Wumpa "<<world->wumpaCollected<<'/'<<world->wumpaTotal<<" | Caixas "<<world->crateBroken<<'/'<<world->crateTotal<<" | Inimigos "<<world->enemyKills<<'/'<<world->enemyTotal<<" | Gemas "<<world->gemsCollected<<'/'<<world->gemTotal<<" | Aku "<<world->akuMasks<<" | Vidas+ "<<world->extraLives;}if(runState.timeTrialActive){unsigned f=runState.timeTrialFrames;title<<" | TIME "<<(f/3600)<<':'<<((f/60)%60)/10<<((f/60)%10)<<'.'<<((f%60)*100/60)/10;}title<<(transitionOut?" | SAIDA":"")<<(paused?" | PAUSADO":"");}
 SetWindowTextA(hwnd,title.str().c_str());InvalidateRect(hwnd,nullptr,FALSE);
}
void fullscreen(){static WINDOWPLACEMENT old{};old.length=sizeof(WINDOWPLACEMENT);static LONG_PTR style=0;isFullscreen=!isFullscreen;if(isFullscreen){style=GetWindowLongPtrA(hwnd,GWL_STYLE);GetWindowPlacement(hwnd,&old);MONITORINFO mi{};mi.cbSize=sizeof(MONITORINFO);GetMonitorInfoA(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);SetWindowLongPtrA(hwnd,GWL_STYLE,style&~WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);}else{SetWindowLongPtrA(hwnd,GWL_STYLE,style);SetWindowPlacement(hwnd,&old);SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);}}
void reloadAssets(){external.load(*data,"assets/overrides");fs::create_directories("exports");external.report("exports/sprite-load-report.txt");}
void sizeWindow(){
 if(isFullscreen)return;
 RECT desired{0,0,240*display.scale,160*display.scale+72};AdjustWindowRect(&desired,WS_OVERLAPPEDWINDOW,FALSE);
 MONITORINFO mi{};mi.cbSize=sizeof(mi);GetMonitorInfoA(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 int width=std::min(int(desired.right-desired.left),int(mi.rcWork.right-mi.rcWork.left));int height=std::min(int(desired.bottom-desired.top),int(mi.rcWork.bottom-mi.rcWork.top));
 SetWindowPos(hwnd,nullptr,mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-width)/2,mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-height)/2,width,height,SWP_NOZORDER);
}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM key,LPARAM param){try{switch(msg){
 case WM_KEYDOWN:
  if(key<256)keys[key]=true;
  if(param&(1LL<<30))return 0;
  if(key==VK_F11){fullscreen();refresh();return 0;}
  if(uiMode==UiMode::Title){
   int choices=savePresent?3:2;if(key==VK_UP){titleChoice=(titleChoice+choices-1)%choices;audio.sfx(crash::SfxCue::Navigate);}else if(key==VK_DOWN){titleChoice=(titleChoice+1)%choices;audio.sfx(crash::SfxCue::Navigate);}else if(key==VK_RETURN||key==VK_SPACE){audio.sfx(crash::SfxCue::Confirm);if(savePresent&&titleChoice==0){runState.loadFile(SavePath);mapLevel=std::min(runState.lastLevel,24u);uiMode=UiMode::Map;syncMusic();audio.sfx(crash::SfxCue::MapOpen);}else if((savePresent&&titleChoice==1)||(!savePresent&&titleChoice==0)){runState.clear();currentLevel=0;loadLevel(0);mapLevel=0;saveState(false);uiMode=UiMode::Map;syncMusic();audio.sfx(crash::SfxCue::MapOpen);}else DestroyWindow(w);}else if(key==VK_ESCAPE){audio.sfx(crash::SfxCue::Cancel);DestroyWindow(w);return 0;}refresh();return 0;
  }
  if(uiMode==UiMode::Map){
   unsigned room=crash::warpRoomOf(mapLevel),slot=crash::warpSlotOf(mapLevel);bool moved=false;
   if(key==VK_ESCAPE){audio.sfx(crash::SfxCue::Cancel);uiMode=UiMode::Title;titleChoice=0;syncMusic();}
   else if(key==VK_LEFT){if(room<4){slot=(slot+5)%6;mapLevel=crash::warpLevel(room,slot);moved=true;}}
   else if(key==VK_RIGHT){if(room<4){slot=(slot+1)%6;mapLevel=crash::warpLevel(room,slot);moved=true;}}
   else if(key==VK_UP){if(room>0){--room;mapLevel=crash::warpLevel(room,slot);moved=true;}}
   else if(key==VK_DOWN){if(room<4){++room;mapLevel=crash::warpLevel(room,slot);moved=true;}}
   else if((key==VK_RETURN||key==VK_SPACE)&&runState.levelUnlocked(int(mapLevel))){audio.sfx(crash::SfxCue::MapConfirm);runState.lastLevel=mapLevel;loadLevel(mapLevel);uiMode=UiMode::Game;saveState(false);syncMusic();resetAudioCounters();}
   if(moved)audio.sfx(crash::SfxCue::MapNavigate);refresh();return 0;
  }
  if(key==VK_ESCAPE){audio.sfx(crash::SfxCue::Cancel);enterMap();refresh();return 0;}
  if(!categoryMode&&(key==VK_SPACE||key=='Z'))audio.sfx(crash::SfxCue::Jump);
  if(!categoryMode&&(key=='X'||key=='K'))audio.sfx(crash::SfxCue::Spin);
  if(key==VK_F5){reloadAssets();}else if(key==VK_F10){saveState();}else if(key==VK_F6){display.scale=display.scale==1?4:display.scale==4?6:1;sizeWindow();}else if(key=='H'){display.external=!display.external;}else if(key=='P'){paused=!paused;accumulator=0;}else if(key=='C')debug=!debug;else if(key==VK_F7){changeLevel(1);}else if(key==VK_F8){changeLevel(-1);}else if(key=='R'){if(categoryMode){categoryStage=std::make_unique<crash::Category0Stage>(*data,currentCategory);categoryStage->setTimeTrialVariant(runState.timeTrialActive&&runState.timeTrialLevel==int(currentLevel));runState.restartTimeTrial(int(currentLevel));}else{runState.syncGlobal(*world,*player);runState.rooms.erase(std::make_pair(int(currentLevel),currentStateSlot()));player->reset();world->reset();world->akuMasks=runState.akuMasks;world->extraLives=runState.extraLives;player->respawns=runState.respawns;world->setProgress(runState.gemFlags,runState.bonusDone(int(currentLevel)),runState.gemPathDone(int(currentLevel)),runState.timeTrialActive&&runState.timeTrialLevel==int(currentLevel));world->setTimeTrialAvailable(roomFlow==RoomFlow::Main&&runState.clockAvailable(int(currentLevel)));world->bind(*player);runState.restartTimeTrial(int(currentLevel));}transitionOut=0;fadeIn=0;resetAudioCounters();}
  refresh();return 0;
 case WM_KEYUP:if(key<256)keys[key]=false;return 0;
 case WM_KILLFOCUS:std::fill(std::begin(keys),std::end(keys),false);paused=true;accumulator=0;refresh();return 0;
 case WM_TIMER:{
  auto now=std::chrono::steady_clock::now();double dt=std::min(.1,std::chrono::duration<double>(now-last).count());last=now;
  if(uiMode==UiMode::Game&&!paused){accumulator+=dt;while(accumulator>=1.0/60){
    if(transitionOut){if(++transitionOut>=30){changeProgression(1);accumulator=0;break;}}
    else if(specialTransitionTicks){stepSpecialTransition();}
    else if(categoryMode){
      const bool left=keys[VK_LEFT]||keys['A'],right=keys[VK_RIGHT]||keys['D'];
      if(categoryStage->jetpack())categoryStage->step(left,right,keys[VK_SPACE]||keys['Z'],keys[VK_UP]||keys['W'],keys[VK_DOWN]||keys['S'],keys['Q'],keys['E']);
      else categoryStage->step(left,right,keys[VK_SPACE]||keys['Z']);
      if(categoryStage->timeTrialStartRequested){categoryStage->timeTrialStartRequested=false;runState.startTimeTrial(int(currentLevel));categoryStage->setTimeTrialVariant(true);}
      syncCategorySfx();
      if(categoryStage->complete)transitionOut=1;
    }
    else{unsigned respawnsBefore=player->respawns;player->step({keys[VK_LEFT]||keys['A'],keys[VK_RIGHT]||keys['D'],keys[VK_SPACE]||keys['Z'],keys['X']||keys['K'],keys[VK_UP]||keys['W'],keys[VK_DOWN]||keys['S'],keys['E'],keys['Q']});world->step(*player);if(roomFlow!=RoomFlow::Main&&player->respawns>respawnsBefore){finishSpecial(false);accumulator=0;break;}if(world->timeTrialStarted){runState.startTimeTrial(int(currentLevel));world->timeTrialStarted=false;world->setTimeTrialAvailable(false);world->setProgress(runState.gemFlags,runState.bonusDone(int(currentLevel)),runState.gemPathDone(int(currentLevel)),true);}if(roomFlow==RoomFlow::Main&&specialReturnEntryLocked){
      const auto kind=specialReturnLockFlow==RoomFlow::Bonus?crash::SpecialPadKind::Bonus:crash::SpecialPadKind::GemPath;
      if(!crash::playerStillOnSpecialPad(*world,*player,kind,specialReturnPadId))specialReturnEntryLocked=false;
      world->bonusRequested=false;world->gemPathRequested=false;
     }else if(roomFlow==RoomFlow::Main&&world->bonusRequested){world->bonusRequested=false;if(enterSpecial(RoomFlow::Bonus)){accumulator=0;break;}}
     else if(roomFlow==RoomFlow::Main&&world->gemPathRequested){world->gemPathRequested=false;if(enterSpecial(RoomFlow::GemPath)){accumulator=0;break;}}if(world->levelExitTriggered||player->hoverExitRequested())transitionOut=1;}
    if(!(categoryMode&&categoryStage&&categoryStage->clockFrozen()))runState.tickTimeTrial();
    if(fadeIn>0)--fadeIn;accumulator-=1.0/60;
  }}
  syncAudioCounters();refresh();return 0;}
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);ensureBackBuffer(dc,r.right-r.left,r.bottom-r.top);
  PatBlt(backDc,0,0,r.right-r.left,r.bottom-r.top,BLACKNESS);
  int availableH=std::max(1,int(r.bottom)-72);int width=std::max(1,std::min(int(r.right),availableH*3/2)),height=std::max(1,width*2/3),x=(r.right-width)/2,y=(availableH-height)/2;
  int sourceW=240*display.scale,sourceH=160*display.scale;
  BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=sourceW;bi.bmiHeader.biHeight=-sourceH;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
  SetStretchBltMode(backDc,COLORONCOLOR);StretchDIBits(backDc,x,y,width,height,0,0,sourceW,sourceH,pixels.data(),&bi,DIB_RGB_COLORS,SRCCOPY);SetTextColor(backDc,RGB(225,230,245));SetBkMode(backDc,TRANSPARENT);
  if(uiMode==UiMode::Game){const char* lines[]={"SALAS: A/D mover, ESPACO/Z pular, X/K giro, E=R agachar/crawl/slide/slam, BAIXO agachar, Q=L turbo | SUB: A/D/W/S nadar","P: pausa | R: reiniciar | ESC: voltar ao mapa | F10: salvar | F11: tela cheia","F5: recarregar PNG | F6: escala | F7/F8: trocar fase (teste) | H: PNG/ROM | C: debug"};for(int i=0;i<3;i++)TextOutA(backDc,12,availableH+8+20*i,lines[i],int(strlen(lines[i])));}
  else if(uiMode==UiMode::Map){std::string a="WARP ROOM - Esquerda/Direita: fase | Cima/Baixo: andar | ENTER: jogar | ESC: menu";TextOutA(backDc,12,availableH+8,a.c_str(),int(a.size()));std::ostringstream b;b<<crash::levelNames()[mapLevel]<<" - "<<(runState.levelUnlocked(int(mapLevel))?"LIBERADA":"BLOQUEADA")<<" | concluidas "<<runState.completedLevels.size()<<"/25 | gemas "<<runState.totalGems();auto bs=b.str();TextOutA(backDc,12,availableH+28,bs.c_str(),int(bs.size()));std::string c="As 5 fases do Warp Room ficam abertas; conclua as 5 para liberar o boss.";TextOutA(backDc,12,availableH+48,c.c_str(),int(c.size()));}
  else{std::string foot="Setas + ENTER | F11 tela cheia | audio nativo ativo";TextOutA(backDc,12,availableH+28,foot.c_str(),int(foot.size()));}
  BitBlt(dc,0,0,r.right-r.left,r.bottom-r.top,backDc,0,0,SRCCOPY);EndPaint(w,&ps);return 0;}
 case WM_SIZE:InvalidateRect(w,nullptr,FALSE);return 0;
 case WM_DESTROY:if(uiMode==UiMode::Game)saveState();KillTimer(w,1);audio.stop();destroyBackBuffer();PostQuitMessage(0);return 0;
 }}catch(const std::exception& e){KillTimer(w,1);MessageBoxA(w,e.what(),"Crash Native - erro",MB_ICONERROR);DestroyWindow(w);return 0;}
 return DefWindowProcA(w,msg,key,param);
}
int WINAPI WinMain(HINSTANCE h,HINSTANCE,LPSTR,int show){try{
 SetProcessDPIAware();char path[MAX_PATH];GetModuleFileNameA(nullptr,path,MAX_PATH);fs::current_path(fs::path(path).parent_path());
 data=std::make_unique<crash::GameData>(crash::read("input/crash.gba"));romUiArt=crash::loadRomUiArt(data->rom.bytes);loadLevel(0);display=crash::settings("display.ini");reloadAssets();savePresent=runState.loadFile(SavePath);if(savePresent){mapLevel=std::min(runState.lastLevel,24u);}uiMode=UiMode::Title;audio.start(&data->rom.bytes);syncMusic();resetAudioCounters();
 WNDCLASSA wc{};wc.lpfnWndProc=proc;wc.hInstance=h;wc.lpszClassName="CrashNativeGameplay";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&wc);
 hwnd=CreateWindowA(wc.lpszClassName,"Crash Native v0.68.0",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,990,755,nullptr,nullptr,h,nullptr);if(!hwnd)throw std::runtime_error("Window creation failed");
 sizeWindow();refresh();last=std::chrono::steady_clock::now();SetTimer(hwnd,1,16,nullptr);ShowWindow(hwnd,show);MSG m;while(GetMessageA(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageA(&m);}return 0;
 }catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Crash Native - erro",MB_ICONERROR);return 1;}}
