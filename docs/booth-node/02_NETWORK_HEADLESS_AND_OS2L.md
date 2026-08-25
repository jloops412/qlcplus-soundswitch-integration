# LLE Booth Node — Network, Headless Control, and OS2L

## Design objective

The booth LAN is a permanent private production network. The DJ laptop and booth node must see the same addresses and services at home, in a ballroom, with LTE, with venue Internet, and with no Internet at all.

```text
                         OPTIONAL UPSTREAMS
               venue Ethernet / venue Wi-Fi / LTE
                               |
                     ReadyNet WAN/repeater/LTE
                               |
                    PRIVATE LLE SHOW LAN
                               |
          +--------------------+--------------------+
          |                                         |
  DJ laptop, wired                            booth node, wired
  VirtualDJ                                   QLC+
  browser control      OS2L TCP 9996          Control One USB
          +-----------------------------------> SoundSwitch DMX
          |
          +---------- QLC+ web TCP 9999 ------>
```

Internet failover is useful, but it is not in the lighting control path.

## ReadyNet model and firmware gate

The owner has multiple ReadyNet devices and they may not all be the same variant. Before changing firmware or copying a configuration:

1. read the exact model from the bottom label;
2. confirm the model and hardware/firmware revision in the administration UI;
3. photograph the label for private inventory, excluding the image from public Git history;
4. export the current configuration if the firmware supports it;
5. use only firmware published for that exact model/revision;
6. never flash an LTE520S image merely because the unit looks like an LTE520S;
7. never upgrade router firmware immediately before an event.

The official LTE520S support page currently lists firmware that specifically addresses WAN/LTE failover for LAN devices. That does not prove an unidentified LTE520 has the same image or behavior.

## Routed LAN requirement

The production show LAN must remain behind ReadyNet NAT/DHCP. **Do not use bridge mode for the production topology.**

ReadyNet documentation uses “Repeater” and “Bridge” for several different configurations. Some documented repeater/bridge arrangements cause downstream clients to obtain addresses from the primary upstream access point. That behavior is unacceptable for show control because:

- the booth-node address can change;
- the venue can become the DHCP authority;
- venue client isolation can block DJ-to-booth traffic;
- a venue subnet can conflict with the ReadyNet management address;
- losing the upstream network can break local addressing.

### Repeater-mode acceptance test

A ReadyNet Wi-Fi upstream mode is acceptable only when all of the following remain true:

1. the ReadyNet LAN address remains unchanged;
2. the ReadyNet remains DHCP authority for the private show LAN;
3. the DJ laptop retains its reservation;
4. the booth node retains its reservation;
5. DJ-to-booth ping and TCP ports continue working when the upstream Wi-Fi is disconnected;
6. connecting to a venue whose LAN uses the same private range does not merge the networks;
7. the ReadyNet administration page remains reachable from the show LAN;
8. LTE failover does not renumber local devices.

A simple way to detect the wrong mode is to inspect `ipconfig /all` on the DJ laptop and booth node before and after connecting the ReadyNet to venue Wi-Fi. If the IPv4 address, DHCP server, or default gateway changes from the LLE router to a venue-owned address, stop: the mode is bridged and is not approved for the show LAN.

Until routed Wi-Fi upstream behavior is proven, use this priority:

1. venue Ethernet into the ReadyNet WAN port;
2. LTE;
3. no Internet while preserving the local show LAN.

Wi-Fi-as-WAN can be added only after the exact unit proves the routed acceptance test.

## Recommended addressing

For a newly dedicated booth router, use a subnet unlikely to collide with common venue defaults:

| Device/service | Address |
|---|---:|
| ReadyNet gateway | `10.52.0.1` |
| DJ laptop reservation | `10.52.0.10` |
| backup DJ laptop reservation | `10.52.0.11` |
| booth node reservation | `10.52.0.20` |
| future dedicated audio node | `10.52.0.30` |
| future services node | `10.52.0.40` |
| managed/control device range | `10.52.0.50–99` |
| ordinary DHCP pool | `10.52.0.100–199` |
| subnet mask | `255.255.255.0` |

An existing stable private subnet may be retained. Replace the example addresses in commands and settings consistently.

Prefer **DHCP reservations** over manually hard-coding Windows NIC addresses. Reservations keep normal automatic networking while giving VirtualDJ and bookmarks a stable target.

Record the reservation evidence privately:

