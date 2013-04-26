#include "audio2/DeviceAudioUnit.h"

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl
// ----------------------------------------------------------------------------------------------------

struct DeviceAudioUnitImpl {

};

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputDeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputDeviceAudioUnit::~OutputDeviceAudioUnit()
{
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputDeviceRef DeviceManagerAudioUnit::getDefaultOutput()
{
	return OutputDeviceRef();
}

InputDeviceRef DeviceManagerAudioUnit::getDefaultInput()
{
	return InputDeviceRef();
}

} // namespace audio2