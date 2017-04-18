#ADSM-CEngine (Animal Disease Spread Model-CEngine)
ADSM is an application with Desktop and Web Based GUI for creating a simulation model to assist decision making and education in evaluating animal disease incursions.
This CEngine is the C code that runs the stochastic modeling simulation in the background.
Scenario parameters are sent to the CEngine via a Scenario SQLite file.
Results are written directly back to the same SQLite file, and supplemental result files are written to a directory alongside the SQLite scenario file.

##Compile CEngine Executable
The CEngine needs to be compiled for testing and deployment both.

Windows is the primary target currently as the ADSM GUI application is mostly used on Windows.

Compiling on Linux isn't fully supported yet for two reasons:

1. The compile output of the CEngine compile process isn't yet deploy friendly. It isn't statically compiled and as such has a lot of dependent files that aren't easily distributed.
2. The ADSM GUI application compile process requires manual intervention on Linux to get the output files collected into the proper folders for the executable to find them. This second reason doesn't matter when you are talking about doing a deployment on a Web Server as the ADMS GUI application does not need to be compiled for that sort of deployment.

###x86-64 Windows 7 - 10
Exactly follow these steps to compile the CEngine Executable for Windows.

1. Download and install msys2: http://repo.msys2.org/distrib/x86_64/msys2-x86_64-20161025.exe  
   **NOTE:** the home direcotry when in the msys shell is shows as '/home/username'; this directory is located at 'C:\msys2\home\username'
1. Open the msys terminal and run `pacman -Syu`
1. Close the terminal and reopen it
1. `pacman -Su`  
1. `pacman -S pkg-config autoconf automake-wrapper gcc make bison python glib2-devel mingw-w64-x86_64-gd mingw-w64-x86_64-gsl libsqlite-devel mingw-w64-x86_64-shapelib mingw-w64-x86_64-json-glib`
1. Close the terminal
1. Download the General Polygon Clipper Library (GPC): http://www.cs.man.ac.uk/~toby/gpc/assets/gpc232-release.zip
1. Unpack gpc (easiest if you unpack into your msys home directory 'C:\msys2\home\username\') 
1. Open the msys terminal and cd into the unpacked gpc directory
1. `mkdir -p /usr/local/lib`  
1. `mkdir -p /usr/local/include/gpcl/`  
1. `mkdir -p /share/aclocal/`   
1. `mkdir -p /mingw64/share/aclocal`  
1. `cc -c -o gpc.o gpc.c`  
1. `ar r libgpcl.a gpc.o`  
1. `cp libgpcl.a /usr/local/lib/`  
1. `cp gpc.h /usr/local/include/`   
1. `cp gpc.h /usr/local/include/gpcl/`
1. In your favorite text editor, add the following lines to '~/.bash_profile':
   ```bash
   export PATH=$PATH:/mingw64/bin  
   export CFLAGS='-I/usr/local/include -I/mingw64/include'  
   export LDFLAGS='-L/usr/local/lib -L/usr/lib -L/mingw64/lib'  
   export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/mingw64/lib/pkgconfig
   
   ```
1. Close the msys terminal and reopen it
1. cd into the CEngine directory.
1. `sh bootstrap`  
1. `./configure --disable-debug`  
1. `make`  
   * The make command will build the ADSM executables but then stop on an error when it reaches the doc/diagrams directory (the free diagramming tool Dia is not available as a precompiled MSYS2 package)
   * The compiled output will be './main_loop/adsm.exe'
   * **NOTE:** You won't be able to run the output in place as it needs to be in the same directory as some dependent dll files (which are in the main ADSM repo already).
1. Take the './main_loop/adsm.exe' file and put it into ADSM GUI application in 'ADSM/bin/' and rename it to 'adsm_simulation.exe'.
1. If once in the 'ADSM/bin' folder it won't run due to a missing dependency, look for the proper dll in your msys installation folders and move it to be alongside the 'adsm_simulation.exe' file in the 'ADSM/bin' folder

###x86-64 Debian Linux (Ubuntu preferred)
Follow these steps to compile the CEngine Executable for Linux.

1. Open a terminal
1. `sudo su`
1. `apt-get update`
1. `apt-get upgrade`
1. `apt-get install make gcc pkg-config autoconf automake bison python libglib2-dev libgd3 libgd-dev libgsl2 libgsl-dev sqlite3 libsqlite3-dev shapelib libshp-dev libjson-glib-1.0-0 proj-bin libproj-dev flex`
1. `wget http://www.cs.man.ac.uk/~toby/gpc/assets/gpc232-release.zip`
1. (unzip gpc)
1. `cd gpc232-release`
1. `mkdir -p /usr/local/lib`  
1. `mkdir -p /usr/local/include/gpcl/`  
1. `mkdir -p /share/aclocal/`
1. `cc -c -o gpc.o gpc.c`  
1. `ar r libgpcl.a gpc.o`  
1. `cp libgpcl.a /usr/local/lib/`  
1. `cp gpc.h /usr/local/include/`  
1. `cp gpc.h /usr/local/include/gpcl/` 
1. `export CFLAGS='-I/usr/local/include'`
1. `export LDFLAGS='-L/usr/local/lib -L/usr/lib'`
1. `cd /path/to/ADSM-CEngine`
1. In your favorite text editor, modify './bootstrap' to comment out the Windows aclocal line and uncomment the Linux aclocal line
1. `sh bootstrap`
1. `./configure --disable-debug`
1. Change the `python` link to point to `python3` (this is a temp fix)
   1. `mv /usr/bin/python /usr/bin/python-back`
   1. `ln -s /usr/bin/python3 /usr/bin/python`
1. `make`
   * The make command will build the ADSM executables but then stop on an error when it reaches the doc/diagrams directory (compiling the free diagramming tool Dia is not covered here)
   * The compiled output will be './main_loop/adsm'
1. Undo your python change
   1. `rm /usr/bin/python`
   1. `mv /usr/bin/python-back /usr/bin/python`
1. Take the './main_loop/adsm' file and put it into ADSM GUI application in 'ADSM/bin/' and rename it to 'adsm_simulation'.
1. **WARNING** The 'adsm_simulation' will properly run on your system once in 'ADSM/bin/'.  
   HOWEVER! It will NOT run on another system that doesn't have the proper dependencies installed already (in exactly the correct location).  
   This is due to this compilation process not being statically linked and is the reason why distribution on Linux is not yet supported.
