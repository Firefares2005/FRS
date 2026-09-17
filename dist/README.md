# FRS Library

## Installation
Extract this folder to C:\frs\

## Usage
#include <frs.h>
using namespace frs;

int main() {
    compress("photo.jpg", "photo.frs", 85);
    decompress("photo.frs", "restored.png");
    return 0;
}

## Build
g++ my_app.cpp -Idist\include -Ldist\lib -lfrs -o my_app.exe