- device name;
- Ethernet MAC address;
- reserved IP;
- ReadyNet configuration screenshot/export;
- date tested.

Do not place public MAC addresses or router credentials in Git.

## Physical wiring

For production:

- DJ laptop Ethernet -> ReadyNet LAN;
- booth node Ethernet -> ReadyNet LAN;
- Control One USB -> booth node;
- SoundSwitch Micro USB -> booth node when used;
- DMX/wireless DMX -> selected booth-node output;
- venue Ethernet -> ReadyNet WAN when available.

Use short, known-good Ethernet cables with working latch tabs. Label both ends `DJ` and `BOOTH`. Disable Windows power saving for both computers' Ethernet adapters after identifying the correct adapter:

1. Device Manager -> Network adapters -> the wired adapter -> Properties;
2. Power Management;
3. clear **Allow the computer to turn off this device to save power**;
4. on Advanced, disable Energy Efficient Ethernet/Green Ethernet only if the adapter exposes it and bench testing shows sleep/reconnect behavior.

Do not broadly disable adapter features without recording the prior value.

## Required traffic

| Source | Destination | Protocol/port | Purpose | Production rule |
|---|---|---|---|---|
| DJ laptop | booth node | TCP `9996` | VirtualDJ OS2L -> QLC+ | Required; private LAN only |
| DJ laptop/browser | booth node | TCP `9999` | QLC+ web interface | Required; authenticated; private LAN only |
| DJ laptop | booth node | ICMP | reachability/latency checks | Recommended |
| DJ laptop | booth node | TCP `445` | completed recording copy | Later recording milestone; restrict to two hosts |
| admin device | booth node | remote desktop/admin port | full Windows recovery | Deferred until OS/edition is known |

The prepared firewall helper creates only the QLC+ 9996/9999 rules, restricted to `LocalSubnet` and the Windows Private profile. SMB or remote administration must be deliberately added later rather than silently opened.

## Windows network profile

On both computers, ensure the ReadyNet Ethernet connection is **Private**, not Public:

```powershell
Get-NetConnectionProfile
```

To change the correct interface from an elevated PowerShell prompt:

```powershell
Set-NetConnectionProfile -InterfaceAlias 'Ethernet' -NetworkCategory Private
```

Use the actual interface alias from `Get-NetConnectionProfile`.

Never mark arbitrary venue Wi-Fi networks Private on the DJ laptop. The private designation is for the wired ReadyNet show LAN.

## QLC+ OS2L listener

QLC+ supports OS2L from VirtualDJ on the same computer or another computer. For this deployment:

1. open the V24 workspace on the booth node;
2. open QLC+ Input/Output;
3. enable the OS2L input plug-in on the intended universe;
4. open OS2L configuration;
5. set listener port `9996`;
6. save the workspace only if the I/O mapping must persist and the change is intentional;
7. verify the joystick/input indicator reacts when VirtualDJ sends data.

On the booth node:

```powershell
Get-NetTCPConnection -State Listen -LocalPort 9996
```

If no listener appears, fix QLC+ input configuration before touching VirtualDJ or the router.

## VirtualDJ direct target

On the DJ laptop, open VirtualDJ Settings -> Options and set:

```text
os2l = auto
os2lDirectIp = 10.52.0.20:9996
os2lBeatOffset = 0
```

Restart VirtualDJ after changing the direct target.

Keep the existing startup/reconnect mapper in the active keyboard mapping:

```xml
<map value="ONINIT" action="os2l_button &apos;QLC KEEPALIVE&apos; &amp; repeat_start &apos;qlc_os2l_keepalive&apos; 5000ms &amp; os2l_button &apos;QLC KEEPALIVE&apos;" name="QLC+ OS2L AUTO-RECONNECT" />
```

The `QLC KEEPALIVE` button name must remain unmapped in the QLC+ project. It is transport maintenance, not a lighting cue.

## Latency and jitter qualification

Moving OS2L from localhost to a wired LAN adds a network hop, but raw delay is not the only concern. The acceptance test focuses on packet loss, jitter, reconnect behavior, and repeatable visual timing.

From the DJ laptop while VirtualDJ and QLC+ are active:

```powershell
ping 10.52.0.20 -t
```

Bench requirements:

- zero packet loss during the test;
- normally steady single-digit-millisecond round trips;
- no repeated spikes above 20 ms;
- no disconnect when the upstream Internet is removed;
- no visible QLC+ UI starvation or DMX hitch during normal DJ activity.

