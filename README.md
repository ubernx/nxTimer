# nxTimer

nxTimer is a lightweight alternative speedrun timer to LiveSplit, specifically designed for **S.T.A.L.K.E.R.: Shadow of Chernobyl** (supporting versions 1.0000 & 1.0006).

## Features & Configuration

You can customize the timer's behavior by modifying the `Settings.txt` file.

### Settings Parameters

- **category**: Allows you to modify and display a brief description of the run category.
  - *Example:* `category: Any% (Novice, 1.0000);`
- **segment_time (ON/OFF)**: Enables or disables an additional timer that resets on every split action.
  - *Example:* `segment_time ON;`
- **show_splits (ON/OFF)**: Shows or hides the splits table containing predefined segments.
- **splits_total (ON/OFF)**: Toggles timestamp formatting.
  - `ON`: Timestamps are displayed in **absolute** format.
  - `OFF`: Timestamps are displayed in **relative** format.

### Controls

Four timer control keys are fully customizable:
- `timer_start_split`
- `timer_reset`
- `timer_skip`
- `timer_undo`

> **Note:** If any listed settings are missing or incorrect, they will be considered invalid, and defaults will be loaded instead.

---

## Developer Guide

For those interested in modifying or building this project from source, follow these instructions to configure your environment using **CLion**.

### Step 1: MSYS2 Setup

1. Install [MSYS2](https://www.msys2.org/), preferably in the `C:/` directory.
2. Open the MSYS2 terminal (`C:/msys64/`) and run the following command regarding dependencies:
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-clang mingw-w64-x86_64-lld mingw-w64-x86_64-gdb mingw-w64-x86_64-make mingw-w64-x86_64-polly mingw-w64-x86_64-compiler-rt mingw-w64-x86_64-qt6
   ```
3. When the Qt package installer requests which subpackages to install, press **Enter** to select all.

### Step 2: Toolchain Setup in CLion

1. Open CLion **Settings**.
2. Navigate to **Build, Execution, Deployment > Toolchains**.
3. Click **Add** and select **MinGW** (optionally rename it to something like "MingGW Clang").
4. Configure the compilers:
   - **C Compiler:** `C:\msys64\mingw64\bin\clang.exe`
   - **C++ Compiler:** `C:\msys64\mingw64\bin\clang++.exe`
5. Move your newly created Toolchain to the top of the list so it takes priority over other toolchains.
