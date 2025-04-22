# 📦 DRM/KMS + EGL Rotating Cube Demo

This project demonstrates rendering a **rotating textured cube** using **DRM/KMS**, **EGL**, and **OpenGL ES 2.0**.

It includes two rendering backends:

- 🟦 **DRM Dumb Buffer Renderer** – uses software framebuffer with `glReadPixels`.
- 🟩 **GBM Renderer (with gbm_bo)** – uses `gbm_bo` and `EGLImageKHR` with OpenGL FBO rendering using a **surfaceless EGL context** (no `gbm_surface`).

---

## ✅ Features

### 🖥 DRM Dumb Buffer Renderer (`main_drm.c`)
- Uses **DRM/KMS atomic commits** to scan out frames.
- Allocates **dumb buffers** via `DRM_IOCTL_MODE_CREATE_DUMB`.
- Creates a **surfaceless EGL context**.
- Renders a rotating textured cube into an **OpenGL FBO**.
- Copies rendered output using `glReadPixels()` into the dumb buffer.
- Displays each frame using **atomic commits**.

### 📺 GBM Buffer Renderer (`main_gbm.c`)
- Uses a **surfaceless EGL context** (`EGL_KHR_surfaceless_context`).
- Allocates **GBM buffer objects** (`gbm_bo`) with `GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT`.
- Converts `gbm_bo` into an **EGLImageKHR**.
- Creates a **GL texture** from the `EGLImageKHR` and attaches it to a **framebuffer object (FBO)**.
- Renders the rotating textured cube into the FBO (fully GPU-accelerated).
- Displays each frame using **DRM atomic commits**.
- No use of `gbm_surface`, `eglCreateWindowSurface`, or `glReadPixels`.

---

## 📁 Project Structure

├── build_drm.sh # Build script for dumb buffer renderer 
├── build_gbm.sh # Build script for GBM renderer 
├── container.jpg # Texture image for the cube 
├── cube_render.cpp # Shared OpenGL cube rendering logic 
├── cube_render.h # Header for rendering logic 
├── drm_render_cube.txt # Sample debug log (DRM mode) 
├── gbm_render_cube.txt # Sample debug log (GBM mode) 
├── main_drm.c # Entry point for dumb buffer renderer 
├── main_gbm.c # Entry point for GBM renderer 
├── README.md # This file └
|── render_report.txt # Optional performance report


---
## 🚀 Performance Notes
Achieves ~250+ FPS on supported hardware.

Both backends render 1000 frames in a loop by default.

GBM version is fully GPU-accelerated (no glReadPixels).

Dumb buffer version is compatible with systems lacking GBM.

---

## 🔧 Build Instructions

### 🔨 Compile

```bash
# Build DRM Dumb Buffer renderer
./build_drm.sh

# Build GBM-based renderer
./build_gbm.sh

# Run DRM dumb buffer renderer
./cube_demo_drm

# Run GBM renderer
./cube_demo_gbm

./cube_demo_drm >> render_report.txt


