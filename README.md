# Holographic Fractals
Holographic fractals for Voxon volumetric display

## Building
VSCode has two tasks:
* **gcc.exe build active file** - Release build
* **gcc.exe build active file - debug, profiling** - Testing build, worse peformance, but debug and profiling info
    * To debug press ``F5`` in VSCode
    * To profile - run the exec, which will output ``gmon.out`` in the working directory and then run this command to generate ``analysis.txt`` file
        ```
        > gprof build\explorer.exe gmon.out > analysis.txt
        ```
