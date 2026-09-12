// Exercises unchanged QLC+ engine/VCButton code and the candidate translator.
// Test seams are WinMM device transport, undo history, and output capture only.
#include <QGuiApplication>
#include <QXmlStreamReader>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QMutexLocker>
#include <QThread>
#include <QCryptographicHash>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "doc.h"
#include "fixture.h"
#include "scene.h"
#include "mastertimer.h"
#include "universe.h"
#include "vcbutton.h"
#include "soundswitchmidiinput.h"
#include "soundswitcheffects.h"

struct SoundSwitchMidiTestAccess
{
    static int activeRawLoop(const SoundSwitchMidiInput &input) { return input.m_activeRawLoop; }
    static void note(SoundSwitchMidiInput &input, int note, bool down)
    {
        input.handleShortMessage((down ? 0x90U : 0x80U) |
            (note << 8) | (down ? 127U << 16 : 0));
    }
};

namespace {
void require(bool condition, const QString &message)
{
    if (!condition) throw std::runtime_error(message.toStdString());
}

void settle(int milliseconds = 120)
{
    QElapsedTimer elapsed;
    elapsed.start();
    do {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::processEvents();
        QThread::msleep(2);
    } while (elapsed.elapsed() < milliseconds);
}

void eventually(const std::function<bool()> &predicate, const QString &message)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!predicate() && elapsed.elapsed() < 2500) settle(10);
    require(predicate(), message);
}

class CaptureIO final : public QLCIOPlugin
{
public:
    SoundSwitchMidiInput *translator = nullptr;
    std::vector<std::pair<quint32, uchar>> feedback;
    QMutex mutex;
    QHash<quint32, QByteArray> frames;
    void init() override {}
    QString name() const override { return "V34 native capture (no hardware)"; }
    int capabilities() const override { return Input | Output | Feedback; }
    QString pluginInfo() const override { return name(); }
    bool openOutput(quint32, quint32) override { return true; }
    bool openInput(quint32, quint32) override { return true; }
    void writeUniverse(quint32 universe, quint32, const QByteArray &data, bool) override
    {
        QMutexLocker lock(&mutex);
        frames[universe] = data;
    }
    void sendFeedBack(quint32 universe, quint32, quint32 channel, uchar value,
                      const QVariant &) override
    {
        if (universe != 1 || translator == nullptr) return;
        feedback.emplace_back(channel, value);
        // The data is emitted by actual native VCWidget::sendFeedback and
        // InputOutputMap::sendFeedBack, never fabricated by the test.
        translator->applyFeedback(channel, value);
    }
    QByteArray frame(quint32 universe)
    {
        QMutexLocker lock(&mutex);
        return frames.value(universe, QByteArray(512, char(0))).leftJustified(512, char(0));
    }
};

class Rig
{
public:
    // Destruction order keeps the output capture alive until Doc closes patches.
    CaptureIO capture;
    SoundSwitchMidiInput input;
    std::unique_ptr<Doc> doc;
    std::vector<std::unique_ptr<VCButton>> buttons;
    QHash<quint32, VCButton *> byId;
    QString workspace;

