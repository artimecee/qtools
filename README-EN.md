# qtools (English Translation)

An English-translated version of the original [forth32/qtools](https://github.com/forth32/qtools) repository.

`qtools` is a collection of low-level command-line utilities designed for communicating, debugging, and flashing Qualcomm-based LTE modems and cellular modules via serial/TTY interfaces.

---

## 🌐 What's Changed

* **Translated Strings & Logs:** All `printf` messages, console outputs, and error warnings converted from Russian to English.
* **Code Documentation:** Inline comments across `.c` and `.h` source files translated into English for better readability.
* **UTF-8 Encoding Fix:** Source files updated to standard UTF-8 to prevent character encoding issues during Linux compilation.

---

## 🛠 Included Tools

* **`qwflash`**: Partition flashing utility using modem partition tables.
* **`qcio`**: Direct NAND memory operations, bad-block inspection, and chip configuration.
* **`qterminal`**: Interactive serial terminal for sending custom diagnostic commands.
* **`sahara`**: Low-level flasher implementing the Qualcomm Sahara protocol.
* **`efsio`**: Read and write tools for modem EFS file systems.
* **`hdlc`**: HDLC frame builder and parser for Qualcomm diagnostic protocols.

---

## ⚙️ Building the Project

### Prerequisites

You need `gcc` and `make` installed on your Linux distribution:

```bash
# Debian / Ubuntu
sudo apt update && sudo apt install build-essential

# Arch Linux / Manjaro
sudo pacman -S base-devel
