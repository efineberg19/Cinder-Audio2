#include "audio2/DeviceManagerAudioSession.h"
#include "audio2/DeviceAudioUnit.h"

using namespace std;

namespace audio2 {

	const string kRemoteIOKey = "iOS-RemoteIO";

	// ----------------------------------------------------------------------------------------------------
	// MARK: - DeviceManagerAudioSession
	// ----------------------------------------------------------------------------------------------------

	DeviceRef DeviceManagerAudioSession::getDefaultOutput()
	{
		::AudioComponentDescription component{ 0 };
		component.componentType = kAudioUnitType_Output;
		component.componentSubType = kAudioUnitSubType_RemoteIO;
		component.componentManufacturer = kAudioUnitManufacturer_Apple;

		mRemoteIOUnit = DeviceRef( new DeviceAudioUnit( component, kRemoteIOKey ) );

		return mRemoteIOUnit;
	}

	DeviceRef DeviceManagerAudioSession::getDefaultInput()
	{
		throw "not implemented";
		return DeviceRef();
	}

	
} // namespace audio2