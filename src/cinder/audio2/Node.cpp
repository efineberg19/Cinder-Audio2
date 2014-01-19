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


#include "cinder/audio2/Node.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/dsp/Converter.h"
#include "cinder/audio2/Debug.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Utilities.h"

#include "cinder/Utilities.h"

#include <limits>

using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Node
// ----------------------------------------------------------------------------------------------------

Node::Node( const Format &format )
	: mInitialized( false ), mEnabled( false ),	mChannelMode( format.getChannelMode() ),
		mNumChannels( 1 ), mAutoEnabled( false ), mProcessInPlace( true ), mLastProcessedFrame( numeric_limits<uint64_t>::max() )
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

const NodeRef& Node::connect( const NodeRef &output, size_t outputBus, size_t inputBus )
{
	// make a reference to ourselves so that we aren't deallocated in the case of the last owner
	// disconnecting us, which we may need later anyway
	NodeRef thisRef = shared_from_this();

	if( ! output->canConnectToInput( thisRef ) )
		return output;

	auto currentOutIt = mOutputs.find( outputBus );
	if( currentOutIt != mOutputs.end() ) {
		NodeRef currentOutput = currentOutIt->second.lock();
		// in some cases, an output may have lost all references and is no longer valid, so it is safe to overwrite without disconnecting.
		if( currentOutput )
			currentOutput->disconnectInput( thisRef );
		else
			mOutputs.erase( currentOutIt );
	}

	mOutputs[outputBus] = output; // set output bus first, so that it is visible in configureConnections()
	output->connectInput( thisRef, inputBus );

	output->notifyConnectionsDidChange();
	return output;
}

const NodeRef& Node::addConnection( const NodeRef &output )
{
	return connect( output, getFirstAvailableOutputBus(), output->getFirstAvailableInputBus() );
}

void Node::disconnect( size_t outputBus )
{
	auto outIt = mOutputs.find( outputBus );
	if( outIt == mOutputs.end() )
		return;

	NodeRef output = outIt->second.lock();
	CI_ASSERT( output );

	mOutputs.erase( outIt );
	output->disconnectInput( shared_from_this() );
	output->notifyConnectionsDidChange();
}

void Node::disconnectAllOutputs()
{
	NodeRef thisRef = shared_from_this();
	for( auto &out : mOutputs )
		disconnect( out.first );
}

void Node::disconnectAllInputs()
{
	NodeRef thisRef = shared_from_this();
	for( auto &in : mInputs )
		in.second->disconnectOutput( thisRef );

	mInputs.clear();
	notifyConnectionsDidChange();
}

void Node::connectInput( const NodeRef &input, size_t bus )
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	mInputs[bus] = input;
	configureConnections();
}

void Node::disconnectInput( const NodeRef &input )
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	for( auto inIt = mInputs.begin(); inIt != mInputs.end(); ++inIt ) {
		if( inIt->second == input ) {
			mInputs.erase( inIt );
			break;
		}
	}
}

void Node::disconnectOutput( const NodeRef &output )
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	for( auto outIt = mOutputs.begin(); outIt != mOutputs.end(); ++outIt ) {
		if( outIt->second.lock() == output ) {
			mOutputs.erase( outIt );
			break;
		}
	}
}

void Node::pullInputs( Buffer *destBuffer )
{
	CI_ASSERT( getContext() );

	if( mProcessInPlace ) {
		for( auto &in : mInputs )
			in.second->pullInputs( destBuffer );

		if( mEnabled )
			process( destBuffer );
	}
	else {
		uint64_t numProcessedFrames = getContext()->getNumProcessedFrames();
		if( mLastProcessedFrame != numProcessedFrames ) {
			mLastProcessedFrame = numProcessedFrames;

			mInternalBuffer.zero();
			mSummingBuffer.zero();

			for( auto &in : mInputs ) {
				NodeRef &input = in.second;

				input->pullInputs( &mInternalBuffer );
				if( input->getProcessInPlace() )
					dsp::sumBuffers( &mInternalBuffer, &mSummingBuffer );
				else
					dsp::sumBuffers( input->getInternalBuffer(), &mSummingBuffer );
			}

			if( mEnabled )
				process( &mSummingBuffer );
		}

		dsp::mixBuffers( &mSummingBuffer, destBuffer );
	}
}

void Node::setEnabled( bool enabled )
{
	if( enabled )
		start();
	else
		stop();
}

size_t Node::getNumConnectedInputs() const
{
	return mInputs.size();
}

size_t Node::getNumConnectedOutputs() const
{
	return mOutputs.size();
}

