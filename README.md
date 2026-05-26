# pywintray

[![build & test](https://img.shields.io/github/actions/workflow/status/yunline/pywintray/build_and_test.yml?label=build%20%26%20test)](https://github.com/yunline/pywintray/actions/workflows/build_and_test.yml)
[![](https://img.shields.io/website?url=https%3A%2F%2Fyunline.github.io/pywintray&label=docs&up_message=online)](https://yunline.github.io/pywintray)
[![GitHub License](https://img.shields.io/github/license/yunline/pywintray)](LICENSE)


A lightweight Python library creating tray icon and menus on Windows.

This library:
- is 100% implemented in C – final binary is under 32 kB
- does not require any heavyweight image library like PIL
- is fully **type annotated** for modern Python development workflows
- supports **no-gil** in python 3.13+
- supports **Windows ARM64** platform

## Usage

TBD

## Installation

This library requires python >= 3.10.  
To build from source, you also need MSVC compiler installed in your environment. 

Use following command to install pywintray:

```bat
git clone https://github.com/yunline/pywintray.git
cd pywintray
pip install .
```
