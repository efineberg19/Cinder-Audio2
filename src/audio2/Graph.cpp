#include "audio2/Graph.h"
#include "audio2/assert.h"

namespace audio2 {

	void Node::render( BufferT *buffer )
	{
		if( ! mSources.empty() )
			mSources[0]->render( buffer );
	}

	void Consumer::connect( NodeRef source )
	{
		if( mSources.empty() )
			mSources.resize( 1 );
		mSources[0] = source;
		mSources[0]->setParent( shared_from_this() );
	}

	Graph::~Graph()
	{
		uninitialize();
	}

	void Graph::initialize()
	{
		if( mInitialized )
			return;
		CI_ASSERT( mOutput );

		// TODO: need to traverse graph and initialize here
		
		mOutput->initialize();
		mInitialized = true;
	}

	void Graph::uninitialize()
	{
		if( ! mInitialized )
			return;

		stop();
		if( mOutput )
			mOutput->uninitialize();
		mInitialized = false;
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