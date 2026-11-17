#include "P007P165View.hpp"
#include "P007Native.hpp"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include <algorithm>
#include <array>
#include <cctype>
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


class BrowserPanelBacking final : public CView {
public:
    explicit BrowserPanelBacking(const CRect& r):CView(r) {setMouseEnabled(false);}
    void draw(CDrawContext* c) override {
        // Cut both lower corners away from the PCB's gold screw heads.
        auto* border=c->createGraphicsPath();
        if(border) {
            border->beginSubpath({16,2});
            for(const CPoint p:std::initializer_list<CPoint>{
                {984,2},{984,319},{969,318},{957,321},{948,329},{944,341},{944,356},
                {58,356},{58,343},{55,332},{47,324},{32,318},{20,321},{16,326}
            }) border->addLine(p);
            border->closeSubpath();
        }
        if(border) {
            // Keep the whole PCB visible at the same overall opacity as P160.
            // The muted gold edge is shared with the SYNTH control islands.
            c->setFillColor(CColor(7,11,16,153));
            c->drawGraphicsPath(border,CDrawContext::kPathFilled);
            c->setFrameColor(CColor(190,131,55,220));
            c->setLineWidth(1.);
            c->drawGraphicsPath(border,CDrawContext::kPathStroked);
            border->forget();
        }
        // P163: the rail's last button and BOTH recessed gold-field right
        // edges finish at X=974 (10px inside the X=984 outer bezel).
        // Preserve 394px width of the bank and search control surfaces.
        // Bezel views remain non-interactive; input controls are unchanged.
        const auto recessed=[&](const CRect& r,CCoord radius,
                                 CColor backing,CColor edge) {
            auto* p=c->createRoundRectGraphicsPath(r,radius);
            if(!p)return;
            c->setFillColor(backing);
            c->drawGraphicsPath(p,CDrawContext::kPathFilled);
            c->setFrameColor(edge);
            c->setLineWidth(1.3);
            c->drawGraphicsPath(p,CDrawContext::kPathStroked);
            p->forget();
            c->setFrameColor(CColor(250,204,126,195));
            c->setLineWidth(1.);
            c->drawLine({r.left+radius+3.,r.top+2.},
                        {r.right-radius-3.,r.top+2.});
        };
        recessed({580,15,974,52},6.,CColor(11,13,15,230),
                 CColor(192,132,58,245)); // search
        recessed({24,63,974,349},7.,CColor(11,14,16,242),
                 CColor(191,136,65,240)); // program table
        setDirty(false);
    }
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
// P165 photograph stays untouched. Original P165 gold/black translucent
// island is redrawn ONLY behind the still-functional host master knob.
// Empty AMP/FILTER/FX islands are not painted as if they were operational.
class MasterHardwareBacking final : public CView {
public:
    explicit MasterHardwareBacking(const CRect& r):CView(r){setMouseEnabled(false);}
    void draw(CDrawContext* c) override {
        auto* p=c->createGraphicsPath();
        if(!p)return;
        p->beginSubpath({784,288});
        for(const CPoint q:std::array<CPoint,9>{{
            {984,288},{984,605},{969,604},{957,607},
            {948,615},{944,628},{944,646},{784,646},{784,288}
        }})p->addLine(q);
        p->closeSubpath();
        c->setFillColor(CColor(7,11,16,153));
        c->drawGraphicsPath(p,CDrawContext::kPathFilled);
        c->setFrameColor(CColor(190,131,55,226));
        c->setLineWidth(1.);
        c->drawGraphicsPath(p,CDrawContext::kPathStroked);
        p->forget();
        setDirty(false);
    }
};

class BrowserTitleInset final : public CView {
public:
    explicit BrowserTitleInset(const CRect& r):CView(r){setMouseEnabled(false);}
    void draw(CDrawContext* c) override {
        const CRect recess{24,15,574,52};
        if(auto* border=c->createRoundRectGraphicsPath(recess,6.)){
            c->setFillColor(CColor(11,13,15,231));
            c->drawGraphicsPath(border,CDrawContext::kPathFilled);
            c->setFrameColor(CColor(192,132,58,245));
            c->setLineWidth(1.3);
            c->drawGraphicsPath(border,CDrawContext::kPathStroked);
            border->forget();
        }
        setDirty(false);
    }
};
class PendingCreativeBacking final : public CView {
public:
    explicit PendingCreativeBacking(const CRect& r):CView(r){setMouseEnabled(false);}
    void draw(CDrawContext* c) override {
        CRect bg{112,608,774,642};
        if(auto* p=c->createRoundRectGraphicsPath(bg,5.)){
            c->setFillColor(CColor(7,11,16,225));
            c->drawGraphicsPath(p,CDrawContext::kPathFilled);
            c->setFrameColor(CColor(190,131,55,190));
            c->setLineWidth(1.);
            c->drawGraphicsPath(p,CDrawContext::kPathStroked);
            p->forget();
        }
        setDirty(false);
    }
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
    synthButton_=new GoldButton({610,234,703,273},this,kSynthPage,"SYNTH",246.);
    browserButton_=new GoldButton({712,234,811,273},this,kBrowserPage,"BROWSER",342.);
    synthButton_->setStyle(CTextButton::kOnOffStyle);
    browserButton_->setStyle(CTextButton::kOnOffStyle);
    synthButton_->setFont(kNormalFont);
    browserButton_->setFont(kNormalFont);
    addView(synthButton_);addView(browserButton_);
    addView(new MasterHardwareBacking({0,0,1000,708}));
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
    addView(new PendingCreativeBacking({0,0,1000,708}));
    auto* pending=makeLabel({120,612,766,637},
        "AMP / FILTER / FX: ORIGINALE CREATIVE-ANBINDUNG NOCH OFFEN",true);
    pending->setFontColor(CColor(208,165,94,255));
    pending->setHoriAlign(kCenterText);
    // Original P165 browser sheet covers only the lower half of the
    // photograph. The existing top LCD, bank and program rail remain live.
    browserPanel_=new CViewContainer({0,287,1000,647});
    browserPanel_->setTransparency(true);
    addView(browserPanel_);
    browserPanel_->addView(new BrowserPanelBacking({0,0,1000,360}));
    browserPanel_->addView(new BrowserTitleInset({0,0,1000,360}));
    auto* browserTitle=new CTextLabel({36,17,565,49},
        "MIDI-PROGRAMME 000-127  |  PATCH-NAMEN NOCH UNBEKANNT");
    browserTitle->setTransparency(true);
    browserTitle->setFont(kNormalFontSmall);
    browserTitle->setFontColor(CColor(228,184,112,255));
    browserPanel_->addView(browserTitle);
    browserSearch_=new CTextEdit({588,17,968,50},this,kSearch,"");
    browserSearch_->setPlaceholderString("Programmnummer suchen...");
    browserSearch_->setImmediateTextChange(true);
    browserSearch_->setBackColor(CColor(18,19,18,255));
    browserSearch_->setFontColor(CColor(244,204,136,255));
    browserSearch_->setFrameColor(CColor(18,19,18,255));
    browserSearch_->setFrameWidth(0.);
    browserSearch_->setTextInset({11.,0.});
    browserPanel_->addView(browserSearch_);
    browserList_=new CDataBrowser({28,67,970,344},this,
        CScrollView::kVerticalScrollbar|CScrollView::kAutoHideScrollbars);
    browserList_->setBackgroundColor(CColor(18,21,22,255));
    if(auto* sb=browserList_->getVerticalScrollbar()){
        sb->setScrollerColor(CColor(181,129,66,255));
        sb->setBackgroundColor(CColor(17,19,20,255));
        sb->setFrameColor(CColor(114,80,43,255));
    }
    browserList_->setWantsFocus(true);
    browserPanel_->addView(browserList_);
    browserHelp_=new CTextLabel({36,90,960,132},
        "Keine passenden MIDI-Programmnummern.");
    browserHelp_->setTransparency(true);
    browserHelp_->setFontColor(CColor(217,168,91,255));
    browserPanel_->addView(browserHelp_);
    rebuildPrograms();
    setPage(0);
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
        case kSynthPage:
            if(control->getValue()>0.5f)setPage(0);
            else setPage(page_);
            break;
        case kBrowserPage:
            if(control->getValue()>0.5f)setPage(1);
            else setPage(page_);
            break;
        case kSearch:
            rebuildPrograms();
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
        syncBrowserSelection(program);
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
void P165View::setPage(int page) {
    page_=std::clamp(page,0,1);
    if(browserPanel_)browserPanel_->setVisible(page_==1);
    if(synthButton_){
        synthButton_->setValue(page_==0?1.f:0.f);
        synthButton_->invalid();
    }
    if(browserButton_){
        browserButton_->setValue(page_==1?1.f:0.f);
        browserButton_->invalid();
    }
}
void P165View::rebuildPrograms() {
    std::string query=browserSearch_
        ?std::string(browserSearch_->getText().data()):std::string();
    std::transform(query.begin(),query.end(),query.begin(),
        [](unsigned char c){return char(std::tolower(c));});
    visiblePrograms_.clear();
    for(int program=0;program<128;++program){
        char label[16]{};
        std::snprintf(label,sizeof(label),"%03d",program);
        const std::string num(label);
        // Only true original guest numerical program IDs are claimed.
        // An SF2 preset name would require actual guest bank metadata;
        // don't substitute guessed GM or previous 125A DSP preset labels.
        if(query.empty() || num.find(query)!=std::string::npos ||
           std::to_string(program).find(query)!=std::string::npos ||
           std::string("midi").find(query)!=std::string::npos)
            visiblePrograms_.push_back(program);
    }
    if(browserList_)browserList_->recalculateLayout();
    if(browserHelp_)browserHelp_->setVisible(visiblePrograms_.empty());
    const int current=controller_?std::clamp(
        int(std::lround(controller_->getParamNormalized(kProgram)*127.)),0,127):0;
    syncBrowserSelection(current);
}
void P165View::syncBrowserSelection(int program) {
    if(!browserList_)return;
    int row=-1;
    for(size_t i=0;i<visiblePrograms_.size();++i)
        if(visiblePrograms_[i]==program){row=int(i);break;}
    syncingRows_=true;
    if(row>=0)browserList_->setSelectedRow(row,true);
    else browserList_->unselectAll();
    syncingRows_=false;
}
int32_t P165View::dbGetNumRows(CDataBrowser*) {
    return static_cast<int32_t>(visiblePrograms_.size());
}
int32_t P165View::dbGetNumColumns(CDataBrowser*) {return 1;}
CCoord P165View::dbGetRowHeight(CDataBrowser*) {return 27.;}
CCoord P165View::dbGetCurrentColumnWidth(int32_t,CDataBrowser* browser) {
    if(!browser)return 942.;
    const CCoord width=browser->getVisibleClientRect().getWidth();
    return width>0. ? width : 942.;
}
void P165View::dbDrawCell(CDrawContext* c,const CRect& r,
                          int32_t row,int32_t,int32_t flags,CDataBrowser*) {
    if(row<0||size_t(row)>=visiblePrograms_.size())return;
    const bool selected=(flags&IDataBrowserDelegate::kRowSelected)!=0;
    c->setFillColor(selected ? CColor(78,56,27,255)
                            : (row%2 ? CColor(21,23,22,255)
                                     : CColor(17,20,20,255)));
    c->drawRect(r,kDrawFilled);
    if(selected){
        c->setFillColor(CColor(235,178,83,255));
        c->drawRect({r.left,r.top,r.left+3.,r.bottom},kDrawFilled);
    }
    const int program=visiblePrograms_[size_t(row)];
    char label[72]{};
    std::snprintf(label,sizeof(label),
                  "%03d     MIDI-PROGRAMM %03d",
                  program,program);
    c->setFont(kNormalFontSmall);
    c->setFontColor(selected?CColor(255,225,161,255)
                            :CColor(229,205,162,255));
    CRect textRect=r;textRect.left+=11.;textRect.right-=5.;
    c->drawString(label,textRect,kLeftText);
    c->setFrameColor(CColor(86,66,41,155));
    c->setLineWidth(1.);
    c->drawLine({r.left+6.,r.bottom-1.},{r.right-5.,r.bottom-1.});
}
CMouseEventResult P165View::dbOnMouseDown(const CPoint&,
    const CButtonState&,int32_t row,int32_t,CDataBrowser* table) {
    if(row>=0&&size_t(row)<visiblePrograms_.size()){
        const int program=visiblePrograms_[size_t(row)];
        syncingRows_=true;
        if(table)table->setSelectedRow(row,true);
        syncingRows_=false;
        sendValue(kProgram,double(program)/127.0);
    }
    return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
void P165View::dbSelectionChanged(CDataBrowser* table) {
    if(syncingRows_||!table)return;
    const int row=table->getSelectedRow();
    if(row<0||size_t(row)>=visiblePrograms_.size())return;
    sendValue(kProgram,double(visiblePrograms_[size_t(row)])/127.0);
}

} // namespace p007
