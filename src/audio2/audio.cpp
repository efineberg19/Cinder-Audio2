#include "audio2/audio.h"

#include "cinder/app/App.h"

using namespace std;
using namespace ci;

namespace audio2 {

void printGraph( ContextRef graph )
{
	function<void( NodeRef, size_t )> printNode = [&]( NodeRef node, size_t depth ) -> void {
		if( ! node )
			return;
		for( size_t i = 0; i < depth; i++ )
			app::console() << "-- ";
		app::console() << node->getTag() << "\t[ ch: " << node->getFormat().getNumChannels() << " ]" << endl;
		for( auto &source : node->getSources() )
			printNode( source, depth + 1 );
	};

	printNode( graph->getRoot(), 0 );
}

} // namespace audio2