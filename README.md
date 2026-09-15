# Qt5 电子相册 PhotoAlbum

基于 **100ask IMX6ULL 开发板** 的 Qt5 电子相册应用。支持播放 MP4 视频、显示 PNG/JPG/BMP 图片，以目录为播放列表，支持自动轮播与触摸滑动/点击手动切换。所有代码在 x86_64 Ubuntu 主机上交叉编译，部署到 ARM 开发板运行。

- 仓库地址：`git@github.com:Celeron1996/photo_album.git`
- 目标平台：100ask IMX6ULL（Linux 4.9.88 + Buildroot Qt 5.12.8）
- 开发记录：见 [`DEVELOPMENT_RECORD.md`](DEVELOPMENT_RECORD.md)

## 功能特性

| 需求 | 实现 |
| --- | --- |
| 播放 MP4、显示 PNG/JPG/BMP | `QMediaPlayer` + `QVideoWidget` 播放视频；`QImageReader` 读取并按屏幕预缩放显示图片 |
| 自定义播放列表 | 点击「选择目录」，通过 `QFileDialog` 选择任意目录 |
| 播放列表即路径 | 自动遍历所选目录（不递归）下的 `*.mp4 *.png *.jpg *.jpeg *.bmp`，按文件名排序生成列表 |
| 自动轮播 | 图片固定 5 秒切换、视频播完自动切下一个、到列表末尾循环 |
| 手动控制 | 上/下一张按钮、触摸左右滑动、点击画面切下一个、鼠标拖拽、键盘 ←/→/空格 |

其他：全屏 1024x600、中文界面、音量滑块（0-100，自动记忆）、静音开关、播放列表面板、记住上次目录。

---

## 1. 运行平台

### 1.1 硬件平台（开发板）

| 项目 | 参数 |
| --- | --- |
| 开发板 | 100ask IMX6ULL（韦东山） |
| SoC | NXP i.MX6ULL，ARM Cortex-A7 单核 @ 792MHz |
| 内存 | 512MB DDR3（系统可见约 489MB） |
| 存储 | eMMC（根分区 `/dev/mmcblk1p2`，约 1.5GB） |
| 显示 | 7 寸 LCD，1024x600，`/dev/fb0`（mxs-lcdif，32bpp，stride 4096） |
| 触摸 | goodix 电容触摸屏（`/dev/input/event1`） |
| 音频 | WM8960 codec，`/dev/snd`（ALSA） |
| 网络 | 以太网，板子 IP `192.168.1.14`（示例开发主机 `192.168.1.22`） |
| 其他 | USB 键鼠；**无 VPU/GPU**，视频只能软解 |

> i.MX6ULL 没有硬件视频解码单元，1080p 视频会卡顿，建议播放 **≤640x480** 的 MP4。

### 1.2 软件平台（开发板）

| 项目 | 版本/说明 |
| --- | --- |
| 内核 | Linux 4.9.88（100ask 定制） |
| 根文件系统 | Buildroot 2020.02（busybox），从 **eMMC 启动** |
| Qt | **Qt 5.12.8**（buildroot 编译，平台插件 linuxfb/eglfs/minimal） |
| 显示环境变量 | `/etc/profile` 已导出 `QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0`、`QT_QPA_FONTDIR=/usr/lib/fonts` |
| 多媒体后端 | GStreamer 1.16.2（isomp4、libav、openh264、alsa、fbdevsink 等插件齐全） |
| 中文字体 | `/usr/lib/fonts/msyh.ttc`（微软雅黑） |
| Qt 运行库 | `/usr/lib/libQt5{Core,Gui,Widgets,Network,Multimedia,MultimediaWidgets}.so.5` |
| Qt 插件 | `/usr/lib/qt/plugins/`（platforms、imageformats、mediaservice） |

> 板子实际从 eMMC 启动，`/home/book/nfs_rootfs/` 的 NFS 根文件系统**不是**当前运行的系统；部署请用 `scp`。

---

## 2. 依赖环境

### 2.1 开发主机（Ubuntu 18.04 x86_64）

| 依赖 | 路径/版本 | 用途 |
| --- | --- | --- |
| 交叉工具链 | `arm-buildroot-linux-gnueabihf-gcc`（buildroot 7.5.0） | 编译 ARM 程序 |
| SDK | `/home/book/100ask_imx6ull-sdk/` | 工具链与 buildroot 输出 |
| buildroot qmake | `/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/bin/qmake` | 编译 Qt 应用（Qt 5.12.8） |
| buildroot sysroot | `/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot` | Qt 头文件/库 |
| Qt Creator（可选） | 任意版本 | 图形化编译，kit 名 `100ask_imx6ull` |
| ffmpeg / ImageMagick（可选） | `ffmpeg`、`convert` | 生成测试素材 |