    Rig()
    {
        SoundSwitchMidiTestBackend::reset();
        capture.translator = &input;
        const QString repo = qEnvironmentVariable("V34_REPOSITORY");
        workspace = qEnvironmentVariable("V34_WORKSPACE", repo +
            "/qlcplus/workspace-tools/IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V34-RELIABILITY.qxw");
        doc = std::make_unique<Doc>(nullptr, 6);
        const QString fixtures = qEnvironmentVariable("V34_FIXTURES", repo +
            "/releases/qlcplus-control-one/v32-testing/Fixtures");
        require(doc->fixtureDefCache()->load(QDir(fixtures)), "Cannot load packaged fixture definitions");
        QFile file(workspace);
        require(file.open(QIODevice::ReadOnly), "Cannot open exact candidate workspace: " + workspace);
        QXmlStreamReader xml(&file);
        bool engineLoaded = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) continue;
            if (xml.name() == QLatin1String("Engine")) {
                // Never open the workspace's physical I/O routes in a test.
                require(doc->loadXML(xml, false), "Native Doc rejected Engine");
                engineLoaded = true;
            } else if (xml.name() == QLatin1String("Button")) {
                auto button = std::make_unique<VCButton>(doc.get());
                require(button->loadXML(xml), "Native VCButton rejected XML");
                byId[button->id()] = button.get();
                buttons.push_back(std::move(button));
            }
        }
        require(engineLoaded && !xml.hasError(), "Native workspace parse failed: " + xml.errorString());
        require(doc->fixture(0) && doc->fixture(410), "Full physical and private V34 patch did not load");
        for (Fixture *fixture : doc->fixtures())
            require(fixture->channels() > 0, "Fixture loaded without channels: " + fixture->name());
        for (int u = 0; u < 6; ++u)
            require(doc->inputOutputMap()->universe(u)->setOutputPatch(&capture, u), "Capture patch failed");
        require(doc->inputOutputMap()->universe(1)->setFeedbackPatch(&capture, 1), "Native feedback patch failed");
        QObject::connect(&input, &SoundSwitchMidiInput::valueChanged,
            [&] (quint32 channel, uchar value) { dispatch(channel, value); });
        // Startup selection is outside these focused contracts. No runtime or
        // fader methods are stubbed: use the real pinned master/universe threads.
        doc->setStartupFunction(Function::invalidId());
        doc->setMode(Doc::Operate);
        doc->inputOutputMap()->startUniverses();
        doc->masterTimer()->start();
        settle();
    }
    ~Rig()
    {
        for (Function *function : doc->functions())
            if (function->flashing()) function->unFlash(doc->masterTimer());
        settle(60);
        doc->masterTimer()->stop();
        capture.translator = nullptr;
        buttons.clear();
        doc.reset();
    }
    Scene *scene(quint32 id)
    {
        auto *value = qobject_cast<Scene *>(doc->function(id));
        require(value != nullptr, QString("Missing Scene %1").arg(id));
        return value;
    }
    VCButton *button(quint32 id)
    {
        require(byId.contains(id), QString("Missing native button %1").arg(id));
        return byId[id];
    }
    void dispatch(quint32 channel, uchar value)
    {
        // Focused equivalent of VirtualConsole's active-page input dispatch.
        // Native button parsing, pressure slot, state, Scene and feedback are used.
        for (const auto &button : buttons) {
            if (button->isDisabled()) continue;
            for (const auto &source : button->inputSources()) {
                if (source->universe() == 1 && source->channel() == channel) {
                    const bool invoked = QMetaObject::invokeMethod(button.get(), "slotInputValueChanged",
                        Qt::DirectConnection, Q_ARG(quint8, 0), Q_ARG(uchar, value));
                    require(invoked, "Native pressure slot unavailable");
                    break;
                }
            }
        }
    }
    void note(int number, bool down)
    {
        SoundSwitchMidiTestAccess::note(input, number, down);
        settle();
    }
    void tap(int number) { note(number, true); note(number, false); }
    void shiftedTap(int number) { note(52, true); tap(number); note(52, false); }
    void stopMouse() { button(1001)->requestStateChange(true); settle(200); }
    uchar value(int universe, int channel) { return uchar(capture.frame(universe).at(channel)); }
    QByteArray composed()
    {
        SoundSwitchEffects effects;
        effects.setFrame(capture.frame(3));
        effects.setColorLatchFrame(capture.frame(4));
        effects.setColorHoldFrame(capture.frame(5));
        return effects.compose(capture.frame(0));
    }
    bool anyFlash() const
    {
        for (Function *function : doc->functions()) if (function->flashing()) return true;
        return false;
    }
    int stopAcknowledgements() const
    {
        return std::count(capture.feedback.begin(), capture.feedback.end(),
            std::pair<quint32, uchar>{817, 255});
    }
};

void nativeStopAllNegativeControl()
{
    Rig rig;
    rig.note(45, true);
    require(rig.scene(1)->flashing(), "WHITE failed to flash in negative control");
    rig.doc->masterTimer()->stopAllFunctions();
    settle();
    require(rig.scene(1)->flashing(),
        "Negative control no longer reproduces pinned StopAll ignoring Flash");
    rig.note(45, false);
    require(!rig.scene(1)->flashing(), "Native unFlash did not release negative control");
}

void independentPerformanceOwnership()
{
    for (const auto &entry : {std::pair<int, int>{45, 1}, {46, 0}, {47, 2}}) {
        Rig rig;
        const int note = entry.first, scene = entry.second;
        rig.shiftedTap(note);
        require(rig.scene(4100 + scene)->flashing(), "Shift performance latch did not flash its clone");
        rig.note(note, true);
        require(rig.scene(scene)->flashing(), "Momentary performance did not flash independent Scene");
        require(rig.scene(4100 + scene)->flashing(), "Momentary press canceled latch");
        rig.note(note, false);
        require(!rig.scene(scene)->flashing(), "Momentary release stuck on");
        require(rig.scene(4100 + scene)->flashing(), "Momentary release canceled established latch");
        rig.shiftedTap(note);
        require(!rig.scene(4100 + scene)->flashing(), "Second Shift tap did not release latch");
    }
}

