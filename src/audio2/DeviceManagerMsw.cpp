#include "audio2/DeviceManagerMsw.h"
#include "audio2/msw/xaudio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace std;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerMsw
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManagerMsw::getDefaultOutput()
{
	CI_ASSERT( 0 && "TODO" );
	return DeviceRef();
}

DeviceRef DeviceManagerMsw::getDefaultInput()
{
	CI_ASSERT( 0 && "TODO" );
	return DeviceRef();
}

void DeviceManagerMsw::setActiveDevice( const string &key )
{
	CI_ASSERT( 0 && "TODO" );

}

std::string DeviceManagerMsw::getName( const string &key )
{
	CI_ASSERT( 0 && "TODO" );
	return "";
}

size_t DeviceManagerMsw::getNumInputChannels( const string &key )
{
	CI_ASSERT( 0 && "TODO" );
	return 0;
}

size_t DeviceManagerMsw::getNumOutputChannels( const string &key )
{
	CI_ASSERT( 0 && "TODO" );
	return 0;
}

size_t DeviceManagerMsw::getSampleRate( const string &key )
{
	CI_ASSERT( 0 && "TODO" );
	return 0;
}

size_t DeviceManagerMsw::getBlockSize( const string &key )
{
	CI_ASSERT( 0 && "TODO" );
	return 0;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Private
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManagerMsw::getDevice( const std::string &key )
{
	// TODO NEXT
}

} // namespace audio2