# Controller Input Mapper for Sentakki

A unified C++ controller input mapper with three modes: **Multi-Touch**, **Mouse**, and **Keyboard**. With real-time visual overlay and works with any XInput or DirectInput controller.

Designed for playing Sentakki (osu! lazer mod) and other game if control somehow apply.

Built with Cursor AI assistance.

---

## Features

**Mode Selection at Startup**
```
[1] Touch Mode - Multi-touch for Sentakki
[2] Mouse Mode - Cursor + click control  
[3] Keyboard Mode - Number keys 1-8
```

### **Mode 1: Touch (Multi-Touch)** ⭐
- **TRUE multi-touch** using Windows UWP InputInjector API
- **No touch hardware required!** Works on any Windows 10/11 PC
- **2 independent touch points** - Both sticks work simultaneously
- LB + Left Stick = Touch point 0
- RB + Right Stick = Touch point 1

### **Mode 2: Mouse (Cursor + Click)**
- both stick controls where the cursor will be
- Either bumper = Left mouse button held
- Both bumpers = Alternates between sticks every frame
- Mouse returns to center when released

### **Mode 3: Keyboard (Number Keys 1-8)**
- Both sticks map to directional keys 1-8
- LB + Left Stick = Keys based on left stick angle
- RB + Right Stick = Keys based on right stick angle
- Keys switch dynamically as you rotate
- Conflict prevention when both sticks point same direction

