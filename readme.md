# Cinder-Audio2

This is the development repo for [Cinder][cinder]'s next audio API. It is available here as a cinderblock for alpha testing and feedback. It will be merged into the master repo closer to the next release.

## Features


*initial release:*

- A flexible and comfortable system for modular audio processing.  We want people to explore what's capable.
- a 'simple api', that lets users easily play audio.
- Wide device and file support
- Built-in support for audio-rate parameter ramping and manipulation.
- a library of DSP tools such as FFT, samplerate conversion, and vector operations.
- Probably goes without saying, but we want this system to be fast.
- For initial release, support for Mac, iOS, Windows 7, 8, and minimal support for XP

*delayed until after initial release:*

- High level constructs for game and 3d audio, ex. voice management.
- an offline audio processing graph
- support for sub-graph processing, such as upsampling or in the spectral domain
- WinRT support

## Design

The core of the design draws from concepts found in other popular modular audio API's, namely [Web Audio][webaudio] and [Pure Data][puredata], however aims to be closely integrated into cinder's existing design patterns. We also take full advantage of C++11 features such as smart pointers, `std::atomic`'s, and `std::mutex`'s.

A modular api is advantagous because it is proven to be very flexible and allows for reusability without loss in performance.  Still, higher level constructs exist and more will be added as time permits.  The cinder philosophy remains, "easy things easy and hard things possible." 

