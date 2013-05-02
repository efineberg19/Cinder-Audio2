#include "audio2/Graph.h"

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

} // namespace audio2