# Cinder-Audio2

WOP repo for [Cinder][1]'s next audio API.



## Goals


## Design


## Usage (graph)

#### Controlling a Node's processing:
 A Node must be enabled in order for it to process audio.  This is done by calling `Node::start()` or `Node::setEnabled( true )`. For convenience, you can also call `Node::setAudioEnabled( true )`, and the Node will be enabled/disabled when you call `Context::start()` or `Context::stop()`.  Some of the Node's have this setting on by default, such as `NodeEffect` subclasses.


## Try It

Currently the best way to try this stuff out is to open up _test/Audio2Test.xcworkspace_ in Xcode or _test/Audio2Test.msw/Audio2Test.sln_ in Visual Studio 2012. In there you'll find the Audio2 project corresponding to your platform, along with a set of admittingly boring test apps that excercise the current functionality.

Be warned, there will be ways to crash your app, possibly ending in disturbance to your neighbors, and if this does happen to you then please make an issue with reproduceable steps, hopefully starting with one of the included tests.

__note on non-working tests:__

- sound file playback is broken on MSW at this time (but not for long!)
- MixerTest is up in limbo, since `Node`'s can implicitly sum multiple inputs. 


#### Building and Using in Your Project

The main reasons for separating this code out into its own repo and placing it in the ci::audio2 namespace is so that you can try it in your own projects, with whatever version of cinder you are currently using.  However, since we make use of many C++11 features, you'll need at least cinder 0.8.5 and, on Windows, you'll need Visual Studio 2012.


## Feedback

Please provide any feedback that you feel is relevant by either creating issues, commenting in line on github, or posting to Cinder's [dev forum][2]. Thanks!

Rich


[1]: https://github.com/cinder/cinder
[2]: https://forum.libcinder.org/#Forum/developing-cinder
