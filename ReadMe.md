# `forgewatch` — Universal Build Script Watcher

A small but powerful tool that watches a directory for file changes and re-runs your build script automatically.

Think of it as `dotnet watch`, but for **any project** that uses a shell script to build or run.

---

## Installation

Clone the repository:

```sh
git clone https://github.com/codespace0x25/forgewatch.git
cd forgewatch
```

Then compile and install the binary:

```sh
make
make install
```

> Both steps may ask for your **sudo password**.
> **Do not** run these with `sudo` directly.

---

## Requirements

* Linux (requires [`inotify`](https://man7.org/linux/man-pages/man7/inotify.7.html), which is built into most modern kernels)
* A build script like `build.sh` or similar in your project

Optional but recommended:

* `clang` or `gcc` to compile

---

## How It Works

`forgewatch` watches a specified directory for any file changes—creation, deletion, or modification. When a change is detected, it:

1. Terminates the last build process (if still running)
2. Executes your build or run script

---

### Example Usage

Assume your project structure looks like:

```
my-project/
├── src/
├── build.sh
└── ...
```

for an exapmle for `build.sh`
```bash
#!/usr/bin/env bash

make run

echo "done."
```

You can launch `forgewatch` like this:

```sh
forgewatch ./src ./build.sh
```

Now every time you save a file or add/remove something in `src/`, your build script will automatically be re-run.

---

## Using the `.forgewatchrc` File

You might not always want to run:

```sh
forgewatch ./src ./build.sh
```

every time. That’s why `forgewatch` supports a project-specific config file. Just run:

```sh
forgewatch init
```

in your project root.

### How to Use It

Running `forgewatch init` walks you through setting up your `.forgewatchrc`.
Just answer the prompts—like which directory to watch and what build script to run.

> ⚠️ Note: A Makefile alone won’t work. You’ll need a separate script (like `build.sh`) to call it.

Paths are **relative**, just like if you were running the commands yourself.
So if your build script is `build.sh`, you’d write:

```sh
./build.sh
```

Once the file is created, just run:

```sh
forgewatch
```

and it’ll load the settings from `.forgewatchrc`.

---

## Why Use This?

* Works with **any language or project**
* Blazingly fast
* Lightweight: written in pure C
* Keeps your development loop tight and reactive

---

## Debug Builds

To compile with debug symbols:

```sh
make debug
```

---

## Uninstallation

To remove the program:

```sh
sudo rm /usr/local/bin/forgewatch
```

---

## Future Features (Planned)

