#!/usr/bin/env python3
"""P184 test-only patch of pinned Steinberg EditorHost source; NEVER shipping VST3."""
from pathlib import Path
import sys
if len(sys.argv) != 2:
    raise SystemExit("usage: P184PatchEditorHostLifecycle.py <sdk-root>")
path = Path(sys.argv[1]) / "public.sdk/samples/vst-hosting/editorhost/source/editorhost.cpp"
source = path.read_text(encoding="utf-8")
def replace_once(old, new):
    global source
    n=source.count(old)
    if n != 1:
        raise RuntimeError(f"P184 pinned SDK source changed, anchor count {n}: {old[:80]!r}")
    source=source.replace(old,new,1)
replace_once('#include <iostream>\n',
             '#include <iostream>\n#include <cstdlib>\n#include <cstring>\n#include <functional>\n')
replace_once('static AppInit gInit (std::make_unique<App> ());',
'''static AppInit gInit (std::make_unique<App> ());
// Test host only: genuine two-view detach/recreate on one controller.
static int gP184LiveViews = 0;
static bool gP184Reopened = false;
static std::function<void ()> gP184ReopenView;''')
anchor='\tif (flags & kSecondWindow)\n\t{\n\t\tSMTG_DBPRT0 ("Open 2cd Editor...\\n");\n\t\tcreateViewAndShow (editController);\n\t}\n'
replace_once(anchor,anchor+'''
    const char* p184 = std::getenv ("P184_EDITOR_REOPEN");
    if ((flags & kSecondWindow) && p184 && std::strcmp (p184, "1") == 0)
        gP184ReopenView = [this, editController] () { createViewAndShow (editController); };
''')
anchor_attach='\tif (plugView->attached (platformWindow.ptr, platformWindow.type) != kResultTrue)\n\t{\n\t\tIPlatform::instance ().kill (-1, "Attaching PlugView failed");\n\t}\n'
replace_once(anchor_attach,anchor_attach+'\t++gP184LiveViews;\n')
replace_once(
    '\t// TODO maybe quit only when the last window is closed\n\tIPlatform::instance ().quit ();',
    '''    // Stock EditorHost quits on either close. P184 retains the second
    // actual native view while opening a NEW IPlugView on that controller.
    if (gP184LiveViews > 0)
        --gP184LiveViews;
    if (gP184ReopenView && gP184LiveViews > 0)
    {
        if (!gP184Reopened)
        {
            gP184Reopened = true;
            std::puts ("P184_EDITOR_WINDOW_DETACHED_AND_REOPEN_REQUESTED");
            std::fflush (stdout);
            gP184ReopenView ();
        }
        return;
    }
    IPlatform::instance ().quit ();''')
path.write_text(source,encoding="utf-8")
print("P184_STEINBERG_EDITORHOST_TEST_ONLY_REOPEN_HOOK_READY",flush=True)
