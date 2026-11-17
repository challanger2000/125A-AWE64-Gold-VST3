#include "P007P165View.hpp"
#include "P007Native.hpp"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <utility>

namespace p007 {
using namespace VSTGUI;
namespace {
// EXACT original P165 photographic faceplate and physical hardware
// CViews. The four class implementations below are copied unchanged from
// the actual user's P165 PresetBrowserView.cpp. NO new faceplate rendering.
constexpr CColor kBack(22,27,34,255), kRaised(42,52,63,255),
                 kPaper(226,230,233,255), kMuted(151,168,184,255),
                 kSel(51,86,103,255), kGrid(56,68,80,255),
                 kBezel(11,16,22,255), kStroke(85,100,113,255),
                 kOrange(220,151,87,255);
class FunctionalLayoutBackground final : public CView {
public:
    explicit FunctionalLayoutBackground(const CRect& r)
        : CView(r), photo_(new CBitmap("125A_P165_Original_Faceplate_1000x708.png")) {
        setMouseEnabled(false);
    }
    ~FunctionalLayoutBackground() noexcept override {
        if(photo_) photo_->forget();
    }
    void draw(CDrawContext* c) override {
        if(photo_ && photo_->isLoaded()) {
            photo_->draw(c,getViewSize());
        } else {
            // No silent P164/P160 substitute: a missing approved asset is
            // a packaging failure (also rejected in CMake/CI).
            c->setFillColor(CColor(13,17,23,255));
            c->drawRect(getViewSize(),kDrawFilled);
        }
        // One navigation rail. No header decorations or second LCD.
        const CRect rail{16,228,984,279};
        if(auto* p=c->createRoundRectGraphicsPath(rail,7.)) {
            c->setFillColor(CColor(9,12,16,205));
            c->drawGraphicsPath(p,CDrawContext::kPathFilled);
            c->setFrameColor(CColor(190,131,55,220));
            c->setLineWidth(1.);
            c->drawGraphicsPath(p,CDrawContext::kPathStroked);
            p->forget();
        }
        setDirty(false);
    }
private:
    CBitmap* photo_{nullptr};
};


class GoldButton final : public CTextButton {
public:
    GoldButton(const CRect& r,IControlListener* listener,int tag,
               const char* caption,CCoord sourceX)
        :CTextButton(r,listener,tag,caption),
         caption_(caption),sourceX_(sourceX),
         atlas_(new CBitmap("125A_Button_Atlas_Hardware_Normal_Pressed.png")) {}
    ~GoldButton() noexcept override {
        if(atlas_)atlas_->forget();
    }
    void draw(CDrawContext* c) override {
        if(!atlas_ || !atlas_->isLoaded()) {
            // Keep the controls usable even if a resource is missing;
            // GUI acceptance will explicitly reject this fallback.
            CTextButton::draw(c);
            return;
        }
        const bool active=getValue()>0.5f;
        atlas_->draw(c,getViewSize(),{sourceX_,active?44.:0.});
        c->setFont(getFont());
        c->setFontColor(active?CColor(255,232,173,255):
                              CColor(241,201,127,255));
        CRect textRect=getViewSize();
        // The keycap text follows the depressed surface by one pixel.
        if(active) {
            textRect.top+=1.;
            textRect.bottom+=1.;
        }
        c->drawString(caption_.c_str(),textRect,kCenterText);
        setDirty(false);
    }
private:
    std::string caption_;
    CCoord sourceX_;
    CBitmap* atlas_{nullptr};
};


class GoldHardwareMenu final : public COptionMenu {
public:
    GoldHardwareMenu(const CRect& rect, IControlListener* listener, int32_t tag)
        : COptionMenu(rect,listener,tag) {}
    // Externally selected MIDI programs can be absent from this SoundFont.
    // Preserve the real phdr list and show the absent ID on the closed
    // hardware display; never pretend the first existing patch was chosen.
    void SetUnavailableProgram(int program) {
        std::string next;
        if(program >= 0) {
            char label[48]{};
            std::snprintf(label,sizeof(label),"%03d  NICHT VORHANDEN",program);
            next=label;
        }
        if(program>=0) {
            // COptionMenu::setValue(-1) intentionally rejects -1. Clear
            // the protected native selected index explicitly: opening the
            // popup must not highlight an unrelated real program either.
            currentIndex=-1;
        }
        if(next!=unavailableProgram_) {
            unavailableProgram_=std::move(next);
            invalid();
        }
    }
    void draw(CDrawContext* ctx) override {
        ctx->saveGlobalState();
        const auto fillRounded=[&](const CRect& r,CCoord radius,
                                   const CColor& upper,const CColor& lower) {
            auto* p=ctx->createRoundRectGraphicsPath(r,radius);
            if(!p)return;
            auto* g=CGradient::create(0.,1.,upper,lower);
            ctx->fillLinearGradient(p,*g,{r.left,r.top},{r.left,r.bottom});
            g->forget();p->forget();
        };
        CRect r=getViewSize();
        r.inset(.25,.25);
        const bool thin=r.getHeight()<29.;
        const auto rad=thin?4.:6.;
        // Stable black chassis, bright metal relief, and inset dark display.
        fillRounded(r,rad+1.,CColor(7,7,9),CColor(7,7,9));
        CRect rim=r;rim.inset(1.,1.);
        fillRounded(rim,rad,CColor(246,204,119),CColor(112,70,29));
        CRect brass=rim;brass.inset(1.4,1.4);
        fillRounded(brass,rad-1.,CColor(97,61,32),CColor(235,163,69));
        CRect well=brass;well.inset(1.8,1.8);
        fillRounded(well,std::max(2.,rad-2.),
                    inPopup?CColor(8,8,9):CColor(18,17,17),
                    inPopup?CColor(43,29,16):CColor(36,28,22));
        // Fine bezel reflection and recessed inner shadow.
        ctx->setLineWidth(1.);
        ctx->setFrameColor(CColor(251,219,146,190));
        ctx->drawLine({rim.left+rad+1.,rim.top+1.},
                      {rim.right-rad-1.,rim.top+1.});
        ctx->setFrameColor(CColor(5,5,5,210));
        ctx->drawLine({well.left+rad,well.top+1.},
                      {well.right-rad,well.top+1.});
        const CCoord edge=well.right-(thin?21.:25.);
        ctx->setFrameColor(CColor(132,91,39,200));
        ctx->drawLine({edge,well.top+3.},{edge,well.bottom-3.});
        ctx->setFrameColor(CColor(25,17,12,255));
        ctx->drawLine({edge+1.,well.top+3.},{edge+1.,well.bottom-3.});
        // Arrow is part of the hardware surface, not an extra hit target.
        const CCoord ax=edge+(well.right-edge)*.50;
        const CCoord ay=(well.top+well.bottom)*.5;
        auto* arrow=ctx->createGraphicsPath();
        if(arrow) {
            const double dx=thin?4.5:5.5,dy=thin?2.7:3.3;
            arrow->beginSubpath({ax-dx,ay-dy*.5});
            arrow->addLine({ax+dx,ay-dy*.5});
            arrow->addLine({ax,ay+dy});
            arrow->closeSubpath();
            ctx->setFillColor(inPopup?CColor(255,222,151):CColor(234,180,90));
            ctx->drawGraphicsPath(arrow,CDrawContext::kPathFilled);
            arrow->forget();
        }
        // The currently selected item is always native data, never drawn
        // into a bitmap. Explicit clip keeps long SF2 names out of the arrow.
        const CMenuItem* item=getCurrent();
        if(item || !unavailableProgram_.empty()) {
            CRect textRect=well;
            textRect.left+=thin?8.:11.;
            textRect.right=edge-5.;
            ctx->setFont(getFont());
            ctx->setFontColor(CColor(244,200,118));
            ctx->setClipRect(textRect);
            const auto title=unavailableProgram_.empty()
                ? std::string(item->getTitle().data()) : unavailableProgram_;
            ctx->drawString(title.c_str(),textRect,kLeftText);
        }
        ctx->restoreGlobalState();
        setDirty(false);
    }
private:
    std::string unavailableProgram_;
};

static void StyleGoldMenu(COptionMenu* menu) {
    if(!menu)return;
    menu->setBackColor(CColor(22,19,18,255));
    menu->setFontColor(CColor(244,198,112,255));
    menu->setFrameColor(CColor(181,122,55,255));
    menu->setFont(menu->getViewSize().getHeight()<29.?kNormalFontSmall:kNormalFont);
}

class HardwareKnob final : public CControl {
public:
    HardwareKnob(const CRect& r,IControlListener* listener,int tag)
        :CControl(r,listener,tag,nullptr) {
        setMin(0.f);setMax(1.f);
        setDefaultValue(0.f);
        // Exactly 33 equal-size vertical frames. The larger master receives
        // a separate 134px PNG; no runtime image scaling or frame rotation.
        const bool isMaster=r.getWidth()>90.;
        frameSize_=isMaster?134.:58.;
        frames_=new CBitmap(isMaster
            ? "125A_Knob_Master_33frames_134px.png"
            : "125A_Knob_Standard_33frames_58px.png");
    }
    ~HardwareKnob() noexcept override {
        if(frames_)frames_->forget();
    }
    void draw(CDrawContext* ctx) override {
        if(frames_&&frames_->isLoaded()) {
            const int index=std::clamp(
                int(std::lround(double(getValue())*32.)),0,32);
            // VSTGUI source offset selects exactly one square frame.
            frames_->draw(ctx,getViewSize(),{0.,frameSize_*index});
            setDirty(false);
            return;
        }
        // Visible, functional fallback if a PNG resource is unavailable.
        // The Windows GUI acceptance explicitly rejects this fallback.
        const CRect outer=getViewSize();
        CRect inner=outer;inner.inset(4,4);
        CRect core=inner;core.inset(5,5);
        ctx->setFillColor(kBezel);
        ctx->drawEllipse(outer,kDrawFilled);
        ctx->setFrameColor(CColor(87,101,113,255));
        ctx->setLineWidth(2);
        ctx->drawEllipse(outer,kDrawStroked);
        ctx->setFillColor(CColor(58,70,81,255));
        ctx->drawEllipse(inner,kDrawFilled);
        ctx->setFrameColor(CColor(126,136,145,255));
        ctx->drawEllipse(inner,kDrawStroked);
        ctx->setFillColor(CColor(32,42,51,255));
        ctx->drawEllipse(core,kDrawFilled);
        ctx->setFrameColor(CColor(16,22,28,255));
        ctx->drawEllipse(core,kDrawStroked);
        const double cx=(outer.left+outer.right)*.5;
        const double cy=(outer.top+outer.bottom)*.5;
        const double angle=(135.0+270.0*getValue())*3.141592653589793/180.;
        ctx->setLineWidth(3);
        ctx->setFrameColor(getValue()>0.0001f?kOrange:kMuted);
        ctx->drawLine({cx+std::cos(angle)*8,cy+std::sin(angle)*8},
                      {cx+std::cos(angle)*(outer.getWidth()-9)*.5,
                       cy+std::sin(angle)*(outer.getHeight()-9)*.5});
        ctx->setLineWidth(1);
        setDirty(false);
    }
    CMouseEventResult onMouseDown(CPoint& where,const CButtonState& buttons) override {
        if(!(buttons&kLButton))return kMouseEventNotHandled;
        // Explicit Ctrl-click neutral/unity reset. Our custom drag handler
        // must not rely on an inherited CControl shortcut being invoked.
        if(buttons&kControl) {
            if(getValue()!=getDefaultValue()) {
                beginEdit();
                setValue(getDefaultValue());
                valueChanged();
                endEdit();
                invalid();
            }
            return kMouseEventHandled;
        }
        lastX_=where.x;lastY_=where.y;dragging_=true;
        beginEdit();
        return kMouseEventHandled;
    }
    CMouseEventResult onMouseMoved(CPoint& where,const CButtonState&) override {
        if(!dragging_)return kMouseEventNotHandled;
        const double delta=(lastY_-where.y)+.25*(where.x-lastX_);
        lastX_=where.x;lastY_=where.y;
        const auto next=std::clamp(float(getValue()+delta/170.),0.f,1.f);
        if(next!=getValue()) {setValue(next);valueChanged();invalid();}
        return kMouseEventHandled;
    }
    CMouseEventResult onMouseUp(CPoint&,const CButtonState&) override {
        if(dragging_) {dragging_=false;endEdit();}
        return kMouseEventHandled;
    }
    CMouseEventResult onMouseCancel() override {
        if(dragging_) {dragging_=false;endEdit();}
        return kMouseEventHandled;
    }
    CLASS_METHODS_NOCOPY(HardwareKnob,CControl)
private:
    CBitmap* frames_{nullptr};
    CCoord frameSize_{0.};
    bool dragging_=false;
    CCoord lastX_=0.,lastY_=0.;
};
} // namespace

P165View::P165View(const CRect& rect,
                   Steinberg::Vst::EditController* controller,
                   std::function<int()> originalGuestStatus)
  :CViewContainer(rect),controller_(controller),guestStatus_(std::move(originalGuestStatus)) {
    setBackgroundColor(kBack);
    // Same real P165 1000x708 front panel (no Sampler/keyboard).
    addView(new FunctionalLayoutBackground({0,0,1000,708}));
    auto* lcd=makeLabel({370,96,728,112},"EMU8000 / CREATIVE AWE64 GOLD",true);
    lcd->setFontColor(CColor(55,42,9,255));
    programText_=makeLabel({370,118,728,145},"BANK 000 / PROGRAM 000",false);
    programText_->setFont(kNormalFontBig);
    programText_->setFontColor(CColor(41,31,6,255));
    status_=makeLabel({370,145,728,166},"WINDOWS 95: NICHT AKTIV",true);
    status_->setFontColor(CColor(53,40,9,255));
    // Physically identical P165 selectors and 39px GoldButton sprites.
    bank_=new GoldHardwareMenu({26,234,203,273},this,kBank);
    StyleGoldMenu(bank_);
    program_=new GoldHardwareMenu({261,234,552,273},this,kProgram);
    StyleGoldMenu(program_);
    for(int bank=0;bank<=127;++bank) {
        char label[32]{};
        std::snprintf(label,sizeof(label),"Bank %03d",bank);
        bank_->addEntry(label);
    }
    for(int program=0;program<=127;++program) {
        char label[32]{};
        std::snprintf(label,sizeof(label),"%03d (Creative MIDI)",program);
        program_->addEntry(label);
    }
    addView(bank_);
    addView(program_);
    auto* prev=new GoldButton({212,234,252,273},this,kPrevious,"<",158.);
    auto* next=new GoldButton({561,234,601,273},this,kNext,">",201.);
    prev->setFont(kNormalFont);next->setFont(kNormalFont);
    addView(prev);addView(next);
    // Use the original P165 hardware master knob, not an invented design.
    auto* title=makeLabel({810,308,970,333},"MASTER VOLUME",true);
    title->setHoriAlign(kCenterText);
    title->setFontColor(CColor(241,201,127,255));
    master_=new HardwareKnob({817,353,951,487},this,kMaster);
    master_->setDefaultValue(1.f);
    master_->setValue(1.f);
    addView(master_);
    masterText_=makeLabel({814,505,959,528},"100 %",false);
    masterText_->setHoriAlign(kCenterText);
    // AWE envelope/filter and FX send controls require the original Creative
    // Win95 register/WinMM control bridge. Do not fake their functionality.
    // The old 125A custom synth DSP and preset engine must NOT be reused.
    auto* pending=makeLabel({28,610,968,638},
        "ORIGINAL CREATIVE: AMP / FILTER / REVERB / CHORUS - ANBINDUNG AUSSTEHEND",true);
    pending->setFontColor(CColor(208,165,94,255));
    pending->setHoriAlign(kCenterText);
    setWantsIdle(true);
    onIdle();
}
CTextLabel* P165View::makeLabel(CRect rect,const char* label,bool secondary) {
    auto* text=new CTextLabel(rect,label);
    text->setTransparency(true);
    text->setFont(secondary?kNormalFontSmall:kNormalFont);
    text->setFontColor(secondary?kMuted:kPaper);
    text->setTextTruncateMode(CTextLabel::kTruncateTail);
    addView(text);
    return text;
}
void P165View::sendValue(Steinberg::Vst::ParamID tag,double value,bool gesture) {
    if(!controller_)return;
    value=std::clamp(value,0.,1.);
    if(!gesture)controller_->beginEdit(tag);
    (void)controller_->setParamNormalized(tag,value);
    (void)controller_->performEdit(tag,value);
    if(!gesture)controller_->endEdit(tag);
}
void P165View::controlBeginEdit(CControl* c) {
    if(c&&c->getTag()==kMaster&&!masterEditing_&&controller_){
        masterEditing_=true;controller_->beginEdit(kMaster);
    }
}
void P165View::controlEndEdit(CControl* c) {
    if(c&&c->getTag()==kMaster&&masterEditing_&&controller_){
        masterEditing_=false;controller_->endEdit(kMaster);
    }
}
void P165View::valueChanged(CControl* control) {
    if(!control||!controller_)return;
    switch(control->getTag()) {
        case kPrevious:
        case kNext: {
            const auto program=std::clamp(
                int(std::lround(controller_->getParamNormalized(kProgram)*127.)),0,127);
            const int next=(program+(control->getTag()==kNext?1:127))%128;
            sendValue(kProgram,double(next)/127.0);
            break;
        }
        case kBank:
            sendValue(kBank,std::clamp(double(bank_->getValue())/127.0,0.,1.));
            break;
        case kProgram:
            sendValue(kProgram,std::clamp(double(program_->getValue())/127.0,0.,1.));
            break;
        case kMaster:
            sendValue(kMaster,master_->getValueNormalized(),masterEditing_);
            break;
        default:break;
    }
    onIdle();
}
void P165View::onIdle(){
    if(!controller_)return;
    const auto program=std::clamp(
        int(std::lround(controller_->getParamNormalized(kProgram)*127.)),0,127);
    const auto bank=std::clamp(
        int(std::lround(controller_->getParamNormalized(kBank)*127.)),0,127);
    const auto volume=std::clamp(
        int(std::lround(controller_->getParamNormalized(kMaster)*100.)),0,100);
    if(bank!=lastBank_||program!=lastProgram_) {
        if(bank_)bank_->setValue(static_cast<float>(bank));
        if(program_)program_->setValue(static_cast<float>(program));
        if(programText_){
            char text[64]{};
            std::snprintf(text,sizeof(text),"BANK %03d / PROGRAM %03d",bank,program);
            programText_->setText(text);
        }
        lastBank_=bank;lastProgram_=program;
    }
    if(volume!=lastMaster_){
        if(master_)master_->setValueNormalized(float(volume)*.01f);
        if(masterText_){
            char text[32]{};
            std::snprintf(text,sizeof(text),"%d %%",volume);
            masterText_->setText(text);
        }
        lastMaster_=volume;
    }
    const int state=guestStatus_?guestStatus_():0;
    if(state!=lastStatus_){
        const char* message=
            state==2 ? "ORIGINAL CREATIVE WINMM: BEREIT" :
            state==1 ? "ORIGINAL WINDOWS 95: STARTET" :
            state==3 ? "EMU8000: GAST WIRD BEENDET" :
            state<0 ? "WIN95 / CREATIVE: STARTFEHLER" :
                      "WIN95: NICHT AKTIV";
        if(status_)status_->setText(message);
        lastStatus_=state;
    }
    CViewContainer::onIdle();
}
} // namespace p007
