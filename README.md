# opus-hook-discord

> A modded version of Discord's Opus internals, delivering stereo audio without gain limits.

---

## 🚧 Status

**Work in Progress**

Pre-built binaries are available in the `builds/` folder.

---

## 🔧 Features

* Override default gain limits
* Stereo audio support

---

## 📥 Installation

1. **Download or clone** this repository:

   ```bash
   git clone https://github.com/r7uw6mexx2d6a8g7bex8x0tg6nh73/opus-hook-discord.git
   ```
2. **Copy the pre-built file** from the `builds/` folder:

   * **Windows**: `builds/voice.node`
   * **macOS/Linux**: `builds/libopus.so` (or the appropriate library file)
3. **Locate your Discord installation** path:

   * **Windows**: `%LOCALAPPDATA%\Discord\app-<version>\modules\discord_voice`
   * **macOS**: `~/Applications/Discord.app/Contents/Resources/modules/discord_voice`
   * **Linux**: `/usr/share/discord/modules/discord_voice`
4. **Replace** the original voice module file with the modded version you copied.
5. **Restart** Discord.

---

## 🚀 Usage

Simply launch Discord and enjoy enhanced stereo audio.

---

## 🤝 Contributing

Contributions and pull requests are welcome! Please open an issue or submit a PR with your suggestions.

---

## 📄 License

Specify your license here.
