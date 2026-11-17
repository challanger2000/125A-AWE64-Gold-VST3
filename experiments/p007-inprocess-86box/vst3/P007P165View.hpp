#pragma once
// P007-S: P165's REAL existing photographic hardware design and original
// VSTGUI hardware controls, attached to ORIGINAL Win95 Creative program/bank
// and host master. No sampler keyboard, no historic proprietary AWE DSP fork.
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/cdatabrowser.h"
#include "vstgui/lib/idatabrowserdelegate.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <functional>
#include <string>
#include <vector>
namespace p007 {
class P165View final : public VSTGUI::CViewContainer,
                       public VSTGUI::DataBrowserDelegateAdapter,
                       public VSTGUI::IControlListener {
public:
    P165View(const VSTGUI::CRect&,Steinberg::Vst::EditController*,
             std::function<int()> originalGuestStatus);
    void onIdle() override;
    void valueChanged(VSTGUI::CControl*) override;
    void controlBeginEdit(VSTGUI::CControl*) override;
    void controlEndEdit(VSTGUI::CControl*) override;
    int32_t dbGetNumRows(VSTGUI::CDataBrowser*) override;
    int32_t dbGetNumColumns(VSTGUI::CDataBrowser*) override;
    VSTGUI::CCoord dbGetRowHeight(VSTGUI::CDataBrowser*) override;
    VSTGUI::CCoord dbGetCurrentColumnWidth(int32_t,VSTGUI::CDataBrowser*) override;
    void dbDrawCell(VSTGUI::CDrawContext*,const VSTGUI::CRect&,
                    int32_t,int32_t,int32_t,VSTGUI::CDataBrowser*) override;
    VSTGUI::CMouseEventResult dbOnMouseDown(const VSTGUI::CPoint&,
                    const VSTGUI::CButtonState&,int32_t,int32_t,VSTGUI::CDataBrowser*) override;
    void dbSelectionChanged(VSTGUI::CDataBrowser*) override;
protected:
    ~P165View() noexcept override =default;
private:
    enum : int {kPrevious=3101,kNext=3102,kBank=1002,kProgram=1000,
                kMaster=1001,kSynthPage=3111,kBrowserPage=3103,kSearch=3104};
    Steinberg::Vst::EditController* controller_ = nullptr;
    std::function<int()> guestStatus_;
    VSTGUI::COptionMenu* bank_ = nullptr;
    VSTGUI::COptionMenu* program_ = nullptr;
    VSTGUI::CControl* master_ = nullptr;
    VSTGUI::CTextButton *synthButton_=nullptr,*browserButton_=nullptr;
    VSTGUI::CViewContainer* browserPanel_=nullptr;
    VSTGUI::CDataBrowser* browserList_=nullptr;
    VSTGUI::CTextEdit* browserSearch_=nullptr;
    VSTGUI::CTextLabel* browserHelp_=nullptr;
    std::vector<int> visiblePrograms_;
    int page_=0;bool syncingRows_=false;
    VSTGUI::CTextLabel* status_ = nullptr;
    VSTGUI::CTextLabel* programText_ = nullptr;
    VSTGUI::CTextLabel* masterText_ = nullptr;
    int lastStatus_=-999, lastProgram_=-1,lastBank_=-1,lastMaster_=-1;
    bool masterEditing_ = false;
    VSTGUI::CTextLabel* makeLabel(VSTGUI::CRect,const char*,bool);
    void sendValue(Steinberg::Vst::ParamID,double,bool gesture=false);
    void setPage(int page);
    void rebuildPrograms();
    void syncBrowserSelection(int program);
};
} // namespace p007