### 2.2 开发板

Qt 运行库、GStreamer 插件、中文字体在 100ask 出厂根文件系统中已预装，无需额外部署。

---

## 3. 目录说明

```text
photo_album/                 # 仓库根目录（即本应用源码目录）
├── photo_album.pro          # qmake 工程文件（Qt Widgets + Multimedia）
├── main.cpp                 # 程序入口：QSettings 标识、中文字体加载、全屏显示
├── albumwindow.h / .cpp     # 主窗口：界面构建 + 播放控制 + 滑动/点击/键盘交互
├── playlistmodel.h / .cpp   # 播放列表：目录扫描、排序、上/下一项循环
├── scripts/                 # 板端辅助脚本（ALSA 混音器初始化）
│   ├── S51alsa              #   开机恢复混音器状态（/etc/init.d/）
│   └── asound.state         #   WM8960 混音器状态（耳机音量 110）
├── README.md                # 本文件
├── DEVELOPMENT_RECORD.md    # 开发/部署/验证过程记录
└── .gitignore
```

---

## 4. 开发环境搭建

### 4.1 工具链与 SDK

1. 获取 100ask IMX6ULL SDK 并解压到 `/home/book/100ask_imx6ull-sdk/`（包含 `ToolChain/`、`Buildroot_2020.02.x/`）。
2. 在 `~/.bashrc` 末尾添加并 `source ~/.bashrc`：

   ```sh
   export ARCH=arm
   export CROSS_COMPILE=arm-buildroot-linux-gnueabihf-
   export PATH=$PATH:/home/book/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin
   ```

3. 验证：

   ```sh
   arm-buildroot-linux-gnueabihf-gcc -v        # 应显示 gcc 7.5.0 (buildroot)
   /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/bin/qmake -v   # 应显示 Qt 5.12.8
   ```

### 4.2 （可选）Qt Creator

- qmake：`/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/bin/qmake`
- sysroot：`/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot`
- kit 名称：`100ask_imx6ull`
- 打开 `photo_album.pro` 即可编译。

---

## 5. 编译

在仓库根目录执行（影子构建，产物在 `build/`）：

```sh
cd photo_album
mkdir -p build && cd build
/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/bin/qmake \
    ../photo_album.pro CONFIG+=debug CONFIG+=qml_debug
make -j4
# 产物：./PhotoAlbum（ARM 32 位 ELF）
```

> 新增/删除源文件后需重新执行 qmake。

---

## 6. 部署与运行

### 6.1 部署

板子从 eMMC 启动，`/tmp` 为 tmpfs，推荐部署到 `/root`：

```sh
scp build/PhotoAlbum root@192.168.1.14:/root/
scp -r <测试素材目录> root@192.168.1.14:/root/album_media/
```

### 6.2 运行

非交互式 SSH 不会加载 `/etc/profile`，需要显式指定 Qt 环境变量：

```sh
# 前台运行
ssh root@192.168.1.14 'cd /root && QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 \
    QT_QPA_FONTDIR=/usr/lib/fonts ./PhotoAlbum'

# 后台运行 + 日志
ssh root@192.168.1.14 'cd /root && nohup env QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 \
    QT_QPA_FONTDIR=/usr/lib/fonts ./PhotoAlbum >/tmp/photoalbum.log 2>&1 &'
```

首次启动默认扫描 `/root/album_media`；之后会记住上次选择的目录（`/root/.config/100ask/PhotoAlbum.conf`）。

### 6.3 截图验证（framebuffer）

无 GUI 远程桌面的情况下，直接抓取开发板 framebuffer 验证显示效果：

```sh
ssh root@192.168.1.14 'dd if=/dev/fb0 bs=4096 count=600' > fb.raw
convert -size 1024x600 -depth 8 bgra:fb.raw fb.png
```

### 6.4 生成测试素材（主机，可选）

```sh
ffmpeg -f lavfi -i testsrc2=size=640x480:rate=15 -t 8 -c:v libx264 -pix_fmt yuv420p video_red.mp4
ffmpeg -f lavfi -i smptebars=size=640x480:rate=15 -t 6 -c:v libx264 -pix_fmt yuv420p video_bars.mp4
convert -size 1024x600 gradient:red-yellow img_01.png
convert -size 1024x600 gradient:green-blue img_02.jpg
convert -size 800x600 plasma:fractal img_03.bmp
```

---

## 7. 代码架构

