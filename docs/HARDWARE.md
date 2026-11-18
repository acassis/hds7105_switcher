# Hardware notes: DeviceWell HDS7105 / HDS7106

This page collects what is publicly documented about the switchers this
software controls, plus what was observed on a real HDS7105(2021). Sources
are listed at the end.

## The HDS7 "small switcher" family

DeviceWell (Shenzhen) sells these as portable all-in-one live-production
switchers. Each unit combines a multiview processor, an effects generator,
an audio mixer and a control keyboard in a metal case. The control
software, `SWITCHER.exe`, handles the whole family: when you open a device,
it picks the control panel from the model code in the discovery reply (see
`PROTOCOL.md`).

| Model code | Name in the app | Notes |
|---|---|---|
| `0x21` | HDS7105 | HDS7105 control panel |
| `0x24` | HDS7105S | HDS7105 control panel |
| `0x25` | HDS7105(2021) | HDS7105 control panel; the unit used for testing |
| `0x22` | HDS7106 | HDS7106 control panel |

Later models, the HDS7105P, HDS7305, HDS7306 and HDS7308, are listed on
the vendor site and are not in the 2020 `SWITCHER.exe`.

## HDS7105 / HDS7105S / HDS7105P / HDS7305

These are 5-channel HD switchers. Values below come from the official
manual V1.5, which covers all four models.

| | |
|---|---|
| Video inputs | 4× HDMI + 1× DP (DP 1.2). Input 1 can be switched between HDMI and DP; on the panel that is **F2**. |
| Input formats | 480i/576i to 1080p and 2160p, plus PC modes from 800×600 to 3840×2160, auto-adapted |
| Outputs | 1× HDMI PGM, 1× HDMI multiview (PVW, PGM, 4 input monitors, status) |
| Output formats | 1080p60/50/30/25/24, 1080i60/50. This is **F1**, opcode `0xA3`. |
| Audio | HDMI embedded, 3.5 mm line in/out, 2× MIC. Audio follows video (AFV) or a 2-channel mix (MIX) of IN1–4, PHONE_IN, MIC_IN1 and MIC_IN2. Volume 0–100. |
| Effects | MIX, FADE, and WIPE up/down/left/right. Rates RATE1/2/3 = 640/1280/2560 ms, used by AUTO only. Also PIP, STILL (freeze), FTB (fade to black), BAR (colour bars). |
| PIP | H position 0–1440 and V position 0–810 in steps of 10, for the small size. Sizes small/medium/large; border on/off, width 2–7, four colours. |
| USB | USB 3.0 UVC output (HDS7105, 7105P, 7305; not 7105S). It shows up as a webcam. |
| Network | RJ45 100 Mbit/s, used for control by the PC software and for firmware upgrades |
| Serial | 9600 baud, 8N1 on the 15-pin D-sub (pin 2 RX, 3 TX, 5 GND) |
| Tally / GPI | DB-15: tally outputs for PGM 1–4 and PVW 1–4 (active low). GPI inputs can act as PGM1–4, BAR, CUT, AUTO, MUTE, FTB or STILL (foot pedal and similar). |
| Power | 12 V DC, under 25 W; 0–50 °C |
| Size | HDS7105/7105S 250 × 120 × 40 mm; HDS7105P/7305 280 × 110 × 36 mm. Older documents give 236 × 105 × 47.5 mm. |

These features exist only on some models (from the manual's comparison
table):

| | 7105S | 7105 | 7105P | 7305 |
|---|---|---|---|---|
| USB 3.0 UVC output | – | ✓ | ✓ | ✓ |
| Luma / chroma key | – | ✓ | ✓ | ✓ |
| T-bar, 3 audio faders | – | – | ✓ | ✓ |
| TF-card recording, RTMP streaming (STREAM port) | – | – | – | ✓ |

### Function keys

The vendor manual and the opcodes in `SWITCHER.exe` agree on F1–F3:

| Key | Manual | Opcode sent by the PC software |
|---|---|---|
| F1 | Output resolution | `0xA3` + format code |
| F2 | Input 1 source: HDMI or DP | `0xD0` (`VIDEO1_SRC`) |
| F3 | Mute all audio | `0xD4` (`ADUIO_MUTE`) |
| F4 / F5 | Record / pause (HDS7305) | not sent by the 2020 software |
| F6 | RTMP stream on/off (HDS7305) | not sent |
| F7 | Assignable "KEY" function (manual); "OSD rotating" in the 2020 software | `0xD6` (`OSDROTATING`) |
| F8 | MIC gain | not sent |

