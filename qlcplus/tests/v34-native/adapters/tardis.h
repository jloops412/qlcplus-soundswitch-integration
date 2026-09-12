// Test-only boundary for QLC+'s undo/network action history. The engine and
// VCButton/VCWidget implementation are unchanged upstream source. No live
// state, input, feedback, Scene, fader or output behavior is replaced here.
#pragma once
#include <QVariant>
class Tardis
{
public:
    enum ActionCodes {
        VCButtonSetFunctionID, VCButtonSetPressed, VCButtonSetActionType,
        VCButtonEnableStartupIntensity, VCButtonSetStartupIntensity,
        VCWidgetGeometry, VCWidgetZIndex, VCWidgetCaption,
        VCWidgetBackgroundColor, VCWidgetBackgroundImage,
        VCWidgetForegroundColor, VCWidgetFont
    };
    static Tardis *instance() { static Tardis history; return &history; }
    void enqueueAction(int, quint32, const QVariant &, const QVariant &) {}
};
