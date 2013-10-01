# Cinder-Audio2

WIP repo for [Cinder][1]'s next audio API.



## Goals

- A flexible and comfortable system for modular audio processing.  We want people to explore what's capable.
- Wide device and file support
- a 'simple api', that lets users easily play audio.
- High level constructs for game and 3d audio, ex. voice management.
- Probably goes without saying, but we want this system to be fast.
- For initiial release, support for Mac, iOS, Windows 7, 8 and WinRT, along with minimal support for XP.

 

## Design

#### Node features

* Every node has a virtual method `process( Buffer *buffer )`, which is called from the audio graph if `isEnabled() == true`.
* a Node can be  enabled / disabled / connected / disconnected while audio is playing
* Node's can have multiple inputs and they will sum to their specified number of channels.
* If possible (ex. one input, same num channels), a Node will process audio in-place
* Node::ChannelMode allows the channels to be decided based on either a Node's input, it's output, or specified by user.



## Usage (graph)

FINISH ME

#### Controlling a Node's processing:
 A Node must be enabled in order for it to process audio.  This is done by calling `Node::start()` or `Node::setEnabled( true )`. For convenience, you can also call `Node::setAudioEnabled( true )`, and the Node will be enabled/disabled when you call `Context::start()` or `Context::stop()`.  Some of the Node's have this setting on by default, such as `NodeEffect` subclasses.


## Try It

Currently the best way to try this stuff out is to open up _test/Audio2Test.xcworkspace_ in Xcode or _test/Audio2Test.msw/Audio2Test.sln_ in Visual Studio 2012. In there you'll find the Audio2 project corresponding to your platform, along with a set of admittingly boring test apps that excercise the current functionality.

Be warned, there will be ways to crash your app, possibly ending in disturbance to your neighbors, and if this does happen to you then please make an issue with reproduceable steps, hopefully starting with one of the included tests.

__note on cinder location in tests:__ cinder is expected to be at _$(AUDIO2_PATH)/cinder_ in the test projects, I use a symlink for this.

__note on non-working tests:__

- sound file playback is broken on MSW at this time (but not for long!)
- MixerTest is up in limbo, since `Node`'s can implicitly sum multiple inputs. 


#### Building and Using in Your Project

The main reasons for separating this code out into its own repo and placing it in the ci::audio2 namespace is so that you can try it in your own projects, with whatever version of cinder you are currently using.  However, since we make use of many C++11 features, you'll need at least cinder 0.8.5 and, on Windows, you'll need Visual Studio 2012.

There are two ways to include ci::audio2 in your project:

- put this repo in _$(CINDER_PATH)/blocks/_ and use tinderbox to add the Audio2 cinderblock.
- link to the Audio2.xcodeproj or Audio2.sln as a dependency and add an include path for _$(AUDIO2_PATH)/src_.  This is how I have organized all of the tests / samples as it means I am always up-to-date with the current source files. 

## Feedback

Please provide any feedback that you feel is relevant by either creating issues, commenting in line on github, or posting to Cinder's [dev forum][2]. Thanks!

Rich


[1]: https://github.com/cinder/cinder
[2]: https://forum.libcinder.org/#Forum/developing-cinder
