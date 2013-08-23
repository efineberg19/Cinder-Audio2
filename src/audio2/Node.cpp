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


#include "audio2/Node.h"
#include "audio2/audio.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"
#include "audio2/CinderAssert.h"

#include "cinder/Utilities.h"

using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Node
// ----------------------------------------------------------------------------------------------------

// TODO: seems fitting to pass in Context* as first argument to all Node's
// - setting during init no longer necessary
// - provides samplerate / num frames at init
// - also a garunteed object to syncrhonize with

Node::Node( const Format &format )
: mInitialized( false ), mEnabled( false ),	mChannelMode( format.getChannelMode() ),
mNumChannels( 1 ), mBufferLayout( Buffer::Layout::CONTIGUOUS ), mAutoEnabled( false ), mProcessInPlace( true )
{
	if( format.getChannels() ) {
		mNumChannels = format.getChannels();
		mChannelMode = ChannelMode::SPECIFIED;
	}
}

Node::~Node()
{

}

void Node::initialize()
{
	mInitialized = true;
}

NodeRef Node::connect( NodeRef dest )
{
	dest->setInput( shared_from_this() );
	return dest;
}

NodeRef Node::connect( NodeRef dest, size_t bus )
{
	dest->setInput( shared_from_this(), bus );
	return dest;
}

// TODO: need 2 variants
// - if multi-output is supported, use getOutput( bus )->getInputs()
void Node::disconnect( size_t bus )
{
	stop();

	mInputs.clear();

	auto output = getOutput();
	if( output ) {
		// note: the output must be reset before resetting the output's reference to this Node,
		// since that may cause this Node to be deallocated.
		mOutput.reset();
		
		auto& parentInputs = output->getInputs();
		for( size_t i = 0; i < parentInputs.size(); i++ ) {
			if( parentInputs[i] == shared_from_this() )
				parentInputs[i].reset();
		}
	}
}

void Node::setInput( NodeRef input )
{
	if( ! checkInput( input ) )
		return;

	input->setOutput( shared_from_this() );

	// find first available slot
	for( size_t i = 0; i < mInputs.size(); i++ ) {
		if( ! mInputs[i] ) {
			mInputs[i] = input;
			configureConnections();
			return;
		}
	}

	// or append
	mInputs.push_back( input );
	configureConnections();
}


// TODO: figure out how to best handle node replacements
void Node::setInput( NodeRef input, size_t bus )
{
	if( ! checkInput( input ) )
		return;

	if( bus > mInputs.size() )
		throw AudioExc( string( "bus " ) + ci::toString( bus ) + " is out of range (max: " + ci::toString( mInputs.size() ) + ")" );
	if( mInputs[bus] )
		throw AudioExc(  string( "bus " ) + ci::toString( bus ) + " is already in use. Replacing busses not yet supported." );

	mInputs[bus] = input;
	input->setOutput( shared_from_this() );
	configureConnections();
}

bool Node::isConnectedToInput( const NodeRef &input ) const
{
	return find( mInputs.begin(), mInputs.end(), input ) != mInputs.end();
}

bool Node::isConnectedToOutput( const NodeRef &output ) const
{
	return ( getOutput() == output );
}

void Node::setEnabled( bool enabled )
{
	if( enabled )
		start();
	else
		stop();
}

// FIXME: mProcessInPlace is not thread-safe
// - probably high-time to ditch atomic<bool> and go with std::mutex, since there are multiple pieces that need to be synchronized
void Node::pullInputs( Buffer *outputBuffer )
{
	CI_ASSERT( getContext() );
	
	if( mProcessInPlace ) {
		for( NodeRef &input : mInputs ) {
			if( ! input )
				continue;

			input->pullInputs( outputBuffer );
		}

		if( mEnabled )
			process( outputBuffer );

	}
	else {
		mInternalBuffer.zero();
		mSummingBuffer.zero();

		for( NodeRef &input : mInputs ) {
			if( ! input )
				continue;

			input->pullInputs( &mInternalBuffer );
			if( input->getProcessInPlace() )
				submixBuffers( &mSummingBuffer, &mInternalBuffer );
			else
				submixBuffers( &mSummingBuffer, input->getInternalBuffer() );
		}

		if( mEnabled )
			process( &mSummingBuffer );

		outputBuffer->zero();
		submixBuffers( outputBuffer, &mSummingBuffer );
	}
}

