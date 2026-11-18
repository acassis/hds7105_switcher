# HDS7105 / HDS7106 SWITCHER UDP protocol (reverse engineered)

Source: `SWITCHER.exe` (PE32, Qt 5 static, MinGW **Debug** build with DWARF,
built 2020 from `G:\Qt_Project\SWITCHER`). Addresses below are VMAs in that
binary (`.text` base 0x401000; COFF symbol value + 0x401000).

## Transport

| Item | Value | Evidence |
|---|---|---|
| Transport | UDP/IPv4 | `QUdpSocket` everywhere |
| Device port | **7000** (0x1B58) | every `writeDatagram(..., 0x1b58)` |
| Discovery | broadcast to `QHostAddress::Broadcast` (255.255.255.255):7000 from an **unbound** (ephemeral) socket | `SWITCHER_DATA::setSearcheQuipment` 0x823f9c |
| Control socket | bound to local port 7000, sends unicast to device IP:7000 | `HDS7105_workdata::initUdp` 0x8d2f1c |

## Frame format

All frames, both directions:

```
off  size  field
0    1     0xA5          magic
1    1     0x6C          magic
2    2     total length, little endian (whole frame incl. magic and trailer)
4    1     type          (device type: 0xA5 switcher; 0xE0/0xE5 "HIS" streaming module; 0x81 search)
5    1     id            (device id; 0xFF in search/replies)
6    1     0x01          (Set_ipCom: 0x00 if type==0xA5, 0x01 if type==0xE0, *absent* otherwise)
7    9     0x00 ...      (replies carry info here, e.g. byte 14 = child type)
16   n     payload       (opcode + args)
L-3  2     checksum, little endian = 16-bit sum of bytes [0 .. L-4]
L-1  1     0xAE          end marker
```

`SWITCHER_DATA::CalcCheckSum` 0x8231c8: `uint16 s=0; for(i<n) s+=buf[i];` called with `n = L-3`.

Builders (all in `SWITCHER_DATA`):

| Builder | L | payload | addr |
|---|---|---|---|
| `None_Char_Data(type,id,op)` | 20 | `op` | 0x8237c0 |
| `Single_Char_Data(type,id,op,v)` | 21 | `op v` | 0x823da8 |
| `Double_Char_Data(type,id,op,a,b)` | 22 | `op a b` | 0x823af4 |
| `SendBroadcastBuf()` | 20 | type=0x81 id=0xFF, payload `FF` | 0x823c74 |
| `Set_ipCom(type,id,op,ip[4],mask[4],gw[4])` | 32 | `op ip mask gw` (byte 6 rule above) | 0x82412c |
| `CreateMcuUdpDataBuf_ipDns(type,id,dns[4])` | 28 | `23 dns[4] 02 50 3B 8C` | 0x8d2cfc |
| `CreateMcuUdpDataBuf_url(type,id,ST_RTMP_URL)` | 809 | `67 00 idx namelen name[16] urllenL urllenH url[768]` | 0x8d26ac |

Golden vector (discovery): `A5 6C 14 00 81 FF 01 00 00 00 00 00 00 00 00 00 FF A5 03 AE`.

## Observed on real hardware (HDS7105(2021), 2026-11-18)

* Discovery reply, 41 bytes, unicast from device :7000 to the search
  socket's ephemeral port (IP TTL 255):
  `A5 6C 29 00 A5 FF 01 00 00 00 00 00 00 00 25 00 | FF 00 "HDS7105(2021)-9-C" 17 60 55 | 67 08 AE`
  → model byte 14 = 0x25, signature = `17 60 55`.
* State reply (0x49), 62 bytes:
  `... 25 00 | 49 00 00 00 00 00 23 39 42 24 24 24 24 00 03 11 23 | 02 01 01 03 64 04 64 01 02 00 00 01 00 02 00 00 00 00 00 00 00 03 00 03 00 00 | A6 05 AE`
  → after the 2nd `#`: PGM 2, PVW 1, MIX, CH1 IN1 @100, CH2 IN2 @100,
  transition MIX, border on/white/2, small, H=V=0, rate 1, PGM out 03
  (1080P30), mute 0. Between the two `#`: `39 42 24 24 24 24 00 03 11`
  (meaning unknown).
* The 3 "signature" bytes are the low half of the device MAC address
  (MAC `xx:xx:xx:17:60:55`, signature `17 60 55`); that is how the
  original pairs a switcher with its streaming module.
