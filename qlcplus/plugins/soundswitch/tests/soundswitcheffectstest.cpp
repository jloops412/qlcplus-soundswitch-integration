/* QLC+ SoundSwitch integration. Licensed under Apache-2.0. */
#include "../soundswitcheffects.h"
#include "../soundswitchintensity.h"
#include <QByteArray>
#include <cstdlib>
#include <iostream>

void require(bool ok, const char *message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
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
    std::cout << "PASS: MOVE/STROBE ownership, intensity masks, native GM, Priority, snapshot and release\n";
}
