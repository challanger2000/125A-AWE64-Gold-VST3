// P019: PRIVATE owner Windows binary PCM comparator.
// No Python/SDK/host/other program is required at the user's machine.
// Consumes independent 980 vs 128/64-window int16 stereo chip captures.
// Reports exact sample count and RMS/peak delta after FIFO delay alignment.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void need(bool okay,const char* message){
    if(!okay)throw std::runtime_error(message);
}
std::vector<std::int16_t> load(const char* path){
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    need(bool(in),"PCM file not found");
    const auto length=in.tellg();
    need(length>0 && length%4==0 && length<=20*1024*1024,
         "PCM file length out of bounds (expect stereo int16)");
    std::vector<std::int16_t> samples(static_cast<size_t>(length)/2);
    in.seekg(0);
    in.read(reinterpret_cast<char*>(samples.data()),
            static_cast<std::streamsize>(length));
    need(bool(in),"PCM read failed");
    return samples;
}
}
int main(int argc,char** argv){
 try{
    need(argc==5,
      "Usage: P019ComparePCM.exe ref980.pcm candidate.pcm 128|64 CASE_LABEL");
    const int shorter=std::stoi(argv[3]);
    need(shorter==128||shorter==64,"Unknown chip window");
    const auto reference=load(argv[1]);
    const auto candidate=load(argv[2]);
    const size_t shiftA=980u*2u,shiftB=static_cast<size_t>(shorter)*2u;
    need(reference.size()>shiftA && candidate.size()>shiftB,
         "Missing chip FIFO lead-in");
    const size_t count=std::min(reference.size()-shiftA,
                                  candidate.size()-shiftB);
    need(count>=100000u && count%2==0,
         "Insufficient aligned stereo PCM for original chip comparison");
    double refEnergy=0.,errorEnergy=0.;
    unsigned long long changed=0;
    int peakError=0;
    for(size_t i=0;i<count;++i){
        const auto a=static_cast<int>(reference[shiftA+i]);
        const auto b=static_cast<int>(candidate[shiftB+i]);
        const int d=a-b;
        refEnergy+=static_cast<double>(a)*a;
        errorEnergy+=static_cast<double>(d)*d;
        if(d){++changed;peakError=std::max(peakError,std::abs(d));}
    }
    need(refEnergy>100000000., "Aligned original chip PCM is SILENT");
    const double rms=std::sqrt(errorEnergy/static_cast<double>(count));
    std::printf("P019_PRIVATE_PCM_COMPARE_%s label=%s"
      " window=%d reference=980 compared_stereo_values=%llu"
      " differing_values=%llu max_lsb=%d rms_lsb=%.9f\n",
      changed?"DIFFERENT_NO_GO":"BIT_EXACT_PASS",
      argv[4],shorter,static_cast<unsigned long long>(count),
      changed,peakError,rms);
    return changed?1:0;
 }catch(const std::exception& e){
    std::fprintf(stderr,"P019_PRIVATE_COMPARATOR_INVALID %s\n",e.what());
    return 2;
 }
}
