# 🛠️ Building `forgewatch`

This guide explains how to build the `forgewatch` file watcher tool from source.

---

## 📦 Requirements

- A C compiler (e.g., `gcc`, `clang`)
- `make` (optional, if you create a Makefile)
- Linux system (uses `inotify`)
- `git` (to clone the repo)

---

## 🔧 Build Instructions

### 1. Clone the repository

```sh
git clone https://github.com/yourname/forgewatch.git
cd forgewatch
````

### 2. Compile the project

If you're using plain `gcc`:

```sh
gcc -o forgewatch src/main.c
```

If you want debug messages compiled in:

```sh
gcc -DDEBUG -o forgewatch src/main.c
```

> This will include debug output like file change detection, process spawning, etc.

---

## 🧪 Testing It

Once built, run:

```sh
./forgewatch init
```

This will generate a `.forgewatchrc` with your settings.

Then simply run:

```sh
./forgewatch
```

---

## 🧼 Clean Up

```sh
rm -f forgewatch
```

---

## 📝 Notes

* **No Makefile is provided by default**, but you can create one if desired.
* Uses `inotify`, so it's **Linux-only** (won’t run on macOS/Windows).
* `.forgewatchrc` must be created before running (or pass args manually).