void mouseStopClearsNativeFlashAndMarkers()
{
    Rig rig;
    rig.tap(36);             // latch red
    rig.note(52, true);
    rig.note(41, true);      // hold blue
    rig.note(52, false);
    rig.shiftedTap(45);      // white latch
    rig.note(46, true);      // black hold
    require(rig.anyFlash(), "STOP setup created no native Flash");
    eventually([&] { return rig.value(3, 336) == 255; }, "BLACK marker did not reach real native U4");
    rig.stopMouse();
    require(std::find(rig.capture.feedback.begin(), rig.capture.feedback.end(),
        std::pair<quint32, uchar>{817, 255}) != rig.capture.feedback.end(),
        "Mouse STOP never emitted native feedback817 into translator");
    eventually([&] { return !rig.anyFlash(); }, "Mouse STOP left a native Scene flashing");
    if (rig.doc->masterTimer()->runningFunctions() != 0) {
        for (Function *function : rig.doc->functions())
            if (function->isRunning())
                std::cout << "STOP remaining " << function->id() << " " << function->name().toStdString() << std::endl;
    }
    eventually([&] { return rig.doc->masterTimer()->runningFunctions() == 0; }, "Mouse STOP left native Functions running");
    eventually([&] { return rig.value(3,334) == 0 && rig.value(3,335) == 0 &&
        rig.value(3,336) == 0 && rig.value(4,334) == 0 && rig.value(5,334) == 0; },
        "Mouse STOP left an HTP ownership marker active");
    rig.note(41, false);
    rig.note(46, false);
    require(!rig.anyFlash(), "Late physical NoteOff after STOP resurrected a Flash");
    rig.note(45, true);
    require(rig.scene(1)->flashing(), "Performance controls did not recover after STOP");
    rig.note(45, false);
}

void nativeColorLayersAndBlackPriority()
{
    Rig rig;
    rig.tap(36);
    eventually([&] { return rig.value(4, 334) == 255; }, "Color latch marker missing from native U5");
    require(rig.value(5, 334) == 0, "Color hold marker spuriously active");
    rig.note(52, true); rig.note(41, true); rig.note(52, false);
    eventually([&] { return rig.value(5, 334) == 255; }, "Held color marker missing from native U6");
    require(rig.value(4, 334) == 255, "Held color canceled native latch marker");
    rig.note(41, false);
    eventually([&] { return rig.value(5, 334) == 0; }, "Color hold marker stuck after release");
    require(rig.value(4, 334) == 255, "Color hold release canceled native latch");
    rig.shiftedTap(46);
    rig.note(45, true);        // later white must not defeat BLACK
    eventually([&] { return rig.value(3, 334) == 255 && rig.value(3, 336) == 255; },
        "WHITE and BLACK native HTP flags interfered");
    const QByteArray uncomposed = rig.capture.frame(0);
    require(uchar(uncomposed.at(89)) > 0 && uchar(uncomposed.at(107)) > 0,
        "Native WHITE did not light both main Focus dimmers before BLACK composition");
    for (int start : {0, 10, 20, 30})
        require(uchar(uncomposed.at(start)) > 0, "Native WHITE did not produce nonzero physical output");
    for (int start : {174, 214, 254, 294}) {
        bool nonzero = false;
        for (int i = 0; i < 40; ++i) nonzero |= uchar(uncomposed.at(start + i)) > 0;
        require(nonzero, "Native WHITE did not produce any nonzero tube emitter");
    }
    const QByteArray frame = rig.composed();
    for (int start : {0, 10, 20, 30})
        require(uchar(frame.at(start)) == 0, "BLACK failed IR4 dimmer after later WHITE");
    for (int start : {174, 214, 254, 294})
        for (int index = 0; index < 40; ++index)
            require(uchar(frame.at(start + index)) == 0, "BLACK failed tube pixel after later WHITE");
    for (int index = 44; index < 80; ++index)
        require(uchar(frame.at(index)) == 0, "BLACK failed Wash emitter after later WHITE");
    for (int channel : {89, 91, 107, 109})
        require(uchar(frame.at(channel)) == 0, "BLACK failed Focus dimmers after later WHITE");
    rig.note(45, false);
    rig.shiftedTap(46);
    eventually([&] { return rig.value(3, 336) == 0; }, "BLACK latch marker did not release");
}

