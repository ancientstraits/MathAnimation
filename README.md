# Math Anim

This is a quick and dirty math animation library I am creating for personal use. It should work on any Windows machine, but I am not testing it on any other platforms as of right now. It's main purpose will be to quickly and easily render mathematical animations in real time as well as render mathematical animations to video files.

## Compiling on Windows

First clone the repository and the submodules by running:

```batch
git clone --recursive https://github.com/ambrosiogabe/MathAnimation
```

### First Time Setup

These instructions only need to be followed the first time you ever compile this library.

<details open>

<summary>
Click to See First Time Setup Instructions
</summary>

 First we need to setup ffmpeg.

I'm only writing instructions for Windows and MSVC. For information on compiling ffmpeg in a different environment, please see [ffmpeg documentation](https://ffmpeg.org/platform.html#Windows) for further details and make the appropriate changes.

Unfortunately, ffmpeg is a particularly wild beast, so compiling is non-trivial.

#### Setting up Environment for ffmpeg

(_The following instructions are modified from [ffmpeg documentation](https://ffmpeg.org/platform.html#Windows)_)

* [MSYS2](https://www.msys2.org)
* [NASM](https://www.msys2.org)

Next, make sure that the following are completed.

1. Place `nasm.exe` in your `PATH`.
2. To set up a proper environment in MSYS2, you need to run `msys_shell.bat` from the Visual Studio or Intel Compiler command prompt. To do this:
    * First type in `Developer Command Prompt for VS` in your windows search bar.
    * Run the command prompt.
    * Change directories to where you installed msys2.
        * The default dir for me is `cd C:\tools\msys64`
    * Run `msys2_shell.cmd -use-full-path` to launch msys2.
3. Make sure `cl` works. Running `cl` should print something starting with: `Microsoft (R) C/C++...`
4. Make sure `NASM` is available. Running `nasm -v` should print the version.
5. Change into the directory where you have this repo installed.
6. Finally, to compile ffmpeg, run:

```bash
# NOTE This will take quite some time to compile
pushd ./Animations/vendor/ffmpeg
./configure \
    --toolchain=msvc \
    --prefix=./build \
    --disable-doc \
    --arch=x86_64 \
    --disable-x86asm 
make 
make install

# Rename the files to .lib extension to make premake happy
mv ./build/lib/libavcodec.a ./build/lib/libavcodec.lib
mv ./build/lib/libavdevice.a ./build/lib/libavdevice.lib
mv ./build/lib/libavfilter.a ./build/lib/libavfilter.lib
mv ./build/lib/libavformat.a ./build/lib/libavformat.lib
mv ./build/lib/libavutil.a ./build/lib/libavutil.lib
mv ./build/lib/libswresample.a ./build/lib/libswresample.lib
mv ./build/lib/libswscale.a ./build/lib/libswscale.lib
popd
```

7. Verify that you compiled everything correctly. There should be a file named `build` in the current directory `./Animations/vendor/ffmpeg/build`. Inside this file you should see several directories with and a `lib` folder with the ffmpeg binaries.
    * If this is correct, then you're done compiling ffmpeg.

#### Setting up Environment for freetype

Thankfully, freetype is much simpler to set up than ffmpeg. To compile on windows, I'll be using cmake and MSVC. You can change use a different build system if you like, just ensure that at the end you have two directories for a release and debug version of freetype at the locations:

```bash
./Animations/vendor/freetype/build/Debug/freetyped.lib
./Animations/vendor/freetype/build/Release/freetype.lib
```

To build with CMake and MSVC:

1. Open up a command prompt for MSVC.
2. Change into your local directory for this animations library.
3. Run the following commands to compile freetype:

```batch
pushd .\Animations\vendor\freetype
mkdir build
pushd build
cmake ..
msbuild freetype.sln /property:Configuration=Debug
msbuild freetype.sln /property:Configuration=Release
popd
popd
```

4. If this all succeeds, you should see a build directory in `./Animations/vendor/freetype` that contains a Debug directory and Release directory with the appropriate DLLs.

</details>

### Compiling

_NOTE:_ Make sure that you have completed the first time setup instructions if this is your first time compiling this project. Click the dropdown above to get the full instructions.

Some more stuff...

## Current Features

Right now this library can

* Animate any parametric equation over time
* Draw a grid of arbitrary width
* Draw basic lines

## Planned Features

This library is planned to support

[ ] Export arbitrary video formats (currently only H264 MP4 is supported)
[ ] View audio waveform preview
  [ ] View an audio wave form at the bottom of the timeline editor(and hear the audio as the clip is played) to sync
    animations up with an audio clip
[ ] 3D Lines and Bezier curve approximations
[ ] 3D animation objects
[ ] Gizmos
[ ] Textured 2D objects
[ ] Textured 3D objects
[ ] Animatable Camera movements
[ ] Scriptable Animations
[ ] Multiple Animations on one animation segment
> I may need to rethink the entire architecture here in order to get this to work
[ ] Rethink the architecture
> Right now the way rendering works is I go through each animation up until the current
  frame. If the animation was already completed, I "apply" the animation to the object.
  If an animation is in progress at the current frame, I send an interpolated t value to
  the animation, and it figures out how to render itself. This means that if your at frame
  60,000 in a video, then it must loop through all the previous objects and apply the animations
  before it can render this particular frame. This may or may not be a performance issue, it   remains to be seen
[ ] Export/Import custom scenes (the plumbing is all there, I just need a file picker UI)
[ ] Project hub when starting the app
> Kind of like DaVinci Resolve's startup project display. This way you can just click into the most recent
  project and create a new one without having to do anything complicated
[ ] Font previews when selecting a font
[ ] Animation preview when hovering over a question mark

## Bugs

* If you leave it running for awhile, you can't click on segments
    * I deleted a segment and a subsegment and that's when it started
