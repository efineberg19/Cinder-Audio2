#pragma once

#include "audio2/Device.h"
#include <AudioUnit/AudioUnit.h>

namespace audio2 { namespace cocoa {

class DeviceAudioUnit : public Device {
  public:
	enum Bus { Output = 0, Input = 1 };

	virtual ~DeviceAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	const ::AudioComponentInstance& getComponentInstance();
	bool isInputConnected() const	{ return mInputConnected; }
	bool isOutputConnected() const	{ return mOutputConnected; }
	void setInputConnected()	{ mInputConnected = true; }
	void setOutputConnected()	{ mOutputConnected = true; }

	// FIXME: friendship isn't working since moving to namespaceaudio2::cocoa - sort it out, should be protected or private
	DeviceAudioUnit( const ::AudioComponentDescription &component, const std::string &key ); // TODO: swap these two for consistency

  private:

	::AudioComponentDescription mComponentDescription;
	::AudioComponentInstance	mComponentInstance;

	bool mInputConnected, mOutputConnected;
	
#if defined( CINDER_MAC )
	friend class DeviceManagerCoreAudio;
#elif defined( CINDER_COCOA_TOUCH )
	friend class DeviceManagerAudioSession;
#endif
};

} } // namespace audio2::cocoa