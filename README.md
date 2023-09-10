# gnome-dbus-time-controller
A lazy attempt at making a replacement for systemd's timedate for Void to let Gnome set the time properly x) 

Manually setting time and date as well as timezone works in Gnome now

Currently can only work under root priveliges, because i didn't have the time to integrate polkit into the thing yet

Oh and if you have chrony or NPT installed, make sure its disabled because currently internet time management is not implemented and it will cause clock skew and audio issues for bluetooth devices.

# Requirements
`C++17` - gcc or clang with packages for c++ ISO 17

`cmake` - CMake tools

`clang-format` - formatting

`libdbus-c++` - Dbus lib

# Build and install
Building is quite simple, just generate cmake cache and build it
```
mkdir build && cd build
cmake ..
cmake --build .
```
And to install it just run
```
sudo cmake --install .
```
Oh and dont forget to either start it or reboot
```
sudo sv start gnomeDbusTimeService
```

If you are preparing a MR, build with code formatting enabled and run `codeformat` before pushing any commits
```
cmake -DCODEFORMAT=ON ..
make codeformat
```

# Contributing
If you want to contribute code, make sure you ran `codeformat` and open an MR.

If you found an issue, a bug or just think something could be changed for the better, open an issue.
