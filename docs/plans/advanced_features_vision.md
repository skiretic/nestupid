# NEStupid: Advanced Features Vision

This document outlines a roadmap for "Modern Convenience" and "Polished Aesthetics" features in NEStupid.

## 1. Hybrid State Persistence (Rewind & Save States)

We will implement a hybrid system that provides both reliability (manual saves) and forgiveness (rolling rewinds).

### Configurable Rewind Buffer
The user can select from several "Presets" that balance memory usage and historical depth.

| Preset | History Duration | Memory (Estimated) | Best For... |
| :--- | :--- | :--- | :--- |
| **Minimalist** | 30 Seconds | ~1MB | Quick "Undo" of a bad jump or death. |
| **Standard** | 2 Minutes | ~4MB | Standard gameplay protection without much overhead. |
| **Speedrunner** | 10 Minutes | ~20MB | Practice and routing complex segments. |
| **Marathon** | Infinite (Disk-Backed) | ~60MB/hour | Recording entire playthroughs for verification or archiving. |

### Technical Architecture
- **Snapshots**: Full state serialization including RAM, VRAM, OAM, and Mapper state.
- **Serialization Interface**: We will implement `serialize()` and `deserialize()` functions for every component (`cpu`, `ppu`, `apu`, `memory`, `mapper`).
- **Disk Format**: A custom binary format (or JSON-wrapped binary) for standard `.state` files.

---

## 2. Visual Fidelity & Scaling

To move beyond the raw 256x240 pixel grid, we will introduce modern scaling and filtering.

### Real-Time Scaling
- **Nearest Neighbor**: Clean, pixel-perfect integer scaling (2x, 3x, 4x).
- **Linear Filtering**: Smooths out the raw edges (subtle).
- **Advanced Algorithms**: Implementation of shaders like **xBRZ** or **HQ2X** for high-quality upscaling that preserves character outlines.

### CRT Simulation
- **Scanline Overlays**: Simple transparent lines to mimic old TVs.
- **CRT Shader**: A specialized shader providing Phosphor bloom, curvature, and subtle chromatic aberration for an authentic "retro" feel.

---

## 3. Enhanced Convenience

### Save State Slots
- 10 manual slots per ROM, accessible via keyboard shortcuts (F1-F12) or the native macOS menu.
- **Visual Previews**: Save states will store a tiny 128x120 screenshot to display in the "Load State" menu.

### Fast-Forward / Slow-Motion
- Toggleable speed multiplier (e.g., 200% for grinding, 50% for difficult platforming).
- Implementation via the main emulation loop's clocking mechanism.

---

## 4. Advanced Debugging Suite

To facilitate ROM hacking, translation, and emulator development, we will build a native diagnostic UI.

### Visual Debuggers
- **Nametable Viewer**: Live view of nametables ($2000-$2BFF) with scroll position overlays and attribute grid highlights.
- **Pattern Table Viewer**: Real-time display of tile data (Pattern Table 0 & 1), allowing observation of CHR-RAM bank switching.
- **Palette Viewer**: View current BG and Sprite palettes with color index markers.
- **OAM Viewer**: List of active sprites with their X/Y coordinates, tile IDs, and priority flags.

### Data Inspection
- **Memory Hex Editor**: A real-time viewer/editor for CPU Memory ($0000-$FFFF), allowing live patching of game RAM.
- **Execution Trace**: A circular buffer of the last 1000 instructions executed, including register states, to assist in pinpointing crashes.

---

## 5. Battery-Backed RAM (.sav) Management

Consistent with modern emulator standards, game progress in PRG-RAM will be automatically managed.

### Persistence Logic
- **File Management**: PRG-RAM contents stored in standard `.sav` files (raw binary) in a dedicated `saves/` directory.
- **Persistence Triggers**:
  - **Auto-Save**: RAM content is synced to disk every 30 seconds if any changes are detected.
  - **Save on Exit**: Clean synchronization when the emulator is closed.
- **Verification**: Integrity checks to ensure `.sav` files match the current ROM (via header checksum or filename matching).

---

## 6. Expansion Audio Support

To achieve true "Famicom-level" audio fidelity, we will implement the specialized sound hardware embedded in specific game cartridges.

### Supported Audio Expansions
- **Konami VRC6**: Adds two additional Pulse channels and one Sawtooth channel (used in *Castlevania III* Japanese version).
- **Famicom Disk System (FDS)**: Implements single-cycle Wavetable Synthesis for unique, rich textures.
- **Namco 163**: Adds up to 8 channels of 4-bit Wavetable synthesis (used in *Erika to Satoru no Yume Bouken*).
- **Nintendo MMC5**: Adds two extra Pulse channels identical to the 2A03 (used in *Castlevania III* US).
- **Sunsoft 5B**: Adds 3 channels of square-wave synthesis (essentially a YM2149/AY-8910 clone).

### Technical Integration
- **Modular Mixer**: The main APU mixer will be refactored to support dynamic "plug-in" channels provided by the Mapper.
- **Mixing Levels**: Individual volume balance controls for expansion audio to allow users to tune the "Western" vs "Japanese" hardware sound profile.


