// P216: real pinned 86Box C EMU8000, synthetic ROM, two chip instances.
// Prove a future chip port write cannot take effect before its target sample;
// rollover over native DSP blocks, MIDI message overtaking and C++ heap-free.
#include "Emu8000Box.h"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
static std::atomic<unsigned long long> g_cppNew{0};
void* operator new(std::size_t n) {g_cppNew.fetch_add(1, std::memory_order_relaxed);if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p) noexcept {std::free(p);}
void operator delete(void*p, std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n) {g_cppNew.fetch_add(1, std::memory_order_relaxed);if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete[](void*p) noexcept {std::free(p);}
void operator delete[](void*p, std::size_t) noexcept {std::free(p);}
static void req(bool b,const char* msg){if(!b) throw std::runtime_error(msg);}
static void tick(Emu8000Box& b,unsigned n){for(unsigned i=0;i<n;++i){int32_t l=0,r=0;b.RenderFrame(l,r);}}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 try {
  Emu8000Box b;std::string err;
  req(b.Init(argv[1],0x620,28672,err),"P216 86Box ROM init failed");
  Emu8000Box other;
  req(other.Init(argv[1],0x620,28672,err),"P216 second independent chip init failed");
  other.PortWrite(0xE22,0x0000u);
  const auto otherBaseline=other.PortRead(0xE20);
  const uint16_t otherTarget=otherBaseline^0x6666u;
  const auto postInitCppNew=g_cppNew.load(std::memory_order_relaxed);
  b.PortWrite(0xE22,0x0000u);
  const uint16_t before=b.PortRead(0xE20);
  const uint16_t target=before^0x55AAu;
  req(b.PortWriteTimed(0xE20,target,3),"P216 timed 3-frame queue rejected");
  req(b.PortRead(0xE20)==before,"P216 future data applied prematurely at frame0");
  tick(b,2);
  req(b.PortRead(0xE20)==before,"P216 future data leaked at frame2");
  tick(b,1);
  req(b.PortRead(0xE20)==target,"P216 due data not visible at frame3");
  const uint16_t second=target^0x2222u;
  req(b.PortWriteTimed(0xE20,second,66),"P216 future across-block write rejected");
  tick(b,65);
  req(b.PortRead(0xE20)==target,"P216 next-block future write leaked at 65");
  tick(b,1);
  req(b.PortRead(0xE20)==second,"P216 future write did not survive block rollover");
  // Later NOW MIDI writes sort before an older queued FUTURE note; each
  // same-frame original port select/data order remains stable.
  req(b.PortWriteTimed(0xE20,0x1234u,5),"P216 late write rejected");
  b.PortWrite(0xE20,0x5678u);
  req(b.PortRead(0xE20)==0x5678u,"P216 immediate MIDI write trapped behind future note");
  tick(b,5);
  req(b.PortRead(0xE20)==0x1234u,"P216 future after immediate write changed order");
  req(!b.PortWriteTimed(0xE20,0,100000u),"P216 overflow/latency bound not rejected");
  req(b.PortWriteTimed(0xE20,0x1357u,5),"P216 instance A scheduled write rejected");
  req(other.PortWriteTimed(0xE20,otherTarget,2),"P216 instance B scheduled write rejected");
  tick(other,2);
  req(other.PortRead(0xE20)==otherTarget,"P216 second independent chip did not advance");
  req(b.PortRead(0xE20)==0x1234u,"P216 second chip incorrectly advanced first chip");
  tick(b,5);
  req(b.PortRead(0xE20)==0x1357u,"P216 first chip not independent after B advanced");
  const auto afterNew=g_cppNew.load(std::memory_order_relaxed);
  req(afterNew==postInitCppNew,"P216 C++ heap allocation during timed port IO or render");
  std::puts("P216_ORIGINAL_86BOX_TIMED_PORTS_2_INSTANCES_DUE_ONLY_CROSS_BLOCK_ZERO_CPP_HEAP_PASS");
  return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"P216_NO_GO %s\n",e.what());return 1;}
}
