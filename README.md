# vad-vamp-plugin
The VAD Vamp plugin is an implementation of the VAD voice activity detection library developed by WebRTC project authors as a Vamp plugin 

## Installation

Download the VAP Vamp plugin installation package for your operating system from the [Releases](https://github.com/Ircam-Partiels/vad-vamp-plugin/releases) section and run the installer. 

## Use 

Launch the Partiels application. In a new or existing document, create a new analysis track with the VAD plugin. Modify the analysis parameters via the property window. Please refer to the manual available in the [Releases](https://github.com/Ircam-Partiels/vad-vamp-plugin/releases) section for further information.

## Compilation

The compilation system is based on [CMake](https://cmake.org/), for example:
```
cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest -C Debug -VV --test-dir build
```

## Credits

- **[libfvad](https://github.com/dpirch/libfvad)** Copyright (c) 2011, The WebRTC project authors. All rights reserved. Copyright (c) 2016 Daniel Pirch
- **[VAD Vamp plugin](https://www.ircam.fr/)** by Pierre Guillot at IRCAM IMR Department.   
- **[Vamp SDK](https://github.com/vamp-plugins/vamp-plugin-sdk)** by Chris Cannam, copyright (c) 2005-2024 Chris Cannam and Centre for Digital Music, Queen Mary, University of London.  
- **[Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension)** by Pierre Guillot at [IRCAM IMR department](https://www.ircam.fr/).  

