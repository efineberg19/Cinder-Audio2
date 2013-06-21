#include "audio2/audio.h"
#include "audio2/Device.h"

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
		app::console() << node->getTag() << "\t[ ch: " << node->getNumChannels() << " ]" << endl;
		for( auto &source : node->getSources() )
			printNode( source, depth + 1 );
	};

	printNode( graph->getRoot(), 0 );
}

void printDevices()
{
	for( auto &device : Device::getDevices() ) {
		app::console() << "-- " << device->getName() << " --" << endl;
		app::console() << "\t key: " << device->getKey() << endl;
		app::console() << "\t inputs: " << device->getNumInputChannels() << ", outputs: " << device->getNumOutputChannels() << endl;
		app::console() << "\t samplerate: " << device->getSampleRate() << ", frames per block: " << device->getNumFramesPerBlock() << endl;

	}
}

} // namespace audio2