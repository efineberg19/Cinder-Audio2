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
// MARK: - OutputDeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputDeviceAudioUnit::OutputDeviceAudioUnit( const string &key )
: mKey( key )
{

}

OutputDeviceAudioUnit::~OutputDeviceAudioUnit()
{
}

void OutputDeviceAudioUnit::initialize()
{

}

void OutputDeviceAudioUnit::uninitialize()
{

}

void OutputDeviceAudioUnit::start()
{

}

void OutputDeviceAudioUnit::stop()
{

}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl
// ----------------------------------------------------------------------------------------------------



} // namespace audio2