* The device only answers requests whose source IP is in its own subnet
  (a broadcast from 10.42.0.1 got no reply; the same frame from
  192.168.1.15 did).
* Behind a Wi-Fi router that isolated its LAN port, ARP between PC and
  device failed, so unicast could not be delivered. Broadcast requests
  still worked (see `--broadcast-control`). On a direct link ARP and
  unicast work normally.

## Discovery reply handling (`SWITCHER_DATA::Get_Bind_Dev` 0x82320c)

Receiver validates A5 6C / length / AE / checksum, then:

* `pkt[4]==0xA5 && pkt[5]==0xFF` → new switcher entry:
  `Dev_IP=sender`, `Dev_Type=pkt[4]`, `Dev_ID=pkt[5]`, `Dev_Child_Type=pkt[14]`,
  `Sign1..3 = pkt[L-6], pkt[L-5], pkt[L-4]` (last 3 payload bytes).
* `(pkt[4]==0xE0 || pkt[4]==0xE5) && pkt[5]==0xFF` → HIS module; attached to the
  switcher whose Sign1..3 equal this packet's `pkt[L-6..L-4]`:
  `His_IP=sender`, `His_Type=pkt[4]`, `His_ID=pkt[5]`.

## HDS7105 commands (`HDS7105_workdata`, sent to switcher IP, type/id from discovery)

| Method | Frame | Opcode | Arg | addr |
|---|---|---|---|---|
| SendCDMPGMCH(ch) | Single | 0x80 | channel | 0x8d0430 |
| SendCMDPVWCH(ch) | Single | 0x81 | channel | 0x8d05a0 |
| SendCMDCUT | None | 0x83 | | 0x8cf630 |
| SendCMDAUTO | None | 0x84 | | 0x8cfd20 |
| SendCMDFTB | None | 0x85 | | 0x8cf788 |
| SendCMDPIP(v) | Single | 0x86 | v | 0x8cfa38 |
| SendCMDTRANSITIONS(1) MIX | None | 0x8D | | 0x8d1548 |
| SendCMDTRANSITIONS(2) FADE | None | 0x8E | | 0x8d1548 |
| SET_WIPE | None | 0x8F | | 0x8d3044 |
| SendCMDTRANSITIONS(3..6) wipe | 0x8F, then Single 0x92 | 0x92 | 3→0x25 4→0x29 5→0x23 6→0x27 | 0x8d1548 (jump table 0x112297c) |
| SendCMDPIP_Border_Enable(v) | Single | 0x9D | v | 0x8d2b8c |
| SendCMDPIP_Border_Color(v) | Single | 0x9E | v | 0x8d28ac |
| SendCMDPIP_Border_Width(v) | Single | 0x9F | v | 0x8d2a1c |
| SendCMDPIP_H_Position(u16) | Double | 0xA0 | lo, hi | 0x8d2190 |
| SendCMDPIP_V_Position(u16) | Double | 0xA1 | lo, hi | 0x8d2340 |
| SendCMDPIP_Border_Size(v) | Single | 0xA2 | v | 0x8d24f0 |
| SendCMDF1(idx) | Single | 0xA3 | {7,0B,09,01,03,0D,05}[idx] | 0x8d31c8 |
| SendCMDRATE(v) | Single | 0xB1 | v | 0x8cfe78 |
| SendCMDCH1(idx) | Single | 0xB9 | {7,3,4,5,6,8,0}[idx] | 0x8cf2e8 |
| SendCMDCH2(idx) | Single | 0xBA | {7,3,4,5,6,8,0}[idx] | 0x8cf48c |
| SendCMDMIX | None | 0xCB | | 0x8cf8e0 |
| SendCMDAFV | None | 0xCC | | 0x8cf190 |
| SendCMDSTILL | None | 0xCD | | 0x8d0710 |
| SendCMDCH1_VOL(v) | Single | 0xCE | v | 0x8d0bd0 |
| SendCMDCH2_VOL(v) | Single | 0xCF | v | 0x8d0d40 |
| SendCMDF2 | None | 0xD0 | | 0x8d336c |
| SendCMDF3 | None | 0xD4 | | 0x8d34c4 |
| SendCMDF7 | None | 0xD6 | | 0x8d361c |
| SendCMDRead7105state | None | 0x49 | | 0x8d2038 |
| SendCMD_SetIP(ip) | Set_ipCom to switcher | 0x05 | ip, mask=0, gw=0 | 0x8d08dc |

