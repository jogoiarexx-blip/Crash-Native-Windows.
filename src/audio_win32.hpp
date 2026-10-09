#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "audio.hpp"
#include "gax_rom.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace crash {

class Win32Audio {
    static constexpr int Rate=44100;
    static constexpr int Frames=2048;
    static constexpr int BufferCount=4;
    HWAVEOUT out_=nullptr;
    std::array<std::array<int16_t,Frames*2>,BufferCount> pcm_{};
    std::array<WAVEHDR,BufferCount> hdr_{};
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::atomic<MusicCue> music_{MusicCue::Silence};
    std::atomic<uint32_t> seedCounter_{1};
    MusicCue renderMusic_=MusicCue::Silence;
    uint64_t sampleClock_=0,musicClock_=0;
    double phase1_=0,phase2_=0,bassPhase_=0;
    struct Voice {double phase=0,freq=440,amp=.3;unsigned left=0,total=0;bool noise=false;uint32_t seed=1;};
    std::mutex mutex_;
    std::mutex gaxMutex_;
    std::vector<Voice> voices_;
    GaxLitePlayer gax_;
    bool gaxReady_=false;

    static double hz(int midi){return midi<=0?0.0:440.0*std::pow(2.0,(midi-69)/12.0);}
    static double sq(double p){return std::sin(p)>=0?1.0:-1.0;}
    static double tri(double p){double x=std::fmod(p/(2.0*3.14159265358979323846),1.0);if(x<0)x+=1.0;return 1.0-4.0*std::abs(x-.5);}
    static const std::vector<int>& motif(MusicCue c){
        static const std::vector<int> silent{0};
        static const std::vector<int> menu{52,55,59,64,59,55,50,55,59,62,59,55,48,52,55,60,55,52};
        static const std::vector<int> warp{48,55,60,55,50,57,62,57,52,59,64,59,50,57,62,57};
        static const std::vector<int> jungle{52,55,57,55,52,60,57,55,50,52,55,57,55,52,50,48};
        static const std::vector<int> arctic{57,60,64,67,64,60,55,59,62,65,62,59,53,57,60,64};
        static const std::vector<int> sewer{43,46,50,53,50,46,41,45,48,52,48,45,39,43,46,50};
        static const std::vector<int> water{55,59,62,67,64,62,59,55,53,57,60,65,62,60,57,53};
        static const std::vector<int> rocket{52,59,64,67,71,67,64,59,50,57,62,66,69,66,62,57};
        static const std::vector<int> future{48,55,63,60,67,63,58,55,46,53,61,58,65,61,56,53};
        static const std::vector<int> bonus{60,64,67,72,67,64,62,65,69,74,69,65,60,64,67,72};
        static const std::vector<int> boss1{43,43,50,46,43,55,50,46,41,41,48,45,41,53,48,45};
        static const std::vector<int> boss2{40,47,52,55,52,47,38,45,50,54,50,45,36,43,48,52};
        static const std::vector<int> boss3{45,52,57,60,57,52,43,50,55,59,55,50,41,48,53,57};
        static const std::vector<int> cortex{36,43,48,51,55,51,48,43,35,42,47,50,54,50,47,42};
        switch(c){case MusicCue::MainMenu:return menu;case MusicCue::WarpRoom:return warp;case MusicCue::Jungle:return jungle;case MusicCue::Arctic:return arctic;case MusicCue::Sewers:return sewer;case MusicCue::Underwater:return water;case MusicCue::Rocket:return rocket;case MusicCue::Future:return future;case MusicCue::Bonus:return bonus;case MusicCue::Dingodile:return boss1;case MusicCue::NGin:return boss2;case MusicCue::Tiny:return boss3;case MusicCue::Cortex:return cortex;default:return silent;}
    }
    static int bassFor(MusicCue c){switch(c){case MusicCue::Sewers:return 31;case MusicCue::Dingodile:case MusicCue::NGin:case MusicCue::Tiny:case MusicCue::Cortex:return 28;case MusicCue::Rocket:case MusicCue::Future:return 36;default:return 40;}}
    void addVoice(double freq,double amp,unsigned ms,bool noise=false){std::lock_guard<std::mutex> g(mutex_);Voice v;v.freq=freq;v.amp=amp;v.total=v.left=std::max(1u,unsigned(uint64_t(Rate)*ms/1000));v.noise=noise;v.seed=seedCounter_.fetch_add(0x9e3779b9u)+unsigned(freq*17)+1;voices_.push_back(v);}
    void synthSfx(SfxCue c){
        switch(c){
        case SfxCue::Navigate:addVoice(660,.18,45);break;case SfxCue::Confirm:addVoice(880,.24,90);addVoice(1320,.12,70);break;case SfxCue::Cancel:addVoice(330,.20,90);break;case SfxCue::MapNavigate:addVoice(610,.18,55);break;case SfxCue::MapConfirm:addVoice(920,.24,110);addVoice(1380,.12,90);break;case SfxCue::MapOpen:addVoice(520,.15,140);break;
        case SfxCue::Jump:addVoice(520,.22,120);break;case SfxCue::Spin:addVoice(180,.18,130,true);break;case SfxCue::Wumpa:addVoice(1040,.22,55);break;
        case SfxCue::Crate:addVoice(110,.30,110,true);break;case SfxCue::Checkpoint:addVoice(740,.24,180);addVoice(1110,.12,180);break;case SfxCue::Hurt:addVoice(95,.34,220,true);break;
        case SfxCue::Life:addVoice(660,.20,220);addVoice(990,.16,220);break;case SfxCue::Gem:addVoice(1175,.23,240);addVoice(1568,.14,240);break;case SfxCue::Clock:addVoice(900,.20,250);break;
        case SfxCue::Shot:addVoice(1250,.13,55);break;case SfxCue::Explosion:addVoice(70,.38,320,true);break;case SfxCue::EnemyDown:addVoice(160,.24,150,true);break;
        case SfxCue::BossHit:addVoice(90,.34,220,true);addVoice(360,.10,120);break;case SfxCue::LevelClear:addVoice(784,.20,380);addVoice(988,.16,380);addVoice(1319,.12,380);break;
        }
    }
    void fill(int index){
        MusicCue cue=music_.load();
        if(cue!=renderMusic_){renderMusic_=cue;musicClock_=0;phase1_=phase2_=bassPhase_=0;}
        std::array<double,Frames> gaxPcm{};
        if(gaxReady_){
            std::lock_guard<std::mutex> gg(gaxMutex_);
            gax_.select(cue);
            for(int i=0;i<Frames;i++)gaxPcm[i]=gax_.next()*.42;
        }
        const auto& m=motif(cue);const uint64_t stepSamples=Rate/6;
        for(int i=0;i<Frames;i++,sampleClock_++,musicClock_++){
            double mix=gaxReady_?gaxPcm[i]:0.0;
            if(!gaxReady_&&cue!=MusicCue::Silence){
                size_t st=size_t((musicClock_/stepSamples)%m.size());int note=m[st];int harmony=note?note+7:0;double f1=hz(note),f2=hz(harmony),fb=hz(bassFor(cue)+(int(st)%4==3?5:0));
                phase1_+=2.0*3.14159265358979323846*f1/Rate;phase2_+=2.0*3.14159265358979323846*f2/Rate;bassPhase_+=2.0*3.14159265358979323846*fb/Rate;
                double gate=((musicClock_%stepSamples)<stepSamples*5/6)?1.0:.15;mix+=(sq(phase1_)*.075+tri(phase2_)*.045+sq(bassPhase_)*.035)*gate;
            }
            {
                std::lock_guard<std::mutex> g(mutex_);
                for(auto& v:voices_){if(!v.left)continue;double env=double(v.left)/std::max(1u,v.total);double s=0;if(v.noise){v.seed=v.seed*1664525u+1013904223u;s=(double((v.seed>>8)&0xffff)/32767.5)-1.0;}else{s=std::sin(v.phase);v.phase+=2.0*3.14159265358979323846*v.freq/Rate;}mix+=s*v.amp*env;--v.left;}
                voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.left==0;}),voices_.end());
            }
            int v=int(std::lround(std::clamp(mix,-.95,.95)*32767.0));pcm_[index][i*2]=int16_t(v);pcm_[index][i*2+1]=int16_t(v);
        }
    }
    void loop(){
        for(int i=0;i<BufferCount;i++){fill(i);hdr_[i]={};hdr_[i].lpData=reinterpret_cast<LPSTR>(pcm_[i].data());hdr_[i].dwBufferLength=DWORD(pcm_[i].size()*sizeof(int16_t));waveOutPrepareHeader(out_,&hdr_[i],sizeof(WAVEHDR));waveOutWrite(out_,&hdr_[i],sizeof(WAVEHDR));}
        while(running_){bool did=false;for(int i=0;i<BufferCount;i++)if(hdr_[i].dwFlags&WHDR_DONE){fill(i);hdr_[i].dwFlags&=~WHDR_DONE;waveOutWrite(out_,&hdr_[i],sizeof(WAVEHDR));did=true;}if(!did)Sleep(3);}
    }
