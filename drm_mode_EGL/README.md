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

1. **Build the application:**
   ./build.sh

Switch to a TTY session (e.g., TTY3):

Fn + Alt + F3

Run the demo:

./cube_demo

(Optional) Save performance logs:

./cube_demo >> render_report.txt

2. **Performance Notes & Optimization Needs:**

The application currently runs at ~40 FPS under optimal conditions.

After 250+ frames, the performance drops to 1–5 FPS.