### Streaming module ("HIS")

`SWITCHER.exe` has a second device type, `0xE0`/`0xE5`, with its own IP
address. It is paired with a switcher by the last three bytes of the
switcher's MAC address. Its settings are an IP/subnet/gateway/DNS and **8
stream slots**, each with an index, a user name of up to 16 characters and
an RTMP URL.

This matches the manual's description of the HDS7305 **STREAM** port. That
port has its own IP address and a web interface, and it stores 8 groups of
stream addresses. The HDS7105 has no such port, so the app's "HIS INFO" and
"URL SET" sections stay disabled for it. The tested HDS7105(2021) did not
answer as a streaming module.

### Software features newer than `SWITCHER.exe`

The current DeviceWell manual describes features the 2020 tool does not
have: logo upload (BMP 256×128, 4 slots), time sync, and a web interface
for streaming and recording. This app does not implement them either.

## HDS7106

The HDS7106 is a 6-channel switcher with a T-bar. Values below are from the
official product page.

| | |
|---|---|
| Video inputs | 4× 3G-SDI + 2× HDMI (PGM/PVW buttons 1–6) |
| Outputs | PGM: 2× SDI + 1× HDMI; multiview: 1× HDMI + 1× SDI; AUX: 1× SDI, custom PGM/PVW |
| Audio | 3.5 mm in/out, RCA in/out, SDI embedded. The app's sources are Phone_IN, RCA_IN and IN1–IN6. |
| Effects | MIX, FADE, wipes, PIP and POP windows, CUT, AUTO and T-bar |
| Control | RJ45 100M network, RS-232 (9600 8N1, DB-9: 2 RX, 3 TX, 5 GND), tally. Internal modules use a CAN bus. |
| Power | 12 V DC |
| Size | 255 × 153 × 58 mm |

## Observed on the tested HDS7105(2021)

* **Discovery reply:** model `0x25`, with the text `HDS7105(2021)-9-C`
  after the opcode. The last three payload bytes equal the low three
  bytes of its MAC address.
* **Subnet:** the switcher only answers requests whose source address is
  in its own IPv4 subnet. A broadcast from another subnet gets no reply.
* **ARP behind a router:** with the switcher behind a Wi-Fi router that
  isolated its LAN port, ARP failed while broadcasts passed. On a direct
  cable everything worked normally.
* **Link:** 100 Mbit/s.
* **USB:** the device enumerates as `534d:2021` "MACROSILICON 7105 Video
  In". That is a UVC video device carrying the PGM picture (also visible to
  OBS), plus a HID interface. The HID interface is not used by
  `SWITCHER.exe`; control is over the network only.
* **Unsolicited frames:** the switcher sometimes sends 21-byte frames of
  its own, such as `81 00` (opcode, value) after a PVW command. These look
  like acknowledgements or change reports. The app does not use them yet.

## Sources

* **DeviceWell HDS7105/7105S/7105P/7305 user manual V1.5:**
  <http://www.devicewell.com/dwdoc/dl/document/HDS/HDS7305_V2023/HDS7105+7105S+7105P+7305_GCE_V15.pdf>
* **DeviceWell HDS7 series product pages:**
  <http://devicewell.com/index.php/en/live-broadcast-en/hds7-series-small-switcher-cn>
  * HDS7105_V2021:
    <http://devicewell.com/index.php/en/live-broadcast-en/hds7-series-small-switcher-cn/629-hds7105-v2021-en>
  * HDS7106:
    <http://devicewell.com/index.php/en/live-broadcast-en/hds7-series-small-switcher-cn/367-hds7106>
* **Retail listing:** "Devicewell HDS7105 (2021 version) 5-CH HD Live
  Production Switcher, 4 HDMI + 1 DP input, 1 HDMI PGM output, 1 HDMI
  multi-view output":
  <https://www.amazon.co.jp/-/en/Devicewell-HDS7105-Production-Switcher-Multi-view/dp/B08M8PCL9P>
* **Manual mirrors:**
  <https://www.manualslib.com/manual/1851713/Devicewell-Hds7105.html>,
  <https://manualshive.com/en/manual/devicewell/hds7105/2488807>

The vendor site's HTTPS certificate had expired when this page was written
(2026-11), so the HTTP links are given.
