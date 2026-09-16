# AGENTS.md

Qt5 electronic album for the 100ask IMX6ULL board (Linux 4.9.88 + Buildroot Qt 5.12.8, linuxfb). Sources are at the repo root; `README.md` has the full guide and `DEVELOPMENT_RECORD.md` the debugging history.

## Build

```sh
mkdir -p build && cd build
/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/bin/qmake ../photo_album.pro CONFIG+=debug CONFIG+=qml_debug
make -j4        # re-run qmake after adding/removing files
```

## Deploy & run (board boots from eMMC, not NFS)

```sh
scp build/PhotoAlbum root@192.168.1.14:/root/          # kill the running app first
ssh root@192.168.1.14 'cd /root && nohup env QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 \
    QT_QPA_FONTDIR=/usr/lib/fonts ./PhotoAlbum >/tmp/photoalbum.log 2>&1 &'
```

Board media lives in `/root/image/`; settings in `/root/.config/100ask/PhotoAlbum.conf` (`lastDir`, `volume`). Audio needs `scripts/S51alsa` + `scripts/asound.state` installed on the board (see README FAQ Q6).

## Verify visually

```sh
ssh root@192.168.1.14 'dd if=/dev/fb0 bs=4096 count=600' > fb.raw
convert -size 1024x600 -depth 8 bgra:fb.raw fb.png
```

## Gotchas (all handled in code — don't regress)

- Images: `QImageReader::setDecideFormatFromContent(true)` (files are often misnamed).
- Videos: transcode to 340x200@24 H.264 Baseline + AAC (README FAQ Q2); the `QVideoWidget` is fixed to 340x200 and centered because `QPainterVideoSurface::present()` returning false is fatal to Qt's GStreamer sink. Only image→image switches animate.
- Touch: `main.cpp` disables libinput, picks the goodix device by `ABS_MT_POSITION_X/Y` and fixes the abs range via `EVIOCSABS` (udev mislabels it as tablet; DTS says 800x480 but the driver emits 1024x600).
- Audio: `main.cpp` auto-detects the PulseAudio socket (`/tmp/pulse-XXXX/native`) and sets `PULSE_SERVER`; WM8960 headphone volume defaults to 0 (see `scripts/`).
- `QPropertyAnimation` needs explicit `setStartValue/setEndValue` or it won't run.
- Qt 5.12.8 has no `QVideoFrame::pixelFormatName()`, no `QCamera::StandbyState`.
