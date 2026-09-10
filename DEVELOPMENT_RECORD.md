# Qt5 电子相册（PhotoAlbum）开发记录

在 100ask IMX6ULL 开发板上开发、交叉编译、部署并验证 Qt5 电子相册应用的全过程（2026-09-10，已上板验证）。

## 1. 功能

1. 播放 MP4 视频（`QMediaPlayer` + `QVideoWidget`），显示 PNG/JPG/BMP 图片（`QImageReader` 按屏幕预缩放，省内存）
2. 自定义播放列表：点击「选择目录」，用 `QFileDialog::getExistingDirectory` 选路径
3. 播放列表即路径：自动遍历所选目录（不递归）下的 `*.mp4 *.png *.jpg *.jpeg *.bmp`（大小写不敏感），按文件名排序；`QSettings` 记住上次目录，首次默认 `/root/album_media`
4. 自动轮播：图片固定 5 秒切换、视频播完自动切下一个、列表循环；手动控制：上一张/下一张按钮、触摸左右滑动（阈值 80px）、点击画面切下一个、鼠标拖拽、键盘 ←/→/空格
5. 底部控制栏：选择目录、列表（可开关播放列表）、上一张、播放/暂停、下一张、自动轮播开关、静音、退出；全屏 1024x600，中文界面

## 2. 文件与位置

- 源码：`project/photo_album/`
  - `photo_album.pro` — `QT += core gui widgets multimedia multimediawidgets`，TARGET=`PhotoAlbum`
  - `main.cpp` — 加载 `/usr/lib/fonts/msyh.ttc` 作为全局中文字体
  - `playlistmodel.h/.cpp` — 目录扫描、排序、上/下一项循环
  - `albumwindow.h/.cpp` — 界面、播放控制、滑动/点击/键盘交互
- 影子构建目录：`project/build-photo_album-100ask_imx6ull-Debug/`
- 二进制：`PhotoAlbum`（ARM 32 位，动态链接 Qt 5.12.8）

## 3. 交叉编译

```sh
cd project/build-photo_album-100ask_imx6ull-Debug
/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/bin/qmake \
  /home/book/100ask_development_board/project/photo_album/photo_album.pro CONFIG+=debug CONFIG+=qml_debug
make -j4
```

（与 CameraQt 相同的 buildroot Qt 5.12.8 qmake + sysroot，Qt Creator kit `100ask_imx6ull`）

## 4. 部署与运行（重要：板子从 eMMC 启动）

板子 `root=/dev/mmcblk1p2`，**不是** NFS 根文件系统，`/tmp` 是 tmpfs：

```sh
scp PhotoAlbum root@192.168.1.14:/root/
scp -r test_media root@192.168.1.14:/root/album_media/   # 测试素材
ssh root@192.168.1.14 'cd /root && nohup env QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 QT_QPA_FONTDIR=/usr/lib/fonts ./PhotoAlbum >/tmp/photoalbum.log 2>&1 &'
```

非交互 SSH 不会加载 `/etc/profile`，必须显式给 `QT_QPA_PLATFORM`、`QT_QPA_FONTDIR`。

## 5. 测试素材（host 生成）

```sh
ffmpeg -f lavfi -i testsrc2=size=640x480:rate=15 -t 8 -c:v libx264 -pix_fmt yuv420p video_red.mp4
ffmpeg -f lavfi -i smptebars=size=640x480:rate=15 -t 6 -c:v libx264 -pix_fmt yuv420p video_bars.mp4
convert -size 1024x600 gradient:red-yellow img_01.png
convert -size 1024x600 gradient:green-blue img_02.jpg
convert -size 800x600 plasma:fractal img_03.bmp
```

板子自带 `/root/my.mp4`（MPEG-4）也可直接测试。

## 6. 上板验证方法（framebuffer 截图）

```sh
ssh root@192.168.1.14 'dd if=/dev/fb0 bs=4096 count=600' > fb.raw
convert -size 1024x600 -depth 8 bgra:fb.raw fb.png
```

验证结果：状态栏与中文按钮正常显示；PNG/JPG/BMP 依次播放；MP4 画面正常（含时间码），`EndOfMedia` 自动切下一个；列表循环正常。

## 7. 注意事项 / 踩坑

- `QVideoWidget` 在 linuxfb 下可用（Qt 的 GStreamer 后端用自带 `QGstVideoRenderer` 软渲染，不依赖 GL）
- i.MX6ULL 无 VPU，只能软解：测试用 640x480；1080p 会卡
- 播放带音轨的 MP4 时日志出现 `A lot of buffers are being dropped.`（GStreamer 警告，单核 A7 上正常现象）
- `QSettings` 保存在 `/root/.config/100ask/PhotoAlbum.conf`
- 触摸会被 Qt 合成为鼠标事件，滑动/点击只需处理鼠标事件即可（无需 `QSwipeGesture`）