### **Visual Overlay (All Modes):**
- **Smooth fading effects** - Appears only when sticks move
- **Real-time stick visualization** (Blue = left, Pink = right)
- **Angle indicators** - Arc segments showing direction
- **Variable pen widths** - Thicker as sticks move from center
- **Full-screen overlay** - Transparent, click-through
- **Dynamic refresh rate** - Matches your monitor
- **Smart debug UI** - Shows mode-specific info (toggle with Ctrl+Shift+`)

---

## Quick Start

### **Download & Run (Portable!)**

**Just double-click `ControllerInput.exe` - that's it!**

No installation, drivers, or DLLs needed. Works on any Windows 10/11 PC.

**Requirements:**
- Windows 10 or 11
- XInput or DirectInput compatible controller
- (Optional) Run as admin if Touch mode doesn't work

**Ssu! lazer setting**
Enable touch input: Settings → Input → Touch → Enable
100% Ui Scaling only

---

## 🎮 How to Use

### **1. Start the Program**
Run `ControllerInput.exe` and select your mode:
- Press **1** for Touch mode
- Press **2** for Mouse mode
- Press **3** for Keyboard mode

### **2. Select Controller**
Choose your controller from the list (XInput or DirectInput)

### **3. Play!**
The overlay appears - the position and size is hard coded as of now. and may only work with 100% osu ui scale

### **Controls:**

**Touch Mode:**
- **Left Stick** + Hold **LB**  → Touch point 0
- **Right Stick** + Hold **RLB** → Touch point 1
- Both work simultaneously for true multi-touch!

**Mouse Mode:**
-  Hold **LB** → Mouse follows left stick
- Hold **RB** → Mouse follows right stick  
- Hold **both** → Mouse alternates between sticks
- Release → Mouse returns to center

**Keyboard Mode:**
- **Left Stick** + Hold **LB** → Number keys 1-8
- **Right Stick** + Hold **RB** → Number keys 1-8
- Keys map to 8 directions (45° sectors)

**Keyboard Shortcuts (All Modes):**
- `Ctrl+Shift+Tilde` (the key to the left of 1 and above tab on a qwerty keyboard) → Toggle debug info
- `Ctrl+Alt+Shift+Tilde` → Restart (return to mode selection)
- Close console → Exit program

---

## 🔧 Building from Source

### **Requirements:**
- Visual Studio 2022 with C++ Desktop Development
- Windows 10 SDK (includes C++/WinRT)

### **Build:**
```bash
build.bat
```

This compiles `ControllerInput.exe` from `ControllerInput.cpp`.

**Manual build:**
```bash
cl /EHsc /std:c++17 /await ControllerInput.cpp /link dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerInput.exe
```

**All executables are fully portable** - copy to any Windows 10/11 PC and run!

---

## 🛡️ Security & Antivirus

### **Why might this get flagged?**
- ⚠️ Uses input injection APIs (SendInput, UWP InputInjector)
- ⚠️ Unsigned executable
- ⚠️ Simulates keyboard/mouse/touch (similar to legitimate automation tools)

### **Is it safe?**
✅ **100% Open Source** - All code is visible and auditable  
✅ **No network activity** - Doesn't send data anywhere  
✅ **No persistence** - Doesn't install or modify system  
✅ **Clean build** - Compiled with official Microsoft tools 
✅ **Portable** - Single exe, no hidden files  

### **If flagged by antivirus:**
1. **Add to exclusions** - Safest for personal use
2. **Build from source** - Review code and compile yourself
3. **Virus scan the source** - Upload to VirusTotal if concerned

---

## 📋 Technical Details

### **Input APIs:**
- **Touch Mode:** Windows UWP InputInjector (C++/WinRT)
  - Enables touch injection without physical touch hardware
  - Works on any Windows 10/11 PC
  - No admin required (contrary to old SendInput touch APIs)
  
- **Mouse/Keyboard Modes:** SendInput API
  - Standard Windows input simulation
  - Compatible with most games
  
- **Controller Support:** DirectInput 8 + XInput 1.4
  - Auto-detects Xbox and generic controllers
  - Controller selection menu at startup

### **Rendering:**
- **GDI** for overlay with alpha blending
- **Layered windows** with color key transparency
- **Variable pen widths** for smooth fading
- **Dynamic refresh rate** matching screen (60Hz+)
- **Full-screen overlay** - Click-through, transparent

### **Architecture:**
- Single executable with mode selection
- Clean state management with restart support
- Efficient: Debug info only updates when visible
- Memory safe: Proper cleanup on mode switch

---

## 📦 Portability

**ControllerInput.exe is 100% portable!**

- ✅ **No installation** - Just run the .exe
- ✅ **No external DLLs** - Only uses Windows system libraries
- ✅ **No configuration files** - Self-contained
- ✅ **No registry changes**
- ✅ **Works anywhere** - Copy to USB, cloud, or share directly
- ✅ **No admin required** - (Touch mode works without it!)

**Just copy `ControllerInput.exe` to any Windows 10/11 PC and run!**

---

## 📝 Notes

### **Development:**
- Built entirely with Cursor AI assistance
- Tested with DualShock 4 and Xbox 360 controllers
- Works with any XInput or DirectInput compatible controller
- Open source - feel free to modify!

### **Compatibility:**
- ✅ **osu! lazer / Sentakki** (primary target - confirmed working!)
- ✅ **Other touch/rhythm games** (maimai-style games)
- ✅ **General purpose** - Use for any game needing controller mapping
- ⚠️ **Anti-cheat games** - Some may block synthetic input

### **Removed Features:**
- DS4 touchpad support was attempted but removed due to hardware/driver conflicts
- Legacy separate executables (ControllerToNumberKeys.exe, ControllerToMouse.exe) - now integrated into one program

### **Known Limitations:**
- Mouse mode limited to single cursor (Windows limitation)
- Touch injection may not work in some sandboxed apps
- Some games require specific input modes to be enabled

---

---

## 🙏 Credits

**Author Note:**  
Most of the code is written by Cursor AI, as I don't know C++ at all!

**Special thanks to:**
- Sentakki community for cool mod
- Nutch for telling UWP InputInjector API exist

---

## 📄 License

**MIT License** - Copyright (c) 2025 Jackbuthesuck

This means you can:
- Use it for free (personal or commercial)
- Modify it however you want
- Share it with anyone
- Include it in your own projects
- Sell software that uses it

**The only requirements:**
- Keep the copyright notice if you share the code
- Don't blame me if something breaks (no warranty!)

**TL;DR:** Do whatever you want with it, just don't sue me

For the full legal text, see: https://opensource.org/licenses/MIT