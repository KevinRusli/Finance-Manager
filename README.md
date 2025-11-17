Personal Finance Manager (C, GTK3, SQLite3, Cairo)

A small desktop application to track personal finances. It uses GTK3 for the UI, SQLite3 for data storage, and Cairo for charts.

## Features
- Add, edit, delete transactions (income/expense)
- Budgets per category with progress
- Savings goals with projected completion date
- Monthly reports (income, expense, balance)
- Charts: expense breakdown by category (Cairo)
- Data persisted in SQLite
- Export transactions to CSV (optional)

## Project structure
- `src/`: C source files
- `include/`: public headers
- `Makefile`: build rules (uses `pkg-config` for GTK3, SQLite3 and Cairo)
- `finance.db`: SQLite database file (created on first run in the working directory)

## Requirements
- A C toolchain (GCC or compatible)
- `make`
- `pkg-config`
- GTK3 development files, SQLite3 development files, and Cairo development files

On Linux (Debian/Ubuntu) these can usually be installed with:

```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-3-dev libsqlite3-dev libcairo2-dev
```

On macOS with Homebrew:

```bash
brew install pkg-config gtk+3 sqlite cairo
```

On Windows, use MSYS2 (recommended):

1. Install MSYS2 from https://www.msys2.org/ and open the "MSYS2 MinGW 64-bit" shell.
2. Update the package database and core packages:

```bash
pacman -Syu
# close and re-open the MSYS2 MinGW 64-bit shell, then:
pacman -Su
```

3. Install the required packages (MinGW-w64 64-bit):

```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-pkg-config mingw-w64-x86_64-gtk3 \
    mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-cairo make
```

Then build using the steps below from that same MinGW shell.

### Quick MSYS2 build example (exact commands)
If you have MSYS2 installed and have the project located at `C:\finance_manager`, do the following in the "MSYS2 MinGW 64-bit" shell:

```bash
# change into the project directory (this is important — "No makefile found" means you're in the wrong directory)
cd /c/finance_manager

# build with the provided Makefile
make

# run the application (in MSYS2 shell)
./finance_manager
```

If you prefer to run the exe from PowerShell after building, run:

```powershell
cd C:\finance_manager
.\finance_manager.exe
```

Common mistakes you ran into
- "make: *** No targets specified and no makefile found. Stop." — you ran `make` from `/c` (root) instead of the project directory. Run `cd /c/finance_manager` first, then `make`.
- Typo in command (e.g., `ma finance_manager`) — ensure you type `make` (or use the `make` commands above).

The pacman package name for SQLite in current MSYS packages is `mingw-w64-x86_64-sqlite3` (not `mingw-w64-x86_64-sqlite`). Use the corrected package list above when installing dependencies.

## Build
From the project root you can build using the provided `Makefile`:

```bash
make
```

This produces the executable `finance_manager` (or `finance_manager.exe` on Windows).

Alternative: compile manually (works in bash / MSYS2 MinGW shell):

```bash
gcc -o finance_manager src/*.c $(pkg-config --cflags --libs gtk+-3.0 sqlite3 cairo) -Wall -Wextra -std=c11 -lm
```

Note: the command substitution `$(pkg-config ...)` is for POSIX shells (bash/MSYS). In a native PowerShell session this syntax won't expand; use the MinGW shell for building or expand the flags manually.

## Run
After building, run the program from the project directory. The application will create a `finance.db` SQLite file in the working directory on first run.

On Linux/macOS or in an MSYS2 shell:

```bash
./finance_manager
```

On Windows PowerShell (after building with MSYS2), run the EXE from the directory where it was built:

```powershell
.\finance_manager.exe
```

If the program reports "Failed to initialize database.", check file permissions in the working directory.

## Usage notes
- Date format used by the app: `YYYY-MM-DD`.
- Month filters accept `YYYY-MM`.
- CSV exports are written to `transactions.csv` in the current working directory.

## Troubleshooting
- "pkg-config: command not found": ensure `pkg-config` is installed and available on PATH. On MSYS2 install `mingw-w64-x86_64-pkg-config` and use the MinGW shell.
- GTK/SQLite/Cairo headers not found: install the development packages for your OS (see Requirements above).
- On Windows prefer building in MSYS2 MinGW shell or use WSL and follow the Linux instructions.

## License
See `LICENSE` (if present) or contact the project owner for license details.


