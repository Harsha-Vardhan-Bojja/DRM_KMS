# 📦 DRM/KMS + EGL Cube Demo

This project demonstrates rendering a rotating textured cube using **DRM/KMS** with **EGL** and **OpenGL ES 2.0**, using **dumb buffers** for framebuffer allocation.

---

## ✅ Features

- Uses **DRM/KMS atomic commits** for display.
- Renders with **EGL + GLES2** using a **surfaceless context**.
- Uses **dumb buffers** to allocate memory without GBM.
- Rendering output is copied via `glReadPixels` into dumb buffers for display.
- Rotating **textured cube** animation.

---

## ⚙️ Build Instructions

**Build the application:**
   ./build.sh

Switch to a TTY session (e.g., TTY3):

Fn + Alt + F3

Run the demo:

./cube_demo

(Optional) Save performance logs:

./cube_demo >> render_report.txt

**Performance Notes**

The application currently runs at ~250+ FPS under optimal conditions and renderd in loop for 1000 frames.
