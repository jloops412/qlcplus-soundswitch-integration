/* QLC+ SoundSwitch integration. Licensed under Apache-2.0. */
#include "../soundswitcheffects.h"
#include "../soundswitchintensity.h"
#include <QByteArray>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

void require(bool ok, const char *message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

unsigned byteAt(const QByteArray &frame, int channel)
{
    return static_cast<uchar>(frame[channel]);
}

struct EmitterGroup { int first; int count; };

std::vector<EmitterGroup> emitterGroups()
{
    std::vector<EmitterGroup> groups;
    for (int i = 0; i < 4; ++i) groups.push_back({i * 10 + 1, 6});
    for (int i = 0; i < 6; ++i) groups.push_back({44 + i * 6, 6});
    for (int i = 0; i < 32; ++i) groups.push_back({174 + i * 5, 5});
    return groups;
}

QByteArray redTemplate()
{
    QByteArray frame(512, '\0');
    frame[334] = char(255);
    for (const auto group : emitterGroups()) frame[group.first] = char(96);
    frame[84] = char(22);
    frame[102] = char(67);
    return frame;
}

QByteArray greenTemplate()
{
    QByteArray frame(512, '\0');
    frame[334] = char(255);
    for (const auto group : emitterGroups()) frame[group.first + 1] = char(96);
    frame[84] = char(90);
    frame[102] = char(124);
    return frame;
}

void requireBlack(const QByteArray &before, const QByteArray &after)
{
    require(before.size() == after.size(), "BLACK must not resize selected frame");
    for (int i = 0; i < before.size(); ++i)
    {
        bool intensity = false;
        for (auto span : SoundSwitchIntensity::ChannelSpans)
            if (std::size_t(i) >= span.first && std::size_t(i) < span.first + span.count)
                intensity = true;
        require(after[i] == (intensity ? char(0) : before[i]),
                "BLACK must close every intensity while preserving all other bytes");
    }
}

void colorAndPerformanceTests()
{
    SoundSwitchEffects fx;
    QByteArray base(512, char(73));
    const auto groups = emitterGroups();
    std::vector<bool> colorChannels(512, false);
    int groupNumber = 0;
    for (const auto group : groups)
    {
        for (int c = 0; c < group.count; ++c)
        {
            // Include completely dark groups, peaks on Amber/UV and arbitrary
            // nonuniform pixel intensities in an otherwise lit full rig.
            base[group.first + c] = char(groupNumber % 5 == 0 ? 0 :
                (groupNumber * 37 + c * 23) % 256);
            colorChannels[group.first + c] = true;
        }
        ++groupNumber;
    }
    colorChannels[84] = colorChannels[102] = true;
    QByteArray red = redTemplate();
    fx.setColorLatchFrame(red);
    const QByteArray redOut = fx.compose(base);
    for (const auto group : groups)
    {
        unsigned peak = 0;
        for (int c = 0; c < group.count; ++c)
            peak = std::max(peak, byteAt(base, group.first + c));
        require(byteAt(redOut, group.first) == peak,
                "RED must retain each selected IR4/zone/cell peak including dark groups");
        for (int c = 1; c < group.count; ++c)
            require(redOut[group.first + c] == 0,
                    "RED must replace all six IR4 emitters, including Amber/UV");
    }
    for (int i = 0; i < 512; ++i)
        if (!colorChannels[i])
            require(redOut[i] == base[i], "color must leave masters, optics, movement and unpatched channels unchanged");
    require(redOut[84] == char(22) && redOut[102] == char(67),
            "color must preserve distinct unscaled Focus A/B mechanical wheel slots");

    QByteArray green = greenTemplate();
    fx.setColorHoldFrame(green);
    const QByteArray greenOut = fx.compose(base);
    require(greenOut != redOut && greenOut[84] == char(90) && greenOut[102] == char(124),
            "held color owns LEDs and both Focus wheels");
    fx.setColorLatchFrame(red);
    require(fx.compose(base) == greenOut, "hold precedence must not depend on latch update order");
    green[334] = 0;
    fx.setColorHoldFrame(green);
    require(fx.compose(base) == redOut, "native hold release reveals underlying latch despite retained payload");
    green[334] = char(255);
    fx.setColorHoldFrame(green);
    fx.clearColorLatchFrame();
    require(fx.compose(base) == greenOut, "latch removal must not release held color");
    fx.setColorLatchFrame(red);
    fx.clearColorHoldFrame();
    require(fx.compose(base) == redOut, "hold route removal must restore latch");
    fx.clearColorLatchFrame();
    require(fx.compose(base) == base, "both color routes removed must release ownership");

    // Full Color has different hues per fixture, zone and cell. It must not
    // collapse into one palette per fixture class.
    QByteArray fullColor(512, '\0');
    fullColor[334] = char(255);
    fullColor[84] = char(124);
    fullColor[102] = char(22);
    groupNumber = 0;
    for (const auto group : groups)
    {
        fullColor[group.first + groupNumber % group.count] = char(200);
        fullColor[group.first + (groupNumber + 1) % group.count] = char(100);
        ++groupNumber;
    }
    fx.setColorLatchFrame(fullColor);
    const QByteArray fullOut = fx.compose(base);
    groupNumber = 0;
    for (const auto group : groups)
    {
        unsigned peak = 0;
        for (int c = 0; c < group.count; ++c)
            peak = std::max(peak, byteAt(base, group.first + c));
        require(byteAt(fullOut, group.first + groupNumber % group.count) == peak,
                "FULL COLOR must retain the individual emitter-group hue and peak");
        require(byteAt(fullOut, group.first + (groupNumber + 1) % group.count) == (peak + 1) / 2,
                "FULL COLOR must preserve the individual mixed-hue ratio with bounded rounding");
        ++groupNumber;
    }
    for (int gain : {1, 32, 127, 192, 255})
    {
        QByteArray scaled = fullColor;
        scaled[334] = char(gain);
        for (const auto group : groups)
            for (int c = 0; c < group.count; ++c)
                scaled[group.first + c] = char((byteAt(fullColor, group.first + c) * gain + 127) / 255);
        fx.setColorLatchFrame(scaled);
        const auto out = fx.compose(base);
        for (const auto group : groups)
        {
            unsigned beforePeak = 0, afterPeak = 0;
            for (int c = 0; c < group.count; ++c)
            {
                beforePeak = std::max(beforePeak, byteAt(base, group.first + c));
                afterPeak = std::max(afterPeak, byteAt(out, group.first + c));
            }
            require(beforePeak == afterPeak, "native GM-scaled templates must not dim selected peaks twice");
        }
        require(out[84] == char(124) && out[102] == char(22),
                "native GM must never numerically scale Focus wheel slots");
    }
    fullColor[334] = 0;
    fx.setColorLatchFrame(fullColor);
    require(fx.compose(base) == base, "inactive marker must ignore retained color payload");
    fullColor[334] = char(255);
    for (const auto group : groups)
        for (int c = 0; c < group.count; ++c) fullColor[group.first + c] = 0;
    fx.setColorLatchFrame(fullColor);
    const auto zeroPaletteOut = fx.compose(base);
    for (const auto group : groups)
        require(zeroPaletteOut.mid(group.first, group.count) == base.mid(group.first, group.count),
                "zero hue templates must preserve selected emitters without dividing by zero");

    fx.setColorLatchFrame(red);
    QByteArray effects(512, '\0');
    for (int performanceMarker : {334, 335})
    {
        for (int gain : {1, 127, 255})
        {
            effects[performanceMarker] = char(gain);
            fx.setFrame(effects);
            require(fx.compose(base) == base,
                    "native WHITE/UV must retain complete QLC-merged colors above any color latch");
            green[334] = char(255);
            fx.setColorHoldFrame(green);
            require(fx.compose(base) == base, "later held color cannot recolor active WHITE/UV");
            fx.clearColorHoldFrame();
        }
        effects[performanceMarker] = 0;
    }
    fx.setFrame(effects);
    require(fx.compose(base) == redOut, "WHITE/UV release restores color latch at current show brightness");
    effects[0] = char(255);
    for (int b : {80, 98}) for (int c : {0, 1, 2, 3, 16}) effects[b + c] = char(150 + c);
    fx.setFrame(effects);
    const auto moveColor = fx.compose(base);
    require(moveColor[80] == char(150) && moveColor[98] == char(150) &&
            moveColor[84] == char(22) && moveColor[102] == char(67),
            "MOVE and color must own disjoint Focus parameters");
    effects[1] = char(128);
    effects[2] = 0;
    fx.setFrame(effects);
    requireBlack(moveColor, fx.compose(base));
    effects[1] = effects[2] = char(128);
    fx.setFrame(effects);
    require(fx.compose(base) == moveColor, "strobe open phase must restore exact color and movement");
    for (int gain : {1, 127, 255})
    {
        effects[336] = char(gain);
        fx.setFrame(effects);
        requireBlack(moveColor, fx.compose(base));
    }
    effects[334] = char(255); // WHITE is now pressed after BLACK.
    fx.setFrame(effects);
    fx.setColorHoldFrame(greenTemplate());
    QByteArray whiteWithMove = base;
    for (int b : {80, 98}) for (int c : {0, 1, 2, 3, 16}) whiteWithMove[b + c] = effects[b + c];
    requireBlack(whiteWithMove, fx.compose(base));
    effects[336] = 0;
    fx.setFrame(effects);
    require(fx.compose(base) == whiteWithMove, "BLACK release restores the current WHITE owner and movement");
    effects[334] = 0;
    fx.setFrame(effects);
    const auto heldWithMove = fx.compose(base);
    require(heldWithMove[84] == char(90) && heldWithMove[102] == char(124),
            "WHITE release after BLACK must restore current held color");
    fx.clearFrame();
    require(fx.compose(base) == greenOut, "U4 route removal releases only MOVE/STROBE/performance ownership");

    // Intensity remains the last production plug-in stage after the composer.
    QByteArray scaled = fx.compose(base);
    SoundSwitchIntensity::Levels levels{{128,64,96,160,192,255}};
    SoundSwitchIntensity::scaleFrame(reinterpret_cast<std::uint8_t *>(scaled.data()), scaled.size(), levels);
    for (auto span : SoundSwitchIntensity::ChannelSpans)
        for (std::size_t i = span.first; i < span.first + span.count; ++i)
        {
            const int gain = (128 * levels[span.group] + 127) / 255;
            require(byteAt(scaled, int(i)) == (byteAt(greenOut, int(i)) * gain + 127) / 255,
                    "Global and each group must scale the composed intensity exactly once");
        }
    require(scaled[84] == greenOut[84] && scaled[102] == greenOut[102],
            "Global/group scaling must preserve composed Focus wheel values");

    // Snapshot ownership and truncated/recovered routes must not retain masks.
    char raw[512] = {};
    std::copy(red.constData(), red.constData() + 512, raw);
    fx.clearColorHoldFrame();
    fx.setColorLatchFrame(QByteArray::fromRawData(raw, 512));
    raw[334] = 0; raw[84] = char(200);
    require(fx.compose(base) == redOut, "latch snapshot must own producer bytes");
    const QByteArray greenSnapshot = greenTemplate();
    std::copy(greenSnapshot.constData(), greenSnapshot.constData() + 512, raw);
    fx.setColorHoldFrame(QByteArray::fromRawData(raw, 512));
    raw[334] = 0; raw[102] = 0;
    require(fx.compose(base) == greenOut, "hold snapshot must own producer bytes");
    for (int size : {0, 1, 116, 334, 513})
    {
        fx.setColorHoldFrame(greenTemplate());
        fx.setColorHoldFrame(QByteArray(size, char(255)));
        require(fx.compose(base) == redOut, "invalid hold frame must release only hold, revealing latch");
        fx.setColorLatchFrame(red);
        fx.setColorLatchFrame(QByteArray(size, char(255)));
        require(fx.compose(base) == base, "invalid latch frame must clear stale color ownership");
        fx.setColorLatchFrame(red);
    }
    fx.setColorLatchFrame(red.left(335));
    require(fx.compose(base) == redOut, "complete 335-byte color frame must be accepted");
    require(fx.compose(QByteArray()).isEmpty(), "colors must not grow an empty physical frame");
    for (int size : {1, 5, 6, 45, 178, 332, 334, 511})
    {
        const auto shortened = base.left(size);
        const auto out = fx.compose(shortened);
        require(out.size() == shortened.size(), "color on short physical frames must never grow output");
        for (const auto group : groups)
            if (group.first < size && group.first + group.count > size)
                require(out.mid(group.first) == shortened.mid(group.first),
                        "incomplete physical emitter group must remain untouched");
    }
    effects = QByteArray(512, '\0'); effects[336] = char(255);
    fx.setFrame(effects);
    requireBlack(redOut, fx.compose(base));
    effects[334] = char(255);
    fx.setFrame(effects.left(336));
    require(fx.compose(base) == redOut, "truncated performance frame must not retain partial WHITE/BLACK ownership");
    fx.setFrame(effects.left(116));
    require(fx.compose(base) == redOut, "legacy 116-byte U4 must retain independent color ownership");
    fx.clearColorLatchFrame();
    fx.setFrame(QByteArray(512, '\0'));
    require(fx.compose(base) == base, "recovered clean routes must restore exact underlying output");
}

int main()
{
    SoundSwitchEffects fx;
    QByteArray base(512, '\0');
    for (int i=0;i<512;++i) base[i]=static_cast<char>((i*17+9)%256);
    require(fx.compose(base)==base,"no effect frame must preserve selected output");
    QByteArray frame(512, '\0');
    frame[0]=char(255);
    for (int b:{80,98}) for(int c:{0,1,2,3,16}) frame[b+c]=char(120+c);
    fx.setFrame(frame);
    auto out=fx.compose(base);
    for (int i=0;i<512;++i)
    {
        bool movement=false;
        for(int b:{80,98}) for(int c:{0,1,2,3,16}) if(i==b+c) movement=true;
        require(out[i]==(movement?frame[i]:base[i]),"MOVE must take only pan/tilt/speed");
    }
    // A position Flash is already merged by QLC+ into this one frame.
    frame[80]=char(33); frame[98]=char(77); fx.setFrame(frame);
    out=fx.compose(base); require(out[80]==char(33)&&out[98]==char(77),"native position values take precedence");
    frame[0]=0; frame[1]=char(255); frame[2]=0; fx.setFrame(frame);
    out=fx.compose(base);
    for(int i=0;i<512;++i)
    {
        bool intensity=false;
        for(auto s:SoundSwitchIntensity::ChannelSpans) if(std::size_t(i)>=s.first&&std::size_t(i)<s.first+s.count) intensity=true;
        require(out[i]==(intensity?char(0):base[i]),"closed strobe gate must suppress only intensity");
    }
    frame[2]=char(255); fx.setFrame(frame);
    require(fx.compose(base)==base,"open gate restores exact advancing underlying frame");
    for(int gain:{1,32,127,192,255})
    {
        frame[1]=char(gain); frame[2]=char(gain); fx.setFrame(frame);
        require(fx.compose(base)==base,"native grand master must not be multiplied twice");
    }
    frame[1]=0; frame[2]=char(255); fx.setFrame(frame);
    require(fx.compose(base)==base,"inactive strobe ignores retained gate data");
    frame[0]=char(255); frame[1]=char(255); frame[2]=0; fx.setFrame(frame);
    out=fx.compose(base); require(out[80]==frame[80]&&out[0]==0&&out[174]==0,"MOVE and strobe compose together");
    // Same mask applies after Priority frame selection and before group scaling.
    QByteArray priority(512,char(200)); out=fx.compose(priority);
    require(out[4]==char(200)&&out[0]==0&&out[80]==frame[80],"active Priority remains colour/optics authority");
    char raw[512]={}; raw[1]=char(255); raw[2]=0;
    fx.setFrame(QByteArray::fromRawData(raw,512)); raw[2]=char(255);
    require(fx.compose(base)[0]==0,"effect snapshot must own mutable producer bytes");
    fx.clearFrame(); require(fx.compose(base)==base,"route removal releases all parameter ownership");
    fx.setFrame(frame); fx.setFrame(QByteArray(115,char(255)));
    require(fx.compose(base)==base,"malformed short frame must clear stale effect state");
    fx.setFrame(frame); fx.setFrame(QByteArray(513,char(255)));
    require(fx.compose(base)==base,"oversized frame must clear stale effect state");
    fx.setFrame(frame);
    require(fx.compose(QByteArray()).isEmpty(),"empty base must stay empty");
    require(fx.compose(QByteArray(12,char(0))).size()==12,"short base must not grow");
    require(fx.compose(QByteArray(512,char(0)))==QByteArray(512,char(0)).replace(80,4,frame.mid(80,4)).replace(96,1,frame.mid(96,1)).replace(98,4,frame.mid(98,4)).replace(114,1,frame.mid(114,1)),"strobe never opens a dark fixture");
    colorAndPerformanceTests();
    std::cout << "PASS: MOVE/STROBE, per-emitter color, hold/latch precedence, WHITE/UV/BLACK, native GM, intensity groups, snapshot and recovery\n";
}