These are deployment gates, not claims about what a human can perceive. A wired local path will normally be much faster, but the actual computer, adapter, router, driver, and USB behavior must be measured.

### If lighting looks consistently early or late

1. confirm the ping is stable;
2. compare a known track with QLC+ on the DJ laptop versus the booth node;
3. verify the QLC+ chase timing and selected beat source;
4. verify no second OS2L target/application is running;
5. verify the fixture/DMX path is not adding the apparent delay;
6. only then adjust `os2lBeatOffset` in small measured steps.

Do not use beat offset to hide random network jitter, dropped packets, or an overloaded booth node.

## Headless QLC+ web interface

QLC+ includes its own web server and browser-based Virtual Console. Start the pinned executable with:

```powershell
& 'C:\LLE\QLC\5.3.0-GIT-a124abe\qlcplus5.exe' `
  -3 `
  -w `
  -wp 9999 `
  -wa `
  -a 'C:\ProgramData\LLEBooth\qlc-web-auth' `
  -o 'C:\LLE\Projects\IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw'
```

Arguments:

- `-3`: disable the unused 3D preview context;
- `-w`: enable web access;
- `-wp 9999`: select the web port;
- `-wa`: enable users/authentication;
- `-a`: store credentials in the designated local file;
- `-o`: open the exact V24 workspace.

Kiosk mode (`-k`) is intentionally omitted from the first deployment. Add it only after manual and web control are proven, because kiosk mode can make local troubleshooting less convenient.

From the DJ laptop:

```text
http://10.52.0.20:9999
```

Bookmark it as **LLE Lighting Console**. The browser page mirrors the V24 Runtime Feedback Virtual Console and remains the normal show-time display/control surface.

Create two QLC+ web users:

- an administrator used only for configuration;
- an operator restricted to the Virtual Console for normal events.

QLC+ uses basic HTTP authentication and does not provide HTTPS certificates. Therefore:

- keep it on the isolated wired show LAN;
- never expose port 9999 through WAN/LTE port forwarding;
- never share the operator password with guests;
- never use the show SSID as an open guest network;
- do not send credentials through a venue-owned bridged network.

## What browser control does and does not replace

The browser is sufficient for normal QLC+ operation:

- V24 banks, Autoloops/Priority modes, latched starts, live-loop indicators, pads, color overrides, intensity, transport, independent dwell, and chase multiplier;
- QLC+ I/O/configuration checks when logged in as administrator;
- live state feedback.

It is not the whole Windows desktop. On Windows, do not assume the QLC+ web page can:

- repair a failed driver;
- install an update;
- inspect Device Manager;
- restart a frozen Windows process;
- reboot the computer through the QLC+ System page.

Full remote administration is a separate milestone after the exact Windows edition and security requirements are known. During initial qualification, keep a temporary monitor available nearby. Normal gig operation remains monitorless.

## Internet failover test

With DJ-to-booth ping, QLC+ web, OS2L, MIDI, and fixture output active:

1. start with ReadyNet Internet unavailable;
2. prove all local show functions work;
3. connect venue Ethernet to WAN and prove Internet appears without local interruption;
4. disconnect WAN and prove LTE takes over without local interruption;
5. disable/remove LTE and prove the local show LAN remains operational;
6. test the exact ReadyNet Wi-Fi upstream mode separately;
7. reject any mode that renumbers local devices or blocks peer-to-peer LAN traffic.

The show must continue even if every Internet test fails.

## Capacity note

The LTE520S uses 10/100-Mbps Ethernet ports. That is ample for OS2L, the QLC+ web console, status checks, and stereo archival-audio file copies. A 24-bit/48-kHz stereo PCM stream is approximately 2.3 Mbps before protocol overhead. This network should not, however, become a general guest/video-transfer network while it is carrying show control.

## Official references

- ReadyNet LTE520S support and firmware: https://www.readynetsolutions.com/lte520s-support-and-downloads
- ReadyNet repeater/bridge FAQ: https://www.readynetsolutions.com/faq
- QLC+ OS2L: https://docs.qlcplus.org/v5/plugins/os2l
- QLC+ web interface: https://docs.qlcplus.org/v5/advanced/web-interface
- QLC+ command-line parameters: https://docs.qlcplus.org/v5/advanced/command-line-parameters
- VirtualDJ options: https://virtualdj.com/manuals/virtualdj/appendix/optionslist.html