### HIS (streaming module) commands — sent to HIS IP with His_Type/His_ID

| Method | Frame | Opcode | addr |
|---|---|---|---|
| SendCMD_Read_His_IP | Set_ipCom, all zero | 0x24 | 0x8d1cf8 |
| SendCMD_Set_HIS_IP(ip,mask,gw) | Set_ipCom | 0x05 | 0x8d18c4 |
| SendCMD_Set_HIS_DNS(dns) | ipDns builder | 0x23 | 0x8d1e78 |
| SendCMD_Read_Url(idx) | Single | 0x6A | 0x8d13e4 |
| SendCMD_Set_Url(idx,name,url) | url builder | 0x67 | 0x8d1168 |

### HDS7105 replies (`HDS7105_workdata::AnalysisData` 0x8d00c0)

Validated frame `pkt` (Qt `QByteArray::split` semantics):

* `pkt[4]==A5 && pkt[5]==FF && pkt[16]==0x49` → `split(0x23)[2]` → status (UI: `HDS7105UI::on_toRead_7105Dtat`).
* `pkt[4] in {E0,E5} && pkt[5]==FF`:
  * `pkt[16]==0x6A && pkt[17]==0` → `split(0x6A)[1]` → URL info (UI: `Plane_Botton::on_ToShow_HisUrl`).
  * `pkt[16]==0x24 && pkt[17]==0` → `split(0x24)[1]` → HIS IP info (UI: `Plane_Botton::on_ToShow_HisInfo`).


Port note: the original takes `QByteArray::split(sep)[n]` over the *whole*
frame. That truncates at the next separator byte inside the data (e.g. an IP
octet of 36 = 0x24, or a PIP position low byte of 0x23) and is thrown off by
a separator in the header. The C port selects the same slice — bytes after
the n-th separator found from offset 16, up to the checksum — without
those failure modes (`sw_slice_after()` in `switcher/src/proto.c`).

### Vendor opcode names (DWARF `enum DW_SWITCHER_7105CMD`, hds7105_workdata.h:11)

SET_IP=05 ALL_DATA=49 PGM_CH=80 PVM_CH=81 CUT=83 AUTO=84 FTB=85 PIP=86
SHOW_MIX=8D SHOW_DIP=8E SET_WIPE=8F SHOW_WIPE=92 PIP_BORDER_ENABLE=9D
PIP_BORDER_COLOR=9E PIP_BORDER_WIDTH=9F PIP_WND_X_POS=A0 PIP_WND_Y_POS=A1
PIP_WND_WIDTH=A2 PGM_OUT=A3 RATE=B1 AUDIO_SRC1=B9 AUDIO_SRC2=BA AFV=CB
AUDIO=CC STILL=CD CH1VOL=CE CH2VOL=CF VIDEO1_SRC=D0 UACVOL=22 ADUIO_MUTE=D4
ROTATE=D5 OSDROTATING=D6.

So the F-keys are: F1 = PGM_OUT (resolution popup), F2 = VIDEO1_SRC,
F3 = ADUIO_MUTE, F7 = OSDROTATING. F4/F5/F6/F8 exist in the UI but are not
connected. Note the UI's **AFV** button sends 0xCC (enum `AUDIO`) and its
**MIX** button sends 0xCB (enum `AFV`); the port keeps the UI behaviour.

## HDS7105 status reply (0x49) — `HDS7105UI::on_toRead_7105Dtat` 0xd17418

`s` = bytes after the 2nd `#` (0x23) of the reply.

