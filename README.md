# mem-hacker

`mem-hacker` is a tool similar to Cheat Engine and GameConqueror. It can scan and search for values in process memory and write values to memory addresses. This tool is Linux only and only UNIX-like environments are planned to be supported. As of writing, this tool is also only tested on a little endian 64-bit CPU, and I have not wrote any logic to support other architectures yet.

TODO: Write compilation/usage instructions

Uses FTXUI for the awesome TUI!

### Demo

https://github.com/user-attachments/assets/909aa0e8-2de6-4e14-a5fc-cb63f133e7bf

### Known Issues

* searching for a value that returns many results completely borks the UI and makes the application unusable (e.g., searching for a silly value like 0)
* `double` is not yet implemented
* I have ran into issues where values that should be found are not being found. I'm not entirely sure if this was fixed at some point but if it wasn't, it is likely a logical error with the bit alignment.
* No support for Windows and I don't plan on supporting it in the future
* Untested on MacOS
* Only tested on a Little Endian 64-bit CPU
