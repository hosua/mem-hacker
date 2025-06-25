# mem-hacker

`mem-hacker` is a tool similar to Cheat Engine and GameConqueror. It can scan and search for values in process memory and write values to memory addresses. This tool is Linux only, as MacOS does not share the same `ptrace` functionality as Linux. As of writing, this tool is also only tested on a little endian 64-bit CPU, and I have not wrote any logic to support other architectures yet.

To compile:
```bash
./build.sh
cd build
make
sudo ./mem-hacker
```

`sudo` is required for ptrace to have read/write access.

Uses FTXUI for the awesome TUI!

### Some Things I Want to Add

* The ability to search for existing values that increased/decreased
* Searching/writing `double` values
* The ability to search an approximate value for `float` and `double` (also, the ability to specify how precise or non-precise the approximation will be)

### Demo

https://github.com/user-attachments/assets/754c6563-abd2-4d33-9c0a-7213885ed7fa

The demo demonstrates me searching for the score value in yetris and writing a new score.


### Known Issues

* Moving the mouse while typing something to search clears the input field right now for whatever reason
* Searching for a value that returns many results completely borks the UI and makes the application unusable (e.g., searching for a silly value like 0)
* I have ran into issues where values that should be found are not being found. I'm not entirely sure if this was fixed at some point but if it wasn't, it is likely a logical error with the bit alignment.
* No support for Windows and I don't plan on supporting it in the future
* Only tested on a Little Endian 64-bit CPU
