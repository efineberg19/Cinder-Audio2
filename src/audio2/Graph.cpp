#include "audio2/Graph.h"
#include "audio2/assert.h"

namespace audio2 {

	const Node::Format& Node::getSourceFormat()
	{
		CI_ASSERT( ! mSources.empty() );
		return mSources[0]->mFormat;
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
		start( mOutput );
	}

	void Graph::stop()
	{
		if( ! mRunning )
			return;
		mRunning = false;
		stop( mOutput );
	}

	void Graph::start( NodeRef node )
	{
		for( auto& source : node->getSources() )
			start( source );

		node->start();
	}

	void Graph::stop( NodeRef node )
	{
		for( auto& source : node->getSources() )
			stop( source );

		node->stop();
	}

} // namespace audio2