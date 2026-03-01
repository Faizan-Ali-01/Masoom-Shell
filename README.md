# Masoom Shell

Masoom Shell is a custom Windows command-line shell implemented in C. It offers a variety of built-in commands, an advanced input editor, process management utilities, and customization features. It takes all the commands in Roman urdu and English as well. Inspired by UNIX shells, the project is designed to run on Windows using native APIs for process and system information.

## Key Features

- **Built-in commands** with Hindi/Urdu-style names:
  - `help-karo`, `dekhao`, `ghar-jao`, `waqt-batao`, `khatam-karo`
  - File and system utilities: `saaf-karo`, `dastavez-dikhao`, `rasta-batao`, `file-parho`
  - System introspection: `system-info`, `hisaab-karo`, `dhundo`
  - Weather, date, and time: `mausam-batao`, `tarikh-batao`, `waqt-batao`
  - Alias management: `alias-banao`, `alias-dikhao`, `alias-mitao`
  - Process viewer: `process-dikhao` (advanced manager launched via the same codebase)
  - Internet connectivity check: `internet-check`
  - Color theme switching: `rang-badlo`

- **Advanced input handling** with:
  - Arrow keys navigation
  - In-line editing and history support
  - Tab autocomplete for commands and file names

- **Alias system** for command shortcuts, persisted between sessions.
- **Theming** with multiple color schemes.
- **Process manager module** (`process_win.c`) provides:
  - CPU/memory/disk usage overview
  - Process listing with PID, name, memory, CPU, user, path, and priority
  - Filtering and ASCII art banners

## Getting Started

### Prerequisites

- Windows system with a C compiler (e.g. GCC in MinGW or MSVC).
- Development tools for building (makefile or manual compilation commands).

### Compilation

Use the following example command with GCC (MinGW) in a terminal:

```sh
gcc -o masoom_shell masoom_win.c process_win.c -lpsapi -lpdh -lwtsapi32
```

For MSVC, create a Visual Studio project or use the command-line tools, linking the same libraries.

### Running

```sh
.\\masoom_shell.exe
```

Once started, you will see a welcome message and a `:> ` prompt where you can enter built-in commands.

### Using the Shell

- Type `help-karo` to display the manual and a list of supported commands.
- Use arrow keys to navigate through command history.
- Tab completion works for built-in commands and file paths.
- Create aliases with `alias-banao foobar "saaf-karo"` and list them with `alias-dikhao`.
- View system/process information using `system-info` or `process-dikhao`.

## Project Structure

```
Masoom Shell/
├── masoom_win.c      # Main shell implementation
├── process_win.c     # Advanced Windows process manager
└── README.md         # Project overview and instructions
```

## Contribution

Feel free to fork the project and submit pull requests. Issues and feature requests are welcome on GitHub.

## License

Specify your license here (e.g., MIT, GPL, etc.).

Enjoy using Masoom Shell and happy hacking!