<h1 align="center">Video-Player-for-Numworks</h1>
<p align="center">
    <img alt="Version" src="https://img.shields.io/badge/Version-1.1.1-blue?style=for-the-badge&color=blue">
    <img alt="Stars" src="https://img.shields.io/github/stars/SaltyMold/Video-Player-for-Numworks?style=for-the-badge&color=magenta">
    <img alt="Forks" src="https://img.shields.io/github/forks/SaltyMold/Video-Player-for-Numworks?color=cyan&style=for-the-badge&color=purple">
    <img alt="License" src="https://img.shields.io/github/license/SaltyMold/Video-Player-for-Numworks?style=for-the-badge&color=blue">
    <br>
    <a href="https://github.com/SaltyMold"><img title="Developer" src="https://img.shields.io/badge/Developer-SaltyMold-red?style=flat-square"></a>
    <img alt="Maintained" src="https://img.shields.io/badge/Maintained-Yes-blue?style=flat-square">
    <img alt="Written In" src="https://img.shields.io/badge/Written%20In-C-yellow?style=flat-square">
</p>

_This is a simple video player app for the Numworks calculator._

| Rickroll | Matrix |
|----------|--------|
| <img src="https://github.com/user-attachments/assets/73ac20ed-aa3b-43c1-b8ff-2c6eefd35902" width="200" alt="Rickroll"> | <img src="https://github.com/user-attachments/assets/1dc0c5ae-7666-4559-8f90-535b9280fde1" width="200" alt="Matrix"> |

## 📕 Install the app

To install this app, you'll need to:
1. Download the **`app.nwa` file**, which you can download from the [Releases](https://github.com/SaltyMold/Video-Player-for-Numworks/releases).
2. Download a compatible video file in `MJPEG` format. You can take them from the [samples folder](https://github.com/SaltyMold/Video-Player-for-Numworks/tree/main/samples). Or convert your own videos using tools like FFmpeg with the instructions below.
3. Connect your **NumWorks calculator** to your computer using a USB cable.
4. Head to **[my.numworks.com/apps](https://my.numworks.com/apps)** to send the **`nwa` file** on your calculator along the **`mjpeg` video**.

   
### 🎞️ Converting videos to MJPEG format

Keep the resolution at 320×240. Adjust `-q:v` and `-fps` for quality and size.

With cropping:
```sh
ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=increase,crop=320:240,setsar=1:1,fps=15" -t 00:00:30 -vcodec mjpeg -q:v 24 -an output.mjpeg
```
Without cropping:
```sh
ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=increase,setsar=1:1,fps=15" -t 00:00:30 -vcodec mjpeg -q:v 24 -an output.mjpeg
```

## 📕 How to use the app

| Key   | Action        |
|-------|---------------|
| Back  | Quit app      |
| Shift | Change FPS    |
| EXE   | Toggle debug  |

## 🛠️ Build the app

I made tutorials here :
- [C-App-Guide-for-Numworks](https://github.com/SaltyMold/C-App-Guide-for-Numworks)
- [Numworks-App-Development-Template](https://github.com/SaltyMold/Numworks-App-Development-Template)