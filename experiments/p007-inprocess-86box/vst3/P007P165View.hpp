#pragma once
// P007-S: P165's REAL existing photographic hardware design and original
// VSTGUI hardware controls, attached to ORIGINAL Win95 Creative program/bank
// and host master. No sampler keyboard, no historic proprietary AWE DSP fork.
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <functional>
namespace p007 {
class P165View final : public VSTGUI::CViewContainer,
                       public VSTGUI::IControlListener {
public:
    P165View(const VSTGUI::CRect&,Steinberg::Vst::EditController*,
             std::function<int()> originalGuestStatus);
    void onIdle() override;
    void valueChanged(VSTGUI::CControl*) override;
    void controlBeginEdit(VSTGUI::CControl*) override;
    void controlEndEdit(VSTGUI::CControl*) override;
protected:
    ~P165View() noexcept override =default;
private:
    enum : int {kPrevious=3101,kNext=3102,kBank=1002,kProgram=1000,kMaster=1001};
    Steinberg::Vst::EditController* controller_ = nullptr;
    std::function<int()> guestStatus_;
    VSTGUI::COptionMenu* bank_ = nullptr;
    VSTGUI::COptionMenu* program_ = nullptr;
    VSTGUI::CControl* master_ = nullptr;
    VSTGUI::CTextLabel* status_ = nullptr;
    VSTGUI::CTextLabel* programText_ = nullptr;
    VSTGUI::CTextLabel* masterText_ = nullptr;
    int lastStatus_=-999, lastProgram_=-1,lastBank_=-1,lastMaster_=-1;
    bool masterEditing_ = false;
    VSTGUI::CTextLabel* makeLabel(VSTGUI::CRect,const char*,bool);
    void sendValue(Steinberg::Vst::ParamID,double,bool gesture=false);
};
} // namespace p007