size_t Node::getNumInputs() const
{
	size_t result = 0;
	for( const auto &input : mInputs ) {
		if( input )
			result++;
	}

	return result;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Protected
// ----------------------------------------------------------------------------------------------------

void Node::setNumChannels( size_t numChannels )
{
	mNumChannels = numChannels;
}

// TODO: if matches input and multiple inputs, find max channels of all inputs
void Node::configureConnections()
{
	CI_ASSERT( getContext() );

	mProcessInPlace = true;

	if( getNumInputs() > 1 )
		mProcessInPlace = false;

	for( auto &input : mInputs ) {
		if( input && input->getNumChannels() != mNumChannels ) {
			if( mChannelMode == ChannelMode::MATCHES_INPUT ) {
				// TODO: figure out the best thing to do when we have multiple inputs.
				// - probably set num channels to equal the most channels any input has
				setNumChannels( input->getNumChannels() );
			}
			else if( input->getChannelMode() == ChannelMode::MATCHES_OUTPUT ) {
				input->setNumChannels( mNumChannels );
				input->configureConnections();
			}
			else {
				mProcessInPlace = false;
				input->setProcessWithSumming();
			}
		}
	}

	NodeRef output = getOutput();
	if( output && output->getNumChannels() != mNumChannels ) {
		if( output->getChannelMode() == ChannelMode::MATCHES_INPUT ) {
			output->setNumChannels( mNumChannels );
			output->configureConnections();
		}
		else
			mProcessInPlace = false;
	}

	if( ! mProcessInPlace )
		setProcessWithSumming();
}

// TODO: reallocations could be made more efficient by using DynamicBuffer
void Node::setProcessWithSumming()
{
	CI_ASSERT( getContext() );

	mProcessInPlace = false;
	size_t framesPerBlock = getContext()->getNumFramesPerBlock();

	if( mSummingBuffer.getNumChannels() == mNumChannels && mSummingBuffer.getNumFrames() == framesPerBlock )
		return;

	mSummingBuffer = Buffer( mNumChannels, framesPerBlock );
	mInternalBuffer = Buffer( mNumChannels, framesPerBlock );
}

// TODO: I need 2 of these, one for summing and one for copying
void Node::submixBuffers( Buffer *destBuffer, const Buffer *sourceBuffer )
{
	CI_ASSERT( destBuffer->getNumFrames() == sourceBuffer->getNumFrames() );
	CI_ASSERT( destBuffer->getLayout() == sourceBuffer->getLayout() ); // TODO: add support for layout conversions

	size_t destChannels = destBuffer->getNumChannels();
	size_t sourceChannels = sourceBuffer->getNumChannels();
	if( destChannels == sourceBuffer->getNumChannels() ) {
		for( size_t c = 0; c < destChannels; c++ )
			sum( destBuffer->getChannel( c ), sourceBuffer->getChannel( c ), destBuffer->getChannel( c ), destBuffer->getNumFrames() );
	}
	else if( sourceChannels == 1 ) {
		// up-mix mono sourceBuffer to destChannels
		for( size_t c = 0; c < destChannels; c++ )
			sum( destBuffer->getChannel( c ), sourceBuffer->getChannel( 0 ), destBuffer->getChannel( c ), destBuffer->getNumFrames() );
	}
	else if( destChannels == 1 ) {
		// down-mix mono destBuffer to sourceChannels
		// TODO: try equal power fading all channels to center
		for( size_t c = 0; c < sourceChannels; c++ )
			sum( destBuffer->getChannel( 0 ), sourceBuffer->getChannel( c ), destBuffer->getChannel( 0 ), destBuffer->getNumFrames() );
	}
	else
		CI_ASSERT( 0 && "unhandled" );
}

bool Node::checkInput( const NodeRef &input )
{
	return ( input && ( input != shared_from_this() ) && ! isConnectedToInput( input ) );
}


} } // namespace cinder::audio2
