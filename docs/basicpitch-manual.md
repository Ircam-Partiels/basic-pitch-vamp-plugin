<h1 align="center">Basic Pitch Vamp Plugin Manual</h1>

<p align="center">
<i>Version APPVERSION for Windows, Mac & Linux</i><br>
<i>Manual by Pierre Guillot</i><br>
<a href="www.ircam.fr">www.ircam.fr</a><br><br>
</p>

<p align="center">
<img src="../resource/Screenshot.png" alt="Example" width="400"/>
</p>

## Table of contents

1. [Introduction](#introduction)
2. [Requirements](#system-requirements)
3. [Installation](#installation)
5. [Credits](#credits)

## Introduction

The Basic Pitch plugin is an implementation of the [Basic Pitch](https://github.com/spotify/basic-pitch) automatic music transcription (AMT) library, using lightweight neural network, developed by [Spotify's Audio Intelligence Lab](https://research.atspotify.com/audio-intelligence/) as a [Vamp plugin](https://www.vamp-plugins.org/). The Basic Pitch model is embedded in the plugin. 

The Basic Pitch plugin provides three parameters, `Frame Threshold`, `Onset Threshold` and `Minimum Note Duration`, which allow you to control the sensitivity of the pitch detection. The Basic Pitch model is multiphonic, and the Voice Index parameter is used to select the voice. The Basic Pitch plugin analyses the pitch in the audio stream and generates curves corresponding to the frequencies. The amplitude of the note is associated with each result, enabling the data to be filtered according to a threshold.

The Basic Pitch Vamp Plugin has been designed for use in the free audio analysis application [Partiels](https://forum.ircam.fr/projects/detail/partiels/).

## Requirements

- Windows 10
- MacOS 10.15 (ARM)
- Linux

## Installation

Use the installer for your operating system. The plugin dynamic library (*basicpitch.dylib* for MacOS, *basicpitch.dll* for Windows and *basicpitch.so* for Linux) and the category file (*basicpitch.cat*) will be installed in your operating system's Vamp plugin installation directory:
- Linux: `~/vamp`
- MacOS: `/Library/Audio/Plug-Ins/Vamp`
- Windows: `C:\Program Files\Vamp`

## Credits

- **[Basic Pitch Vamp plugin](https://www.ircam.fr/)** by Pierre Guillot at IRCAM IMR Department.
- **[Basic Pitch](https://github.com/spotify/basic-pitch)** model by Spotify's Audio Intelligence Lab.
- **[TensorFlow](https://github.com/tensorflow/tensorflow)** originally developed by Google Brain team.
- **[Vamp SDK](https://github.com/vamp-plugins/vamp-plugin-sdk)** by Chris Cannam, copyright (c) 2005-2024 Chris Cannam and Centre for Digital Music, Queen Mary, University of London.
- **[Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension)** by Pierre Guillot at [IRCAM IMR department](https://www.ircam.fr/).  