void actualRawChaserFeedback()
{
    Rig rig;
    // Disabled widgets suppress feedback at runtime. The visible strips are
    // inert Labels; only the enabled offscreen observer owns raw-loop feedback.
    // Prove the negative case explicitly without inventing XML Frame behavior.
    rig.button(2100)->setDisabled(true);
    Function *raw = rig.doc->function(532);
    require(raw != nullptr, "Raw loop 532 missing");
    raw->start(rig.doc->masterTimer(), FunctionParent::master());
    settle(200);
    require(SoundSwitchMidiTestAccess::activeRawLoop(rig.input) == -1,
        "Disabled native observer unexpectedly emitted active raw-loop feedback");
    raw->stop(FunctionParent::master());
    settle(120);
    rig.button(2100)->setDisabled(false);
    raw->start(rig.doc->masterTimer(), FunctionParent::master());
    eventually([&] { return SoundSwitchMidiTestAccess::activeRawLoop(rig.input) == 0; },
        "Real Chaser running signal did not reach translator through enabled native feedback monitor");
    require(std::find(rig.capture.feedback.begin(), rig.capture.feedback.end(),
        std::pair<quint32, uchar>{1100, 255}) != rig.capture.feedback.end(),
        "Native raw-Chaser monitor did not emit feedback1100");
    raw->stop(FunctionParent::master());
    eventually([&] { return SoundSwitchMidiTestAccess::activeRawLoop(rig.input) == -1; },
        "Real Chaser stopped signal left stale current-loop state");
}

void immediateOwnerStartThenStop()
{
    Rig rig;
    Function *owner = rig.doc->function(660);
    require(owner != nullptr && owner->type() == Function::CollectionType, "Manual owner 660 missing");
    for (int attempt = 0; attempt < 8; ++attempt) {
        const int acknowledgements = rig.stopAcknowledgements();
        owner->start(rig.doc->masterTimer(), FunctionParent::master());
        // Deliberately no event-loop/timer settle between starting and STOP.
        // One iteration also reproduces a rapid double click before preRun.
        if (attempt == 0) rig.button(1001)->requestStateChange(true);
        rig.stopMouse();
        require(rig.stopAcknowledgements() == acknowledgements + 1,
            "Mouse STOP did not cross exactly one actual native preRun acknowledgement");
        if (rig.doc->masterTimer()->runningFunctions() != 0) {
            for (Function *function : rig.doc->functions())
                if (function->isRunning())
                    std::cout << "IMMEDIATE STOP remaining " << function->id() << " "
                        << function->name().toStdString() << std::endl;
        }
        require(rig.doc->masterTimer()->runningFunctions() == 0,
            "Immediate owner start/STOP left native queued playback running");
    }
}

void immediateOwnerStartThenHardwareStop()
{
    Rig rig;
    Function *owner = rig.doc->function(660);
    require(owner != nullptr, "Manual owner 660 missing");
    rig.note(52, true);
    for (int attempt = 0; attempt < 8; ++attempt) {
        const int acknowledgements = rig.stopAcknowledgements();
        owner->start(rig.doc->masterTimer(), FunctionParent::master());
        // Physical packet arrives before the next native timer tick.
        SoundSwitchMidiTestAccess::note(rig.input, 51, true);
        SoundSwitchMidiTestAccess::note(rig.input, 51, false);
        settle(250);
        require(rig.stopAcknowledgements() == acknowledgements + 1,
            "Held-Shift repeated STOP did not cross native preRun acknowledgement");
        require(rig.doc->masterTimer()->runningFunctions() == 0,
            "Immediate owner start/Shift+Play STOP left native queued playback running");
    }
    rig.note(52, false);
}
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("QLCPlusV34NativeContract");
    app.setApplicationName("NoHardwareTest");
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
        if (type == QtCriticalMsg || type == QtFatalMsg)
            std::cerr << message.toStdString() << '\n';
    });
    std::cout << "QLC+ source " << PINNED_QLC_SOURCE << "; Qt " << qVersion() << '\n';
    const std::vector<std::pair<const char *, std::function<void()>>> tests = {
        {"pinned StopAll/Flash negative control", nativeStopAllNegativeControl},
        {"independent WHITE/BLACK/UV hold and latch ownership", independentPerformanceOwnership},
        {"mouse STOP through actual native feedback and Flash release", mouseStopClearsNativeFlashAndMarkers},
        {"native U5/U6 color markers and BLACK priority", nativeColorLayersAndBlackPriority},
        {"actual raw Chaser observer start/stop feedback and disabled negative control", actualRawChaserFeedback},
        {"immediate native manual-owner start then mouse STOP", immediateOwnerStartThenStop},
        {"immediate native manual-owner start then Shift+Play STOP", immediateOwnerStartThenHardwareStop}
    };
    int failures = 0;
    for (const auto &test : tests) {
        try { test.second(); std::cout << "PASS " << test.first << std::endl; }
        catch (const std::exception &error) {
            ++failures;
            std::cerr << "FAIL " << test.first << ": " << error.what() << std::endl;
        }
    }
    return failures ? 1 : 0;
}
