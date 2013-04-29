#include "audio2/DeviceAudioUnit.h"
#include "audio2/audio.h"
#include "audio2/assert.h"


using namespace std;
using namespace ci;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl Declaration
// ----------------------------------------------------------------------------------------------------

struct DeviceAudioUnitImpl {
	DeviceAudioUnitImpl()	{}

};

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

	DeviceAudioUnit::DeviceAudioUnit( const ::AudioComponentDescription &component, const std::string &key )
: mKey( key )
{

}

DeviceAudioUnit::~DeviceAudioUnit()
{
}

void DeviceAudioUnit::initialize()
{

}

void DeviceAudioUnit::uninitialize()
{

}

void DeviceAudioUnit::start()
{

}

void DeviceAudioUnit::stop()
{

}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl
// ----------------------------------------------------------------------------------------------------



} // namespace audio2