## Required Cinder Version:
cinder from github, **[dev branch](https://github.com/cinder/Cinder/tree/dev) or newer**

As this code is aimed at making it into the next cinder release (1.56), it is not tested against 1.55. While it may or may not work there for you, I can't spend the time to ensure it does. Look forward!


## Building

The code is currently wrapped up as a cinderblock for easy testing, so the fastest way to get up and running is to clone it to your cinder/blocks path and use [Tinderbox][tinderbox].

However, **you must still build the audio2 static library**, which can be done by opening up your platform's IDE project file (xcode/Audio2.xcodeproject on mac, vc2012/Audio2.sln on windows) and building in the same manner that you would cinder from source. Organizing the code into a static library means that as updates are made and file names are changed, your project files don't need to be updated as well.

Another option is to link to the Audio2.xcodeproj or Audio2.sln as a project dependency, and add an include path for `$(AUDIO2_PATH)/src`.  This is how the tests are organized.

**iOS Only:**

I've only tested on the device, not simulator.  The iOS simulator has many problems related to audio, limiting its usefulness for testing this code.  Instead, build for mac desktop when testing.

**Windows 8 Only: Building for XAudio2.8**

* delete the "$(DXSDK_DIR)\include" include paths entry from the Audio2.sln
* set the value of `_WIN32_WINNT` to equal win 8 by opening up the Audio2.sln's property sheet and changing `AUDIO2_DEPLOYMENT_TARGET` to 0x0602.

## Viewing the Tests

There are currently only a small handful of samples that are meant to get new users up and running in the samples folder.  To see more extensive usage of the various components, open up one of the test workspaces:

- mac: test/Audio2Test.xcworkspace
- windows: test/Audio2Test.msw/Audio2Test.sln

These are meant to be more for feature and regression testing than anything else, but at the moment they are also the most useful way to see the entire breadth of the available functionality.


## Usage

#### Voice API

The Voice class allows users to easily perform common audio tasks, like playing a sound file. For example:

```cpp
mVoice = audio2::Voice::create( audio2::load( loadAsset( "soundfile.wav"  ) ) );
mVoice->play();
```

Common tasks like play(), pause(), and stop() are supported. Each Voice has controls for volume and 2d panning. For more information, see the samples [VoiceBasic](samples/VoiceBasic/src/VoiceBasicApp.cpp) and [VoiceBasicProcessing](samples/VoiceBasicProcessing/src/VoiceBasicProcessingApp.cpp).

The Voice API sits above and ties into the modular API, wihch is explained below. Each Voice has a virtual `getNode()` member function that gives access to more advanced functionality.

#### Modular API

##### Context

The Context class manages platform specific audio processing and thread synchronization between the 'audio' (real-time) and 'user' (typically UI/main, but not limited to) threads. There is one 'master', which is the only hardware-facing Context. All Node's are created using the Context, which is necessary for thread safety:

```cpp
auto ctx = audio2::Context::master();
mNode = ctx->makeNode( new NodeType );
```

##### Node

A Node is the fundamental building block for audio processing graphs. They allow for flexible combinations of synthesis, analysis, effects, file reading/writing, etc., and are meant to be easily subclassed. There are a three fundamental types of Node's:

* **NodeOutput**: an endpoint at the end of an audio graph. Has no outputs.
* **NodeInput**: an endpoint at the beginning of an audio graph. Has no inputs.
* **NodeEffect**: has both inputs and outputs.

Node's are connected together to from an audio graph. For audio to reach the speakers, the last Node in the graph is connected to the Context's NodeOutput:

```cpp
auto ctx = audio2::Context::master();
mSine = ctx->makeNode( new audio2::GenSine );
mGain = ctx->makeNode( new audio2::Gain );

mSine->connect( mGain );
mGain->connect( ctx->getOutput() );
```

Node's are connected from source to destination. A convenient shorthand syntax that is meant to represent this is as follows:

```cpp
mSine >> mGain >> ctx->getOutput();
```


To process audio, each Node subclass implements a virtual method `process( Buffer *buffer )`. Processing can be enabled/disabled on a per-Node basis. While `NodeEffect`s are enabled by default, `NodeInput`s must be turned on before they produce any audio. `NodeOutput`s are managed by their owning `Context`, which has a similar enabled/disabled syntax:

```cpp
mSine->start();
ctx->start();
```

It is important to note, however, that turning that enabling/disabling the Context effects the enabled state of the entire audio graph - no Node will process audio if it is off, which is useful catch-all way to shut off the audio processing thread.

The reason why the above is true is that, although Node's are (by convention) connected source >> destination, the actual processing follows the 'pull model', i.e. destination (recursively) pulls the source.  This is achieved in Node's` pullInputs( Buffer *destBuffer)`, although the method is virtual and may be customized by sublasses.

Other Node features include:

* can be enabled / disabled / connected / disconnected while audio is playing
* supports multiple inputs, which are implicitly summed to their specified number of channels.
* supports multiple outputs, which don't necessarily have to be connected to the Context's output( they will be added to the 'auto pull list').
* If possible (ex. one input, same # channels), a Node will process audio in-place
* Node::ChannelMode allows the channels to be decided based on either a Node's input, it's output, or specified by user.

See:

* samples: [NodeBasic](samples/NodeBasic/src/NodeBasicApp.cpp), [NodeAdvanced](samples/NodeAdvanced/src/NodeAdvancedApp.cpp)
* tests: [NodeTest](test/NodeTest/src/NodeTestApp.cpp), [NodeEffectTest](test/NodeEffectTest/src/NodeEffectTestApp.cpp)

####  Device Input and Output

FINISH ME

Allows for choosing either default input or output devices, or choosing device by name or key

Supports audio interfaces with an arbitrary number of channels

See:

* [DeviceTest](test/DeviceTest/src/DeviceTestApp.cpp)

#### Reading Audio Files

Audio files are represented by the `audio2::SourceFile` class. The main interface for audio file playback is SamplePlayer, which is abstract and comes in two concrete flavors:

- **BufferPlayer**: plays back audio from an `audio2::Buffer`, i.e. in-memory. The Buffer is loaded from a `SourceFile` in one shot and there is no file i/o during playback.
- **FilePlayer**: streams the audio playback via file. 

```cpp
// load a sample
mSourceFile = audio2::load( loadAsset( "audiofile.wav" ) );

// create an load a BufferPlayer
auto ctx = audio2::Context::master();
mBufferPlayer = ctx->makeNode( new audio2::BufferPlayer() );
mBufferPlayer->loadBuffer( mSourceFile );

// or create a FilePlayer, which takes a ref to the SourceFile at construction:
mFilePlayer = ctx->makeNode( new audio2::FilePlayer( mSourceFile ) );
```

Both support reading of file types async; `BufferPlayer::loadBuffer` can be done on a background thread, and FilePlayer can be specified as reading from a background thread during construction. 

Supported File types:

- For mac, see file types [listed here][coreaudio-file-types].
- For windows, see file types [listed here][mediafoundation-file-types]. 
- supported ogg vorbis on all platforms.

See:

* [SamplePlayerTest](test/SamplePlayerTest/src/SamplePlayerTestApp.cpp)

#### Writing Audio Files

TODO (not finished implementing)

#### Viewing audio using Scope and ScopeSpectral

TODO

See:

* sample using `Scope`: [NodeAdvanced](samples/NodeAdvanced/src/NodeAdvancedApp.cpp)
* test using `ScopeSpectral`: [SpectralTest](test/SpectralTest/src/SpectralTestApp.cpp)

## Try It

Currently the best way to try this stuff out is to open up _test/Audio2Test.xcworkspace_ in Xcode or _test/Audio2Test.msw/Audio2Test.sln_ in Visual Studio 2012. In there you'll find the Audio2 project corresponding to your platform, along with a set of admittingly boring test apps that excercise the current functionality.

Be warned, there will be ways to crash your app, possibly ending in disturbance to your neighbors, and if this does happen to you then please make an issue with reproduceable steps, hopefully starting with one of the included tests.

__note on cinder location in tests:__ cinder is expected to be at _$(AUDIO2_PATH)/cinder_ in the test projects, I use a symlink for this.

__note on non-working tests:__

- sound file playback is broken on MSW at this time (but not for long!)
- MixerTest is up in limbo, since `Node`'s can implicitly sum multiple inputs. 

## Third Party Libraries

There are a few libraries written by third parties, all redistributed in source form and liberally licensed:

* [ooura] general purpose FFT algorithms.
* [r8brain] sample rate converter library, designed by Aleksey Vaneev of Voxengo.
* [oggvorbis] audio decoder / encoder for the ogg file format.

To all of the people responsible for making these available and of such high quality, thank you!

## Feedback

Please provide any feedback that you feel is relevant by either creating github issues, commenting in line on github, or posting to Cinder's [dev forum][dev-forum].

Cheers,

Rich


[cinder]: https://github.com/cinder/cinder
[tinderbox]: http://libcinder.org/docs/welcome/TinderBox.html
[dev-forum]: https://forum.libcinder.org/#Forum/developing-cinder
[webaudio]: https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html
[puredata]: http://puredata.info/
[coreaudio-file-types]: https://developer.apple.com/library/ios/documentation/MusicAudio/Conceptual/CoreAudioOverview/SupportedAudioFormatsMacOSX/SupportedAudioFormatsMacOSX.html
[mediafoundation-file-types]: http://msdn.microsoft.com/en-us/library/windows/desktop/dd757927(v=vs.85).aspx
[ooura]: http://www.kurims.kyoto-u.ac.jp/~ooura/fft.html
[r8brain]: https://code.google.com/p/r8brain-free-src/
[oggvorbis]: http://xiph.org/vorbis/
