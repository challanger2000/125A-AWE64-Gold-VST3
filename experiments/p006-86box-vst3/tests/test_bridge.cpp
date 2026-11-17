#include "P006Transport.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <atomic>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"P006_FAIL:%d %s\n",__LINE__,#x);std::abort();}}while(false)
int main(){
 auto o=std::make_unique<P006AudioShared>();p006::init(*o);
 CHECK(p006::valid(*o));
 float l[100]{},r[100]{};
 CHECK(p006::read(*o,l,r,100)==0);
 CHECK(p006::load(o->underrun_frames)==100);
 std::array<float,200> input{};
 for(unsigned i=0;i<100;++i){input[2*i]=float(i)/100.f;input[2*i+1]=-input[2*i];}
 CHECK(p006::write(*o,input.data(),100)==100);
 CHECK(p006::read(*o,l,r,100)==100);
 for(unsigned i=0;i<100;++i)CHECK(l[i]==input[2*i]&&r[i]==input[2*i+1]);
 std::array<float,2000> buf{};
 for(unsigned k=0;k<70;++k) {
   CHECK(p006::write(*o,buf.data(),1000)==1000);
   for(unsigned j=0;j<10;++j)CHECK(p006::read(*o,l,r,100)==100);
 }
 std::array<float,2*P006_AUDIO_CAPACITY> full{};
 full[0]=.42f;full[1]=-.42f;
 CHECK(p006::write(*o,full.data(),P006_AUDIO_CAPACITY)==P006_AUDIO_CAPACITY);
 CHECK(p006::write(*o,full.data(),200)==0);
 CHECK(p006::load(o->overrun_frames)==200);
 CHECK(p006::read(*o,l,r,100)==100);
 CHECK(l[0]==.42f&&r[0]==-.42f);
 p006::init(*o);
 constexpr unsigned frames=64,blocks=10000;
 std::array<float,frames*2> msg{};
 for(unsigned i=0;i<frames*2;++i)msg[i]=i%2?-.1f:.1f;
 std::atomic<bool> running{true};
 std::thread producer([&]{
   for(unsigned b=0;b<blocks;){
      if(p006::write(*o,msg.data(),frames)==frames)++b;
      else std::this_thread::yield();
   }
   running.store(false);
 });
 std::array<float,frames> a{},b{};
 unsigned got=0;
 while(running.load()||
       p006::load(o->write_frames)!=p006::load(o->read_frames)){
   const auto n=p006::read(*o,a.data(),b.data(),frames);
   for(unsigned i=0;i<n;++i)CHECK(a[i]==.1f&&b[i]==-.1f);
   got+=n;
   if(n==0)std::this_thread::yield();
 }
 producer.join();
 CHECK(got==frames*blocks);
 P006MidiPacket packet{};
 CHECK(p006::pack(packet,1,0x93,60,127));
 CHECK(p006::validate(packet));packet.crc^=1;CHECK(!p006::validate(packet));
 CHECK(!p006::pack(packet,1,0xf0,60,100));
 CHECK(!p006::pack(packet,1,0x93,60,128));
 std::puts("P006_SPSC_AUDIO_44100_STEREO_AND_MIDI_FRAMING_PASS");
}
