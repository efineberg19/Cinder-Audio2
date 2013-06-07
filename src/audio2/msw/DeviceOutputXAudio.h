#pragma once

#include "audio2/Device.h"
#include "audio2/msw/xaudio.h"

namespace audio2 { namespace msw {

class DeviceOutputXAudio : public Device {
  public:

	virtual ~DeviceOutputXAudio();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::IXAudio2* getXAudio() const	{ return mXAudio; }

  private:
	DeviceOutputXAudio( const std::string &key );

	// TODO: use auto pointers
	::IXAudio2 *mXAudio;
	::IXAudio2MasteringVoice *mMasteringVoice;

	friend class DeviceManagerWasapi;
};

}} // namespace audio2::msw
