# Amiga-Boing-Ball-Screensaver-for-Windows-10-and-11
A screensaver for Windows 10/11 that mimics the iconic Amiga Boing  Ball demo with options.

# BoingBallSaver

A Windows screensaver inspired by the classic Amiga **Boing Ball Demo**.  
This project recreates the bouncing red‑and‑white checkered ball with modern OpenGL, configurable options, and a nostalgic twist.

---

## ✨ Features

- **Bouncing Boing Ball** with realistic gravity, restitution, and spin physics.
- **Configurable options** via dialog:
  - Floor shadow toggle
  - Wall shadow toggle
  - Grid overlay toggle
  - Sound effects toggle
  - Background color picker
  - Geometry mode toggle (Classic 16×8 vs Smooth 64×32 tessellation)
- **Input triggers**: Screensaver exits on any key press or mouse click.
- **Cursor hiding**: Mouse pointer is hidden during full‑screen saver mode.
- **Preview mode support**: Runs safely inside Windows Display Settings preview window.

---

## 📂 Project Structure

- `BoingBallSaver.cpp` — main source code
- `resource.h` — dialog and control IDs
- `.rc` file — dialog layout and resources
- `sounds/` — Boing ball bounce and wall hit WAV files
- `docs/` — optional screenshots, quick reference cards, or guides

---

## 🛠️ Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/YourUserName/BoingBallSaver.git

2. Open the project in Visual Studio (Windows). 

3. Build the project — output will be a .scr file.

4. Copy the .scr file into your Windows system32 or sysWOW64 directory.

6. Right‑click on your desktop → Personalize → Lock Screen → Screen Saver Settings. Select BoingBallSaver from the list.

⚙️ Configuration:

Open the screensaver’s Settings dialog to adjust:

Floor Shadow: Toggle shadow under the ball.

Wall Shadow: Toggle shadow against the back wall.

Grid Overlay: Toggle floor/wall grid lines.

Sound Effects: Enable/disable bounce sounds.

Background Color: Choose any color for the scene.

Geometry Mode: Switch between Classic (Amiga‑style 16×8 sphere) and Smooth (high‑res 64×32 sphere).

## Releases
Latest:
v1.3 - https://github.com/Sinphaltimus/Amiga-Boing-Ball-Screensaver-for-Windows-10-and-11/releases/tag/v1.3

