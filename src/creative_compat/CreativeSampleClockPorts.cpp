#include "CreativeSampleClockPorts.h"
#include <algorithm>
#include <limits>
namespace awe64::creative {
bool SourceClockedPortPlayer::stage(
    awe64::Chip& chip,const std::span<const SourceClockedPort> source) noexcept {
    if(!chip.ready() || started_ || source.empty() ||
       source.size()>kMaxEvents || source.front().relative_frame!=0u)
        return false;
    std::uint32_t previous=0u;
    for(const auto& event:source) {
        if(event.relative_frame<previous ||
           event.relative_frame>44100u*30u ||
           (event.port!=0xe22u && event.port!=0xe20u &&
            event.port!=0xa20u && event.port!=0xa22u &&
            event.port!=0x620u && event.port!=0x622u))
            return false;
        previous=event.relative_frame;
    }
    std::copy(source.begin(),source.end(),events_.begin());
    count_=source.size();
    next_=0u;
    first_frame_=chip.renderedFrames();
    started_=true;
    timing_error_=false;
    return true;
}
void SourceClockedPortPlayer::render(
    awe64::Chip& chip,std::int32_t* stereo,const std::size_t frames) noexcept {
    if(!stereo || !chip.ready() || frames==0u)return;
    if(timing_error_) {chip.render(stereo,frames);return;}
    std::size_t done=0u;
    while(done<frames) {
        const auto now=chip.renderedFrames();
        if(started_ && next_<count_) {
            const auto due=first_frame_+events_[next_].relative_frame;
            if(due<now) {
                // A caller who rendered past an original command deadline
                // cannot silently shift that command into a later frame.
                // Never execute a late source OUT or report parity success.
                timing_error_=true;
                chip.render(stereo+2u*done,frames-done);
                return;
            }
            if(due==now) {
                // Keep original same-frame OUT ordering, without sorting.
                do {
                    chip.writePort(events_[next_].port,events_[next_].value);
                    ++next_;
                }while(next_<count_ &&
                       first_frame_+events_[next_].relative_frame<=now);
                continue;
            }
            const auto delta=due-now;
            const auto chunk=static_cast<std::size_t>(
                std::min<std::uint64_t>(frames-done,delta));
            chip.render(stereo+2u*done,chunk);
            done+=chunk;
        }else {
            chip.render(stereo+2u*done,frames-done);
            break;
        }
    }
}
}