void Node::initializeImpl()
{
	if( mInitialized )
		return;

	initialize();
	mInitialized = true;
//	LOG_V( getName() << " initialized." );

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
//	LOG_V( getName() << " un-initialized." );
}

void Node::setNumChannels( size_t numChannels )
{
	if( mNumChannels == numChannels )
		return;

	uninitializeImpl();
	mNumChannels = numChannels;
}

size_t Node::getMaxNumInputChannels() const
{
	size_t result = 0;
	for( auto &in : mInputs )
		result = max( result, in.second->getNumChannels() );

	return result;
}

void Node::configureConnections()
{
	CI_ASSERT( getContext() );

	mProcessInPlace = true;

	if( getNumConnectedInputs() > 1 || getNumConnectedOutputs() > 1 )
		mProcessInPlace = false;

	for( auto &in : mInputs ) {
		NodeRef input = in.second;

		size_t inputNumChannels = input->getNumChannels();
		if( ! supportsInputNumChannels( inputNumChannels ) ) {
			if( mChannelMode == ChannelMode::MATCHES_INPUT )
				setNumChannels( getMaxNumInputChannels() );
			else if( input->getChannelMode() == ChannelMode::MATCHES_OUTPUT ) {
				input->setNumChannels( mNumChannels );
				input->configureConnections();
			}
			else {
				mProcessInPlace = false;
				input->setupProcessWithSumming();
			}
		}

		// inputs with more than one output cannot process in-place, so for them to sum
		if( input->getProcessInPlace() && input->getNumConnectedOutputs() > 1 )
			input->setupProcessWithSumming();

		input->initializeImpl();
	}

	for( auto &out : mOutputs ) {
		NodeRef output = out.second.lock();
		CI_ASSERT( output );

		if( ! output->supportsInputNumChannels( mNumChannels ) ) {
			if( output->getChannelMode() == ChannelMode::MATCHES_INPUT ) {
				output->setNumChannels( mNumChannels );
				output->configureConnections();
			}
			else
				mProcessInPlace = false;
		}
	}

	if( ! mProcessInPlace )
		setupProcessWithSumming();

	initializeImpl();
}

void Node::setupProcessWithSumming()
{
	CI_ASSERT( getContext() );

	mProcessInPlace = false;
	size_t framesPerBlock = getContext()->getFramesPerBlock();

	mInternalBuffer.setSize( framesPerBlock, mNumChannels );
	mSummingBuffer.setSize( framesPerBlock, mNumChannels );
}

void Node::notifyConnectionsDidChange()
{
	getContext()->connectionsDidChange( shared_from_this() );
}

bool Node::canConnectToInput( const NodeRef &input )
{
	if( ! input || input == shared_from_this() )
		return false;

	for( const auto& in : mInputs )
		if( input == in.second )
			return false;

	return true;
}

size_t Node::getFirstAvailableOutputBus()
{
	size_t result = 0;
	for( const auto& output : mOutputs ) {
		if( output.first != result )
			break;

		result++;
	}

	return result;
}

size_t Node::getFirstAvailableInputBus()
{
	size_t result = 0;
	for( const auto& input : mInputs ) {
		if( input.first != result )
			break;

		result++;
	}

	return result;
}

std::string Node::getName()
{
	return demangledTypeName( typeid( *this ).name() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeAutoPullable
// ----------------------------------------------------------------------------------------------------

NodeAutoPullable::NodeAutoPullable( const Format &format )
	: Node( format ), mIsPulledByContext( false )
{
}

NodeAutoPullable::~NodeAutoPullable()
{
}

const NodeRef& NodeAutoPullable::connect( const NodeRef &output, size_t outputBus, size_t inputBus )
{
	Node::connect( output, outputBus, inputBus );
	updatePullMethod();
	return output;
}

void NodeAutoPullable::connectInput( const NodeRef &input, size_t bus )
{
	Node::connectInput( input, bus );
	updatePullMethod();
}

void NodeAutoPullable::disconnectInput( const NodeRef &input )
{
	Node::disconnectInput( input );
	updatePullMethod();
}

void NodeAutoPullable::updatePullMethod()
{
	bool hasOutputs = ! mOutputs.empty();
	if( ! hasOutputs && ! mIsPulledByContext ) {
		mIsPulledByContext = true;
		getContext()->addAutoPulledNode( shared_from_this() );
		LOG_V( "added " << getName() << " to auto-pull list" );
	}
	else if( hasOutputs && mIsPulledByContext ) {
		mIsPulledByContext = false;
		getContext()->removeAutoPulledNode( shared_from_this() );
		LOG_V( "removed " << getName() << " from auto-pull list" );
	}
}

} } // namespace cinder::audio2
