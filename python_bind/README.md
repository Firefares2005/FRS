# frs-codec

Python bindings for the FRS image compression library.

## Install

    pip install frs-codec

## Usage

    import frs_python

    frs_python.compress("photo.jpg", "photo.frs", 85)
    frs_python.decompress("photo.frs", "restored.png")
