# VirtualDJ OS2L Auto-Reconnect

V21 keeps OS2L inside stock QLC+ and VirtualDJ. There is no bridge or second lighting program.

QLC+ listens on `127.0.0.1:9996`. VirtualDJ must use the same direct target:

```text
os2l = auto
os2lDirectIp = 127.0.0.1:9996
```

VirtualDJ intentionally starts a direct OS2L connection after an OS2L command is sent. To start it at VirtualDJ launch and retry harmlessly every five seconds, add this mapper entry to the active keyboard mapper:

```xml
<map value="ONINIT" action="os2l_button &apos;QLC KEEPALIVE&apos; &amp; repeat_start &apos;qlc_os2l_keepalive&apos; 5000ms &amp; os2l_button &apos;QLC KEEPALIVE&apos;" name="QLC+ OS2L AUTO-RECONNECT" />
```

`QLC KEEPALIVE` is deliberately not mapped to a lighting Function. The periodic message only causes VirtualDJ to establish or re-establish the local TCP connection. Beat and BPM traffic then follows normally.

References:

- [VirtualDJ option list](https://virtualdj.com/manuals/virtualdj/appendix/optionslist.html)
- [VirtualDJ technical explanation of direct OS2L startup](https://virtualdj.com/forums/260254/VirtualDJ_Technical_Support/Bug_report%3A_Slow_startup_time_when_using_os2lDirectIP.html)
