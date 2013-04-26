#pragma once

#include <memory>

namespace audio2 {

class Device {
  public:

	virtual void start() = 0;
	virtual void stop() = 0;

	size_t	getNumChannels() const;
	size_t	getSampleRate() const;
};

class OutputDevice : public Device {
  public:
};

class InputDevice : public Device {
  public:
};

} // namespace audio2