public:
    ~Win32Audio(){stop();}
    bool start(const Bytes* rom=nullptr){if(running_)return true;if(rom){gaxReady_=gax_.load(*rom);if(gaxReady_)gax_.select(music_.load());}WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_PCM;f.nChannels=2;f.nSamplesPerSec=Rate;f.wBitsPerSample=16;f.nBlockAlign=4;f.nAvgBytesPerSec=Rate*4;if(waveOutOpen(&out_,WAVE_MAPPER,&f,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){out_=nullptr;return false;}running_=true;worker_=std::thread([this]{loop();});return true;}
    void stop(){if(!running_)return;running_=false;if(worker_.joinable())worker_.join();if(out_){waveOutReset(out_);for(auto& h:hdr_)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(out_,&h,sizeof(WAVEHDR));waveOutClose(out_);out_=nullptr;}}
    void setMusic(MusicCue c){music_=c;}
    void sfx(SfxCue c){if(!running_)return;if(gaxReady_&&originalSfxId(c)>=0){std::lock_guard<std::mutex> gg(gaxMutex_);gax_.playSfx(c);}else synthSfx(c);}
    static SfxCue fallbackForOriginalSfx(unsigned id){
        switch(id){case 0x03:return SfxCue::Crate;case 0x04:return SfxCue::Explosion;case 0x07:return SfxCue::Life;case 0x0e:return SfxCue::Wumpa;case 0x18:return SfxCue::Clock;case 0x24:return SfxCue::Shot;case 0x25:return SfxCue::EnemyDown;case 0x2d:return SfxCue::Shot;case 0x2e:return SfxCue::Explosion;case 0x3b:return SfxCue::LevelClear;case 0x3e:return SfxCue::Confirm;case 0x43:case 0x45:return SfxCue::BossHit;default:return SfxCue::Explosion;}
    }
    void sfxId(unsigned id,unsigned volumeParam=0x100,int forcedVoice=-1){if(!running_)return;if(gaxReady_){std::lock_guard<std::mutex> gg(gaxMutex_);if(gax_.playSfxId(id,volumeParam,forcedVoice))return;}synthSfx(fallbackForOriginalSfx(id));}
    bool active()const{return running_.load();}
    bool originalGaxActive()const{return gaxReady_;}
};

}
#endif
