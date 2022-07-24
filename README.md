# pycoreaudio
A Python module written in C++, which allows basic operations with CoreAudio like changing the volume level or mute state.

⚠️ This module only works on macOS!  
⚠️ Apple Silicon compatibility is not tested/guaranteed, as I don't own such a device right now.
Tested on macOS Monterey with Intel CPU.

To compile, run `python3 setup.py build`. Python 3.9 is recommended.  
After the build, a `build` folder should be created, along with some build files - in my case:  
```
├── build
│   ├── lib.macosx-12-x86_64-cpython-39
│   │   └── CoreAudio.cpython-39-darwin.so        <---- The module
│   └── temp.macosx-12-x86_64-cpython-39
│       └── libpycoreaudio.o
```
You can now take the `CoreAudio.cpython-39-darwin.so` and put it wherever you need it. Renaming is not necessary.  
After that, you can just `import CoreAudio` in your Python script.