```text
main.cpp
  └─ QApplication：设置 QSettings 组织名(100ask)/应用名(PhotoAlbum)，加载 msyh.ttc 中文字体
  └─ AlbumWindow::showFullScreen()

AlbumWindow（albumwindow.h/.cpp）—— UI 与播放控制
  ├─ 顶部：状态栏（文件名 [序号/总数] 自动/手动 + 音量滑块 0~100）
  ├─ 中部：QStackedWidget
  │     ├─ 页 0：QLabel（图片，等比缩放居中）
  │     └─ 页 1：QWidget（居中固定尺寸的 QVideoWidget，视频输出）
  │     └─ 右侧：QListWidget 播放列表面板（可开关，宽 260px）
  ├─ 底部：控制栏（选择目录 / 列表 / 上一张 / 播放暂停 / 下一张 / 自动轮播 / 静音 / 退出）
  └─ QMediaPlayer（GStreamer 后端，音量由滑块控制并记忆，静音切换）

PlaylistModel（playlistmodel.h/.cpp）—— 播放列表
  ├─ setDirectory()：扫描所选目录（不递归），过滤扩展名，按名称排序
  ├─ next() / prev()：循环切换
  └─ isVideoFile() / isImageFile() / isSupportedFile()
```

**关键流程**

1. 启动：读取 `QSettings` 的 `lastDir`，不存在则用 `/root/album_media`，自动加载并播放。
2. 选择目录：`QFileDialog::getExistingDirectory` → `PlaylistModel::setDirectory` → 刷新列表 → 播放第 0 项。
3. 播放单项：`playCurrent()` 判断类型 → 视频走 `displayVideo()`（`QMediaPlayer::setMedia/play`），图片走 `displayImage()`（`QImageReader` 预缩放后设置到 QLabel）。
4. 自动轮播：图片由 5 秒单次定时器切换；视频由 `mediaStatusChanged == EndOfMedia` 触发切换；到列表尾部循环。
5. 手动控制：上/下一张按钮、触摸滑动/点击（事件过滤器）、键盘 ←/→/空格/Esc。
6. 配置持久化：`QSettings` 保存在板子 `/root/.config/100ask/PhotoAlbum.conf`。

**支持的多媒体格式**：视频 `.mp4`；图片 `.png`、`.jpg`、`.jpeg`、`.bmp`（大小写不敏感）。

---

## 8. 二次开发指南

### 8.1 修改支持的文件类型

编辑 `playlistmodel.cpp` 顶部的后缀表：

```cpp
const char *const kVideoSuffixes[] = { "mp4", 0 };                       // 新增视频格式
const char *const kImageSuffixes[] = { "png", "jpg", "jpeg", "bmp", 0 }; // 新增图片格式
```

> 新增视频格式需要板端 GStreamer 有对应解码器（如 `.avi` 需 avi demux 插件）。

### 8.2 修改图片自动切换间隔

`albumwindow.cpp`：

```cpp
static const int kImageIntervalMs = 5000;   // 单位毫秒，默认 5 秒
```

### 8.3 修改滑动/点击灵敏度

`albumwindow.cpp`：

```cpp
static const int kSwipeThreshold = 80;   // 判定为滑动的水平位移（像素）
static const int kClickThreshold = 12;   // 判定为点击的最大位移
```

触摸事件被 Qt 合成为鼠标事件，因此 `eventFilter()` 只处理 `MouseButtonPress/Release` 即可同时支持触摸屏和鼠标。

### 8.4 修改界面 / 增加按钮

- 界面全部由代码构建，入口是 `AlbumWindow::buildUi()`。
- 新增按钮：创建 `QPushButton` → 设置 `setMinimumHeight(46)` → 加入 `barLayout` → `connect(...)` 到槽函数。
- 全屏分辨率在 `main.cpp` 的 `showFullScreen()`，如需固定窗口可改为 `resize(1024,600)`。

### 8.5 新增媒体类型或功能

- 新的显示页：在 `buildUi()` 中给 `m_stack` 添加控件，在 `playCurrent()` 中按类型 `setCurrentWidget()`。
- 视频进度条：`QMediaPlayer::positionChanged/durationChanged` 信号。
- 保存多个播放目录：在 `QSettings` 中保存 `QStringList`（当前只保存 `lastDir`）。

### 8.6 修改后的编译部署循环

```sh
cd build && make -j4
scp PhotoAlbum root@192.168.1.14:/root/
ssh root@192.168.1.14 'killall PhotoAlbum; cd /root && nohup env QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 \
    QT_QPA_FONTDIR=/usr/lib/fonts ./PhotoAlbum >/tmp/photoalbum.log 2>&1 &'
ssh root@192.168.1.14 'cat /tmp/photoalbum.log'
```

### 8.7 Qt 5.12.8 API 注意事项（本平台踩过的坑）