| s[] | meaning | UI setter (addr) | values |
|---|---|---|---|
| 0 | PGM channel | Below_Widget::set_PGMCH_Style 0x775310 | 1..4, 9=BAR (jump table 0x111cff8) |
| 1 | PVW channel | set_PVWCH_Style 0x77538c | same (0x111d020) |
| 2 | audio mode | Middle_widget::set_AFVorMIX_Style 0x7d6540 | 0=AFV (source combos disabled), 1=MIX |
| 3 | CH1 source | set_CH1_Style 0x7d4f5c | 7 PHONE_IN, 3 IN1, 4 IN2, 5 IN3, 6 IN4, 8 MIC_IN1, 0 MIC_IN2 (table 0x111c2c4) |
| 4 | CH1 volume | set_CH1VOL_Style 0x7d6084 | 0..100 |
| 5 | CH2 source | set_CH2_Style 0x7d5380 | as CH1 |
| 6 | CH2 volume | set_CH2VOL_Style 0x7d60b8 | 0..100 |
| 7,8 | transition | set_Trans_Style(s7,s8) 0x7d5ab8 | s7: 1=MIX 2=FADE 4=WIPE; wipe s8: 0 Left, 2 Up, 4 Right, 6 Down (0x7d57cc) |
| 9 | FTB | Upper_Widget::set_FTB_Style 0x7b3f3c | 1 = FTB active (button blinks 400 ms) |
| 10 | PIP on | Middle_widget::set_PIP_Style 0x7d5460 | 0/1 (also shows/hides the PIP panel) |
| 11 | PIP border enable | Plane_Right::set_BorderEnable_Style 0x705f0c | 0 OFF, 1 ON |
| 12 | PIP border color | set_BorderColor_Style 0x705db0 | 0 White 1 Red 2 Blue 3 Green |
| 13 | PIP border width | set_BorderWidth_Style 0x705e48 | 2..7 |
| 14 | PIP size | set_Size_Style 0x705bd8 | 0 Small 1 Medium 2 Large |
| 15,16 | PIP H position | set_Hposition_Style 0x705d28 | s15 \| s16<<8 |
| 17,18 | PIP V position | set_Vposition_Style 0x705d6c | s17 \| s18<<8 |
| 19 | STILL | set_Stll_Style 0x7d5774 | 0/1 |
| 20 | rate | Below_Widget::set_Rate_Style 0x774b74 | 0..2 = RATE1..3 |
| 21 | PGM out (F1) | Upper_Widget::set_F1_Style 0x7b3e68 | 07,0B,09,01,03,0D,05 = 1080P60,P50,I60,I50,P30,P25,P24 |
| 24 | audio mute (F3) | set_F3_Style 0x7b3ee4 | 0 off, FF on |

The state is read once when the control window opens (`DevTypeID` →
`SendCMDRead7105state`) and when the **DS** button is pressed; there is no
polling timer.

## HDS7105 UI behaviour

* **PGM/PVW** (`Below_Widget`): buttons 1–4 and BAR, button-group ids 1,2,3,4,9.
  Click → highlight (PGM red, PVW green) + 0x80/0x81 id.
* **CUT** → 0x83; locally swaps PGM/PVW highlight; CUT lit 300 ms.
  **AUTO** → 0x84; after 900 ms swaps PGM/PVW highlight and unlights AUTO.
  (`checked` flag at Below_Widget+0x18 is always 0, so the swap always happens.)
* **RATE1..3** (ids 0..2) → 0xB1 id.
* **Transitions** (`Middle_widget`): ids 1 MIX, 2 FADE, 3 Up, 4 Down, 5 Left, 6 Right → `SendCMDTRANSITIONS(id)`.
* **AUDIO**: AFV → 0xCC, MIX → 0xCB; CH1/CH2 source combos (PHONE_IN, IN1..IN4,
  MIC_IN1, MIC_IN2) → 0xB9/0xBA {7,3,4,5,6,8,0}[idx]; volumes 0..100 step 5,
  default 50 → 0xCE/0xCF.
* **FUNC**: PIP toggles 0x86 1/0 and shows the PIP panel (window 1000→1200 wide);
  STILL toggles its light and sends 0xCD.
* **PIP panel** (`Plane_Right`): H/V position (initial range 0..960, step 10),
  Size Small/Medium/Large (re-ranges H/V to 1440×810, 1280×720, 960×540, then 0xA2 idx),
  Border Enable OFF/ON (0x9D), Border Width 2..7 (0x9F), Border Color
  White/Red/Blue/Green (0x9E).
* **Upper row**: Return (back to search, rescans), F1 (popup "PGM Out" combo →
  0xA3 table), F2 (momentary, lit 200 ms, 0xD0), F3 toggle (0xD4), F7 toggle (0xD6),
  FTB (0x85, blinks every 400 ms while active), DS (read state, 0x49).
