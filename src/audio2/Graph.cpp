#include "audio2/Graph.h"
#include "audio2/assert.h"

namespace audio2 {

	void Node::render( BufferT *buffer )
	{
		// handling in Graph::renderCallback
		
//		if( ! mSources.empty() )
//			mSources[0]->render( buffer );
	}

	void Consumer::connect( NodeRef source )
	{
		if( mSources.empty() )
			mSources.resize( 1 );
		mSources[0] = source;
		mSources[0]->setParent( shared_from_this() );
	}

	void Effect::connect( NodeRef source )
	{
		if( mSources.empty() )
			mSources.resize( 1 );
		mSources[0] = source;
		mSources[0]->setParent( shared_from_this() );
	}

	void Mixer::connect( NodeRef source )
	{
		mSources.push_back( source );
		mSources.back()->setParent( shared_from_this() );
	}

	void Mixer::connect( NodeRef source, size_t bus )
	{
		// TODO: throw exception if bus count not enough, else set in sources. Blow away old node
	}

	Graph::~Graph()
	{
		uninitialize();
	}

	void Graph::initialize()
	{
	}

	void Graph::uninitialize()
	{
	}

	void Graph::start()
	{
		if( mRunning )
			return;
		CI_ASSERT( mOutput );
		mRunning = true;
		mOutput->start();
	}

	void Graph::stop()
	{
		if( ! mRunning )
			return;
		mRunning = false;
		mOutput->stop();
	}

} // namespace audio2