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
#include "audio2/Converter.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"
#include "audio2/CinderAssert.h"

#include "cinder/Utilities.h"

using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Node
// ----------------------------------------------------------------------------------------------------

Node::Node( const Format &format )
: mInitialized( false ), mEnabled( false ),	mChannelMode( format.getChannelMode() ),
mNumChannels( 1 ), mInputs( 1 ), mAutoEnabled( false ), mProcessInPlace( true )
{
	if( format.getChannels() ) {
		mNumChannels = format.getChannels();
		mChannelMode = ChannelMode::SPECIFIED;
	}

	if( ! boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled( format.getAutoEnable() );
}

Node::~Node()
{
}

const NodeRef& Node::connect( const NodeRef &dest )
{
	dest->setInput( shared_from_this() );
	return dest;
}

const NodeRef& Node::connect( const NodeRef &dest, size_t bus )
{
	dest->setInput( shared_from_this(), bus );
	return dest;
}

void Node::disconnect( size_t bus )
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	for( NodeRef &input : mInputs )
		input.reset();

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

void Node::setInput( const NodeRef &input )
{
	if( ! checkInput( input ) )
		return;

	{
		lock_guard<mutex> lock( getContext()->getMutex() );

		input->setOutput( shared_from_this() );

		// find first available slot
		bool didSlot = false;
		for( size_t i = 0; i < mInputs.size(); i++ ) {
			if( ! mInputs[i] ) {
				mInputs[i] = input;
				didSlot = true;
				break;
			}
		}

		if( ! didSlot )
			mInputs.push_back( input );

		configureConnections();
	}

	// must call once lock has been released
	getContext()->connectionsDidChange( shared_from_this() );
}

void Node::setInput( const NodeRef &input, size_t bus )
{
	if( ! checkInput( input ) )
		return;

	if( bus > mInputs.size() )
		throw AudioExc( string( "bus " ) + ci::toString( bus ) + " is out of range (max: " + ci::toString( mInputs.size() ) + ")" );


	// TODO: this disconnection kills nodes that are solely owned by the graph. but make sure not disconnecting works out.
	//NodeRef& existingInput = mInputs[bus];
	//if( existingInput )
	//	existingInput->disconnect();

	{
		lock_guard<mutex> lock( getContext()->getMutex() );

		mInputs[bus] = input;
		input->setOutput( shared_from_this() );
		configureConnections();
	}

	// must call once lock has been released
	getContext()->connectionsDidChange( shared_from_this() );
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
				submixBuffers( &mInternalBuffer, &mSummingBuffer );
			else
				submixBuffers( input->getInternalBuffer(), &mSummingBuffer );
		}

		if( mEnabled )
			process( &mSummingBuffer );

		outputBuffer->zero();
		submixBuffers( &mSummingBuffer, outputBuffer );
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

void Node::initializeImpl()
{
	if( mInitialized )
		return;

	initialize();
	mInitialized = true;
	LOG_V << getTag() << " initialized." << endl;

	if( mAutoEnabled )
		start();
}


void Node::uninitializeImpl()
{
	if( ! mInitialized )
		return;

	if( mAutoEnabled )
		stop();

	uninitialize();
	mInitialized = false;
	LOG_V << getTag() << " un-initialized." << endl;
}

void Node::setNumChannels( size_t numChannels )
{
	uninitializeImpl();
	mNumChannels = numChannels;
}

void Node::configureConnections()
{
	CI_ASSERT( getContext() );

	mProcessInPlace = true;

	if( getNumInputs() > 1 )
		mProcessInPlace = false;

	for( auto &input : mInputs ) {
		if( ! input )
			continue;

		if( input->getNumChannels() != mNumChannels ) {
			if( mChannelMode == ChannelMode::MATCHES_INPUT ) {
				// FIXME: use max channels of all inputs
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

		input->initializeImpl();
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

	initializeImpl();
}

// TODO: reallocations could be made more efficient by using DynamicBuffer
void Node::setProcessWithSumming()
{
	CI_ASSERT( getContext() );

	mProcessInPlace = false;
	size_t framesPerBlock = getContext()->getFramesPerBlock();

	if( mSummingBuffer.getNumChannels() == mNumChannels && mSummingBuffer.getNumFrames() == framesPerBlock )
		return;

	mSummingBuffer = Buffer( framesPerBlock, mNumChannels );
	mInternalBuffer = Buffer( framesPerBlock, mNumChannels );
}

void Node::submixBuffers( const Buffer *sourceBuffer, Buffer *destBuffer )
{
	Converter::submixBuffers( sourceBuffer, destBuffer );
}

bool Node::checkInput( const NodeRef &input )
{
	return ( input && ( input != shared_from_this() ) && ! isConnectedToInput( input ) );
}

} } // namespace cinder::audio2
