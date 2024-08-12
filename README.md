# Basic Pitch Vamp Plugin

<p align="center">
    <a href="https://github.com/Ircam-Partiels/basic-pitch-vamp-plugin/actions/workflows/ci.yml"><img src="https://github.com/Ircam-Partiels/basic-pitch-vamp-plugin/actions/workflows/ci.yml/badge.svg" alt="Workflows"></a>
  </p>

The Basic Picth plugin is an implementation of the [Basic Pitch](https://github.com/spotify/basic-pitch) automatic music transcription (AMT) library, using lightweight neural network, developed by [Spotify's Audio Intelligence Lab](https://research.atspotify.com/audio-intelligence/) as a [Vamp plugin](https://www.vamp-plugins.org/).

The basic-pitch Vamp Plugin has been designed for use in the free audio analysis application [Partiels](https://forum.ircam.fr/projects/detail/partiels/).

<p align="center">
<img src="./resource/Screenshot.png" alt="Screenshot" width=720>
</p>

## Installation

Download the Basic Pitch Vamp plugin installation package for your operating system from the [Releases](https://github.com/Ircam-Partiels/basic-pitch-vamp-plugin/releases) section and run the installer. 

## Use 

Launch the Partiels application. In a new or existing document, create a new analysis track with the Basic Pitch plugin. Modify the model or the analysis parameters via the property window. Please refer to the manual available in the [Releases](https://github.com/Ircam-Partiels/basic-pitch-vamp-plugin/releases) section for further information.

## Compilation

The compilation system is based on [CMake](https://cmake.org/), for example:
```
cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest -C Debug -VV --test-dir build
```

## Credits

- **[Basic Pitch Vamp plugin](https://www.ircam.fr/)** by Pierre Guillot at IRCAM IMR Department
- **[Basic Pitch](https://github.com/spotify/basic-pitch)** model by Spotify's Audio Intelligence Lab
- **[Vamp SDK](https://github.com/vamp-plugins/vamp-plugin-sdk)** by Chris Cannam, copyright (c) 2005-2024 Chris Cannam and Centre for Digital Music, Queen Mary, University of London.
- **[Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension)** by Pierre Guillot at [IRCAM IMR department](https://www.ircam.fr/).  
