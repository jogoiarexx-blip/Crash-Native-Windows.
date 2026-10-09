#include "gax_rom.hpp"
#include <iomanip>
#include <iostream>
int main(int argc,char** argv){try{using namespace crash;
    const char* path=argc>1?argv[1]:"input/crash.gba";auto rom=read(path);size_t periodTable=findGaxPeriodTableOffset(rom);size_t table=0;auto songs=parseGaxSongs(rom,&table);auto tracks=scanGaxTracks(rom);auto waves=parseWaveTable(rom,songs[0].waveTablePtr,30);auto sfx=parseGaxSfxTable(rom,table);uint32_t sfxData=findGaxSfxSongData(rom,songs[0]);
    auto musicBank=parseInstrumentBank(rom,songs[0].instrumentTablePtr,52);auto ms=gaxBankStats(musicBank);
    std::cout<<"Engine: "<<gaxVersion(rom)<<"\nSong table ROM offset: 0x"<<std::hex<<std::uppercase<<table<<"\nPeriod table ROM offset: 0x"<<periodTable<<" | first=0x"<<u32(rom,periodTable)<<std::dec<<"\n";
    std::cout<<"Songs: "<<songs.size()<<" | metadata tags: "<<tracks.size()<<" | music PCM waves: "<<(waves.size()?waves.size()-1:0)<<" | SFX routes: "<<sfx.size()<<"\n";
    std::cout<<"Music instruments: "<<ms.valid<<" | envelopes: "<<ms.envelopes<<" | vibrato: "<<ms.vibrato<<" | multi-sequence: "<<ms.multiSequence<<" | loops: "<<ms.loopRows<<" | ping-pong: "<<ms.pingPongRows<<" | sweeps: "<<ms.sweepRows<<"\n";
    std::cout<<"Mixer: song-native rate/Q11 nearest-neighbor -> signed 16-bit wrap -> original ARM downmix/clamp -> 44.1 kHz host hold\n";
    std::cout<<"Jetpack shot: ambient SFX 0x"<<std::hex<<std::uppercase<<originalSfxId(SfxCue::Shot)<<std::dec<<" | voice="<<originalSfxVoice(SfxCue::Shot)<<" | volumeParam=0x"<<std::hex<<std::uppercase<<originalSfxVolumeParam(SfxCue::Shot)<<std::dec<<"\n";
    if(sfxData){auto sfxBank=parseInstrumentBank(rom,gaxU32(rom,sfxData+0x10),88);auto ss=gaxBankStats(sfxBank);std::cout<<"SFX SongData: 0x"<<std::hex<<std::uppercase<<sfxData<<"  instruments: 0x"<<gaxU32(rom,sfxData+0x10)<<"  waves: 0x"<<gaxU32(rom,sfxData+0x14)<<std::dec<<"\n";std::cout<<"SFX instruments: "<<ss.valid<<" | envelopes: "<<ss.envelopes<<" | loop rows: "<<ss.loopRows<<"\n";}
    for(const auto& s:songs){
        std::cout<<std::setw(2)<<s.id<<"  0x"<<std::hex<<std::uppercase<<s.songPtr<<std::dec<<"  "<<std::left<<std::setw(18)<<s.title<<std::right<<" ch="<<s.channels<<" rows="<<s.patternRows<<" orders="<<s.orderCount<<" rate="<<s.mixRate<<" Hz";
        if(s.dspTaps[0].delayMs)std::cout<<"  echo="<<s.dspPrimaryChannels<<"ch/"<<s.dspTaps[0].delayMs<<"ms@"<<s.dspTaps[0].gainQ4<<"/16";
        std::cout<<"\n";
    }
    return songs.size()==19?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
