# DRM_KMS
THIS REPO CONTAINS CODE'S WHICH HELP TO UNDERSTAND ABOUT DRM/KMS

# HOW TO COMPILE
Here is how you can structure the instructions cleanly in a `README.md` file:

---

# DRM Atomic Example

This project demonstrates how to use DRM (Direct Rendering Manager) to interact with the GPU and perform atomic mode setting. It utilizes the `libdrm` and `xf86drm` libraries to communicate with the hardware and render graphics directly.

## Prerequisites

To compile and run this program, ensure the following libraries and development packages are installed:

- **libdrm**: for interacting with DRM.
- **libxf86drm**: for modesetting functionality.

### Installing Dependencies

You can install the required libraries using your package manager:

```bash
sudo apt-get install libdrm-dev libxf86drm-dev
```

## Compilation

To compile the program, use the following `gcc` command:

```bash
gcc -o drm_atomic_example drm_atomic_example.c -ldrm 
             or 
gcc -o your_file your_file.c -ldrm -I/usr/include/libdrm

```

This will generate an executable named `drm_atomic_example`.

## Running the Program

### Switching to a TTY

To run the program, you'll need to use a console that has direct access to the framebuffer device. Follow these steps:

1. **Switch to a TTY**:
   Press `Ctrl + Alt + F1` (or `F2`, `F3`, etc.) to switch to a TTY. The terminal may vary based on your system configuration.

2. **Run the Program**:
   Once you're in the TTY, execute the program with appropriate privileges (you may need `sudo` to access the framebuffer device):

   ```bash
   sudo ./drm_atomic_example
   ```

### Program Execution

After running the program, it will display the rendered image for **5 seconds** before it exits.

---