- `QCameraViewfinder` 没有 `viewfinderSettings()` / `viewfinderSettingsChanged()`，分辨率等信息要从 `QVideoProbe` 的 `QVideoFrame` 或 `QCamera` 能力查询接口获取。
- `QCamera::StandbyState` 不存在（只有 `UnloadedState/LoadedState/ActiveState`）。
- `QVideoFrame::pixelFormatName()` 不存在，需要自己映射枚举名。
- 重载信号（如 `QMediaPlayer::error(QMediaPlayer::Error)`）建议用旧式 `SIGNAL/SLOT` 连接，槽函数可以省略参数。
- `QVideoWidget` 在 linuxfb 平台可正常渲染（Qt 自带 GStreamer 视频 sink，软渲染，不依赖 OpenGL）。
- 触摸屏（本板特有）：udev 把 goodix 触摸屏误标为 tablet（`ID_INPUT_TABLET=1`），linuxfb 默认优先使用的 libinput 会忽略该设备；同时 DTS 的 `touchscreen-size-x/y` 声明为 800x480，而 gt9xx 驱动实际输出 1024x600 坐标且不做缩放，Qt 按 800x480 归一化会把坐标放大，表现为「画面点击正常、底部按钮点不到」。`main.cpp` 启动时自动处理：禁用 libinput、扫描并显式指定触摸设备、通过 `EVIOCSABS` 按屏幕实际分辨率修正上报范围。
- 视频随机报 `Internal data stream error.`：Qt 5.12 的 `QPainterVideoSurface::present()` 在上一帧尚未绘制完成时返回 false，Qt 的 GStreamer sink 将其当作致命错误（flow error），qtdemux 随即报错。带音轨时视频按音频时钟推帧，单核板子上全屏缩放绘制太慢必然触发。解决：`QVideoWidget` 固定为视频原始尺寸并居中（1:1 绘制，见 `albumwindow.cpp` 的 `kVideoWidth/kVideoHeight`）。
- 音频输出：系统 PulseAudio 的 socket 路径带随机后缀（`/tmp/pulse-XXXX/native`），不设置 `PULSE_SERVER` 时 GStreamer 连不上音频（视频仍可静音播放）。`main.cpp` 启动时自动扫描并设置 `PULSE_SERVER`。

---

## 9. 常见问题（FAQ）

**Q1：`ssh: connect to host 192.168.1.14 port 22: No route to host`？**
开发板网络不稳定或未开机。先 `ping 192.168.1.14`，多试几次；确认板子已启动、网线连接正常。

**Q2：视频播放卡顿/丢帧/无法播放？**
i.MX6ULL 无 VPU，只能软解。720p60、1080p 这类视频无法实时解码，请先转码（在性能较好的主机上执行）：

```sh
ffmpeg -i 源视频.mp4 \
    -vf "scale=640:360:force_original_aspect_ratio=decrease,pad=640:360:(ow-iw)/2:(oh-ih)/2" \
    -r 24 -c:v libx264 -profile:v baseline -level 3.0 -preset veryfast -crf 23 \
    -c:a copy -movflags +faststart 输出.mp4
```

日志中的 `Warning: "A lot of buffers are being dropped."` 是单核 A7 上带音轨视频的常见警告，不影响播放。

**Q3：界面中文显示为方框？**
确认板端 `/usr/lib/fonts/msyh.ttc` 存在，并且运行时设置了 `QT_QPA_FONTDIR=/usr/lib/fonts`。

**Q4：程序启动报 `could not find or load the Qt platform plugin "linuxfb"`？**
确认设置了 `QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0`，且板端存在 `/usr/lib/qt/plugins/platforms/libqlinuxfb.so`。

**Q5：程序从哪里开始找媒体文件？**
首次启动默认扫描 `/root/album_media`；之后会记住上次选择的目录（`/root/.config/100ask/PhotoAlbum.conf`）。

**Q6：MP4 播放没有声音？**
本板 WM8960 编解码器的耳机输出音量上电默认为 0（静音），且根文件系统没有 ALSA 初始化脚本。仓库提供了 `scripts/S51alsa`（开机执行 `alsactl restore`）和 `scripts/asound.state`（已调好 `Headphone Playback Volume = 110` 的混音器状态），部署方法：

```sh
scp scripts/S51alsa root@192.168.1.14:/etc/init.d/S51alsa
scp scripts/asound.state root@192.168.1.14:/var/lib/alsa/asound.state
ssh root@192.168.1.14 'chmod +x /etc/init.d/S51alsa && /etc/init.d/S51alsa start'
```

执行后立即生效，重启后由 S51alsa 自动恢复。若需调整音量，修改 `asound.state` 中 `Headphone Playback Volume` 的 `value.0/value.1`（范围 0~127）后重新 restore 即可。

---

## 10. 致谢

- 韦东山 100ask IMX6ULL 系列课程与文档
- Qt 5.12 官方文档

> 本项目仅供学习使用。
