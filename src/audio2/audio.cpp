/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "audio2/audio.h"
#include "audio2/Device.h"

#include "cinder/app/App.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

namespace {
	void printRecursive( NodeRef node, size_t depth )
	{
		if( ! node )
			return;
		for( size_t i = 0; i < depth; i++ )
			app::console() << "-- ";

		string channelMode;
		switch( node->getChannelMode() ) {
			case Node::ChannelMode::SPECIFIED: channelMode = "specified"; break;
			case Node::ChannelMode::MATCHES_INPUT: channelMode = "matches input"; break;
			case Node::ChannelMode::MATCHES_OUTPUT: channelMode = "matches output"; break;
		}

		app::console() << node->getTag() << "\t[ " << ( node->isEnabled() ? "enabled" : "disabled" );
		app::console() << ", ch: " << node->getNumChannels();
		app::console() << ", ch mode: " << channelMode;
		app::console() << ", " << ( node->getProcessInPlace() ? "in-place" : "sum" );
		app::console() << " ]" << endl;

		for( auto &input : node->getInputs() )
			printRecursive( input, depth + 1 );
	};
}

void printGraph( ContextRef graph )
{
	app::console() << "-------------- Graph configuration: --------------" << endl;
	printRecursive( graph->getTarget(), 0 );
}

void printDevices()
{
	for( auto &device : Device::getDevices() ) {
		app::console() << "-- " << device->getName() << " --" << endl;
		app::console() << "\t key: " << device->getKey() << endl;
		app::console() << "\t inputs: " << device->getNumInputChannels() << ", outputs: " << device->getNumOutputChannels() << endl;
		app::console() << "\t samplerate: " << device->getSampleRate() << ", frames per block: " << device->getFramesPerBlock() << endl;
	}
}

} } // namespace cinder::audio2