* **Settings dialog** (`Plane_Botton`, 670×440):
  * SWITCHER IP: "Set IP" → Set_ipCom op 05 to the switcher, mask/gw 0; the
    tool then talks to the new IP.
  * HIS INFO SET: IP/Subnet/Gateway/DNS; "Read Status" → 0x24; "Set IP" →
    op 05 with ip/mask/gw; "Set DNS" → 0x23 frame.
  * URL SET: Index 1..8 (sent as 0..7), Other Name (≤16), Url (≤768);
    "Read Status" → 0x6A idx; "Set Url" → 0x67 frame.

### HIS replies (`Plane_Botton`)

* 0x24 (`on_ToShow_HisInfo` 0x778688), slice `d` after the first 0x24:
  IP = d[2].d[3].d[4].d[5], Subnet = d[6..9], Gateway = d[10..13], DNS = d[20..23].
* 0x6A (`on_ToShow_HisUrl` 0x7784a0), slice `d` after the first 0x6A:
  index = d[1], name_len = d[2], name = d[3..], url_len = d[19] | d[20]<<8,
  url = d[21..] (NUL bytes skipped).

## HDS7106 (`HDS7106_workdata`, sent to device IP:7000 with type/id from discovery)

Vendor enum `DW_SWITCHER_CMD` (hds7106_workdata.h:25) names in brackets.
`CH_NB[8] = {0,1,3,4,5,6,7,8}` (0x1544600) maps audio source index → device code.

| UI action | Frame | Opcode | Argument | addr |
|---|---|---|---|---|
| PGM 1..6 | Single | 0x80 [SET_PGM_CH] | id 1..6 | 0x8d4088 |
| PVW 1..6 | Single | 0x81 [SET_PVM_CH] | id 1..6 | 0x8d44d8 |
| T-bar | Double | 0x82 [SET_SLIDER_POS] | dir 0/1, pos | 0x8d4c00 |
| CUT / AUTO | None | 0x83 / 0x84 | | 0x8d3dc4 / 0x8d49a8 |
| PIP on/off | Single | 0x86 | 1/0 | 0x8d41f8 |
| MIX / FADE / wipe | as 7105 (0x8D / 0x8E / 0x8F + 0x92) | | | jump table 0x1122acc |
| PIP border en/color/width | Single | 0x9D / 0x9E / 0x9F | idx / idx / 2..7 | |
| PIP H / V | Double | 0xA0 / 0xA1 | lo, hi | 0x8d59bc / 0x8d5b6c |
| PIP size | Single | 0xA2 | 0..2 | 0x8d603c |
| PGM Out | Single | 0xA3 | {07,0B,09,01,03,0D,05}[idx] | 0x8d50fa |
| POP on/off | Single | 0xA7 [POP] | 1/0 | 0x8d4368 |
| POP window | Single | 0xA8 [POP_SOURCE] | 1 / 2 (toggle) | 0x8d53c0 |
| POP border en/color/width | Single | 0xA9 / 0xAA / 0xAB | 1/0 / idx / 2..7 | 0x8d68dc / 0x8d648c / 0x8d65fc |
| POP H / V | Double | 0xAC / 0xAD | lo, hi | 0x8d5d1c / 0x8d5ecc |
| CH1 / CH2 volume | Double | **0xB8** [AUDIO_CHX_VOLUME] (both) | CH_NB[source idx], vol | 0x8d4d86 / 0x8d4f56 |
| CH1 / CH2 source | Single | 0xB9 / 0xBA | CH_NB[idx] | 0x8d3afa / 0x8d3c6a |
| MIX / AFV buttons | None | 0xCB / 0xCC | | 0x8d3f1c / 0x8d398c |
| Set IP | Set_ipCom | 0x05 | ip, mask=gw=0 | 0x8d46e7 |

* Audio sources: Phone_IN, RCA_IN, IN1..IN6. Volumes 0..100, default 60, step 10.
* T-bar range 0..126: upward stroke sends `82 00 v`; at 126 the PGM/PVW display
  swaps and direction flips; downward stroke sends `82 01 (126-v)`; flips back
  when `126-v == 125`. AUTO is lit while the bar is between the ends.
* CUT/AUTO swap the displayed PGM/PVW only when PIP is off.
* The 7106 UI never reads state back: `Read7106State_Data` (0x8d54d0) is a
  no-op and `SendCMDReadstate` (0x49) is never called.
* POP panel: Window button toggles 0xA8 1/2; H/V refuse to send until a window
  has been chosen; Size fixed "Large" (disabled); Border Enable button toggles 0xA9.
