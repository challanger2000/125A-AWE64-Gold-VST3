#include "CreativeSbkDescriptor.h"
#include "CreativeSbkCandidate.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>
using namespace awe64::creative;
using Bytes=std::vector<std::uint8_t>;
#define CHECK(x) do { if(!(x)){std::cerr<<"P002_SBK_NATIVE_FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;} }while(0)
void b16(Bytes& b,unsigned v){b.push_back(v&255);b.push_back((v>>8)&255);}
void b32(Bytes& b,unsigned v){b16(b,v&65535);b16(b,v>>16);}
void ascii(Bytes& b,std::string_view s){b.insert(b.end(),s.begin(),s.end());}
void name20(Bytes& b,std::string_view s){ascii(b,s);b.insert(b.end(),20-s.size(),0);}
void chunk(Bytes& b,std::string_view name,const Bytes& data){ascii(b,name);b32(b,static_cast<unsigned>(data.size()));b.insert(b.end(),data.begin(),data.end());if(data.size()&1)b.push_back(0);}
void gen(Bytes& b,unsigned op,unsigned value){b16(b,op);b16(b,value);}
Bytes fixture(){
    // Entirely invented 1MGM-shaped descriptor. Contains NO original Creative bytes.
    Bytes phdr,pbag,pgen,inst,ibag,igen,shdr(288,0),info,pdta,root;
    name20(phdr,"TEST0");b16(phdr,0);b16(phdr,0);b16(phdr,0);b32(phdr,0);b32(phdr,0);b32(phdr,0);
    name20(phdr,"EOP");b16(phdr,0);b16(phdr,0);b16(phdr,1);b32(phdr,0);b32(phdr,0);b32(phdr,0);
    b16(pbag,0);b16(pbag,0);b16(pbag,4);b16(pbag,0);
    gen(pgen,48,107);gen(pgen,43,0x5A32);gen(pgen,44,0x7F20);gen(pgen,41,0);
    name20(inst,"ZONE");b16(inst,0);name20(inst,"EOI");b16(inst,3);
    for(unsigned i:{0u,1u,5u,7u}){b16(ibag,i);b16(ibag,0);}
    gen(igen,8,110); // invented global cutoff default
    gen(igen,43,0x423A);gen(igen,44,0x7F40);gen(igen,8,89);gen(igen,53,3); // key 58..66
    gen(igen,43,0x7F43);gen(igen,53,4); // key 67..127
    ascii(info,"INFO");chunk(info,"isng",Bytes{'E','M','U','8','0','0','0',0});
    chunk(info,"IPRD",Bytes{'S','B','A','W','E','3','2',0});
    chunk(info,"irom",Bytes{'1','M','G','M',0});
    ascii(pdta,"pdta");
    chunk(pdta,"phdr",phdr);chunk(pdta,"pbag",pbag);chunk(pdta,"pgen",pgen);
    chunk(pdta,"inst",inst);chunk(pdta,"ibag",ibag);chunk(pdta,"igen",igen);chunk(pdta,"shdr",shdr);
    chunk(root,"LIST",info);chunk(root,"LIST",pdta);
    Bytes full;ascii(full,"RIFF");b32(full,static_cast<unsigned>(root.size()+4));ascii(full,"sfbk");full.insert(full.end(),root.begin(),root.end());
    return full;
}
int main(){
    const auto raw=fixture();
    const auto p60=resolveCreativeSbkZone(raw,0,0,60,64);
    CHECK(p60 && p60->effective_raw_cutoff==89 && p60->chip_cutoff_candidate==178);
    CHECK(p60->instrument_zone_bag==1 && p60->key_low==58 && p60->key_high==66);
    CHECK(p60->global_raw_cutoff==110 && p60->zone_raw_cutoff==89);
    CHECK(p60->preset_attenuation_raw==107 && p60->sample_id_raw==3);
    const MidiMessage note{MidiKind::NoteOn,0,60,64,0};
    const auto pair=candidateSbkIfatnWrites(note,3,*p60,UnresolvedCreativeVoiceTerms{16,0,false});
    CHECK(pair && (*pair)[0].port==0xE22 && (*pair)[0].value==0x23);
    CHECK((*pair)[1].port==0xE20 && (*pair)[1].value==0xB248);
    auto changedKey=note;changedKey.data1=67;
    CHECK(!candidateSbkIfatnWrites(changedKey,3,*p60,UnresolvedCreativeVoiceTerms{16,0,false}));
    const auto p67=resolveCreativeSbkZone(raw,0,0,67,64);
    CHECK(p67 && p67->effective_raw_cutoff==110 && p67->chip_cutoff_candidate==220);
    CHECK(p67->instrument_zone_bag==2 && !p67->zone_raw_cutoff);
    CHECK(!resolveCreativeSbkZone(raw,0,0,60,63)); // instrument velocity split
    CHECK(!resolveCreativeSbkZone(raw,0,0,67,20)); // preset velocity split
    CHECK(!resolveCreativeSbkZone(raw,0,0,91,64)); // preset key scope
    CHECK(!resolveCreativeSbkZone(raw,0,0,60,0));  // NOTE OFF, no voice
    auto changedVelocity=note;changedVelocity.data2=63;
    CHECK(!candidateSbkIfatnWrites(changedVelocity,3,*p60,UnresolvedCreativeVoiceTerms{16,0,false}));
    const auto pair67=candidateSbkIfatnWrites(changedKey,3,*p67,UnresolvedCreativeVoiceTerms{16,0,false});
    CHECK(pair67 && ((*pair67)[1].value>>8)==220);
    CHECK(!resolveCreativeSbkZone(raw,0,0,57,64));
    CHECK(!resolveCreativeSbkZone(raw,1,0,60,64));
    CHECK(!resolveCreativeSbkZone(raw,0,40,60,64));
    CHECK(!resolveCreativeSbkZone(raw,0,0,128,64));
    auto broken=raw;broken.pop_back();CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    broken=raw;broken[8]='x';CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    broken=raw;
    auto pos=std::search(broken.begin(),broken.end(),std::begin("SBAWE32") ,std::end("SBAWE32")-1);
    CHECK(pos!=broken.end());*pos='X';CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    broken=raw;
    // Overlapping key zones must fail rather than silently picking the first.
    const Bytes pattern{43,0,0x43,0x7f,53,0,4,0};
    auto it=std::search(broken.begin(),broken.end(),pattern.begin(),pattern.end());
    CHECK(it!=broken.end());it[2]=60;CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    broken=raw;
    // Reject a generic 46-byte shdr interpretation.
    const Bytes shdrTag{'s','h','d','r',32,1,0,0};
    it=std::search(broken.begin(),broken.end(),shdrTag.begin(),shdrTag.end());
    CHECK(it!=broken.end());it[4]=184;it[5]=0;CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    broken=raw;
    const Bytes ibagTag{'i','b','a','g',16,0,0,0};
    it=std::search(broken.begin(),broken.end(),ibagTag.begin(),ibagTag.end());
    CHECK(it!=broken.end());it[8+8]=0xff;CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    broken=raw;
    // Creative 1MGM generator 53 uses ROM IDs, not SF2 shdr array indices.
    const Bytes sampleTag{53,0,4,0};
    it=std::search(broken.begin(),broken.end(),sampleTag.begin(),sampleTag.end());
    CHECK(it!=broken.end());it[2]=151;
    const auto rom_id=resolveCreativeSbkZone(broken,0,0,67,64);
    CHECK(rom_id && rom_id->sample_id_raw==151);
    broken=raw;
    // Ordinary sfbk or a different ROM descriptor cannot use this 1MGM-only path.
    const Bytes romTag{'1','M','G','M'};
    it=std::search(broken.begin(),broken.end(),romTag.begin(),romTag.end());
    CHECK(it!=broken.end()); *it='X';
    CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    broken=raw;
    // Invalid preset key-range encoding (low>high).
    const Bytes presetRange{43,0,0x32,0x5A};
    it=std::search(broken.begin(),broken.end(),presetRange.begin(),presetRange.end());
    CHECK(it!=broken.end());it[2]=127;it[3]=1;
    CHECK(!resolveCreativeSbkZone(broken,0,0,60,64));
    std::cout<<"P002_NATIVE_CREATIVE_SBK_ZONE_CONTRACT_PASS synthetic_keys=2 no_real_bank_bytes=TRUE\n";
}
