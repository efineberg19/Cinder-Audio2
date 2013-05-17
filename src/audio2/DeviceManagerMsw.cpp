#include "audio2/DeviceManagerMsw.h"
#include "audio2/msw/xaudio.h"
#include "audio2/msw/util.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")

#include <initguid.h> // must be included before mmdeviceapi.h for the pkey defines to be properly instantiated. Both must be first included from a translation unit.
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

using namespace std;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerMsw
// ----------------------------------------------------------------------------------------------------

// TODO: consider if lazy-loading the devices container will improve startup time

DeviceRef DeviceManagerMsw::getDefaultOutput()
{
	::IMMDeviceEnumerator *enumerator;
	HRESULT hr = ::CoCreateInstance( __uuidof(::MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(::IMMDeviceEnumerator), (void**)&enumerator );
	//HRESULT hr =  CoCreateInstance( __uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(::IMMDeviceEnumerator), (void**)&enumerator );
	CI_ASSERT( hr == S_OK );
	auto enumeratorPtr = msw::makeComUnique( enumerator );

	::IMMDevice *device;
	hr = enumerator->GetDefaultAudioEndpoint( eRender, eConsole, &device );
	CI_ASSERT( hr == S_OK );

	auto devicePtr = msw::makeComUnique( device );

	::LPWSTR idStr;
	device->GetId( &idStr );
	CI_ASSERT( idStr );

	// TODO: this ID isn't useful, so for now using friendly name, but this is not safe
	string deviceId( ci::toUtf8( idStr ) );
	::CoTaskMemFree( idStr );
	LOG_V << "deviceId: " << deviceId << endl;

	IPropertyStore *properties;
	hr = device->OpenPropertyStore( STGM_READ, &properties );
	CI_ASSERT( hr == S_OK );
	auto propertiesPtr = msw::makeComUnique( properties );

	PROPVARIANT nameVar;
	hr = properties->GetValue( PKEY_Device_FriendlyName, &nameVar );
	CI_ASSERT( hr == S_OK );

	string friendlyName = ci::toUtf8( nameVar.pwszVal );
	LOG_V << "friendlyName: " << friendlyName << endl;
	return getDevice( friendlyName );
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

DeviceRef DeviceManagerMsw::getDevice( const string &key )
{
	for( auto device : getDevices() ) {
		LOG_V << "name: " << device.name << endl;
		LOG_V << "key: " << device.key << endl;
		LOG_V << "deviceId: " << ci::toUtf8( device.deviceId ) << endl;

		// FIXME: these keys don't match - who would have guessed..
		if( key == device.key ) {
			LOG_V << "MATCH" << endl;
			// TODO NEXT: return Device constructed with this key
			break;
		}
	}
	return DeviceRef();
}

DeviceManagerMsw::DeviceContainerT& DeviceManagerMsw::getDevices()
{
	if( mDevices.empty() ) {

		::HDEVINFO devInfoSet;
		::DWORD devCount = 0;
		::SP_DEVINFO_DATA devInfo;
		::SP_DEVICE_INTERFACE_DATA devInterface;

		devInfoSet = ::SetupDiGetClassDevs( &DEVINTERFACE_AUDIO_RENDER, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT );
		if( devInfoSet == INVALID_HANDLE_VALUE ) {
			LOG_E << "INVALID_HANDLE_VALUE" << endl;
			return mDevices;
		}

		devInfo.cbSize = sizeof( ::SP_DEVINFO_DATA );
		devInterface.cbSize = sizeof( ::SP_DEVICE_INTERFACE_DATA );

		DWORD deviceIndex = 0;

		while ( true ) {

			// TODO: parse DEVINTERFACE_AUDIO_CAPTURE too
			if( ! SetupDiEnumDeviceInterfaces( devInfoSet, 0, &DEVINTERFACE_AUDIO_RENDER, deviceIndex, &devInterface ) ) {
				DWORD error = GetLastError();
				if( error == ERROR_NO_MORE_ITEMS ) {
					// ok, we're done.
				} else {
					LOG_V << "get device returned false. error: " << error << endl;
				}
				break;
			}
			deviceIndex++;

			// See how large a buffer we require for the device interface details (ignore error, it should be returning ERROR_INSUFFICIENT_BUFFER
			DWORD sizeDevInterface;
			::SetupDiGetDeviceInterfaceDetail( devInfoSet, &devInterface, 0, 0, &sizeDevInterface, 0 );

			shared_ptr<::SP_DEVICE_INTERFACE_DETAIL_DATA> interfaceDetail( (::SP_DEVICE_INTERFACE_DETAIL_DATA*)calloc( 1, sizeDevInterface ), free );
			if( interfaceDetail ) {
				interfaceDetail->cbSize = sizeof( ::SP_DEVICE_INTERFACE_DETAIL_DATA );
				devInfo.cbSize = sizeof( ::SP_DEVINFO_DATA );
				if( ! ::SetupDiGetDeviceInterfaceDetail( devInfoSet, &devInterface, interfaceDetail.get(), sizeDevInterface, 0, &devInfo ) ) {
					continue;
					DWORD error = GetLastError();
					LOG_V << "get device returned false. error: " << error << endl;
				}

				char friendlyName[2048];
				DWORD sizeName = sizeof( friendlyName );
				friendlyName[0] = 0;
				::DWORD propertyDataType;
				if( ! SetupDiGetDeviceRegistryPropertyA( devInfoSet, &devInfo, SPDRP_FRIENDLYNAME, &propertyDataType, (LPBYTE)friendlyName, sizeName, 0 ) ) {
					DWORD error = GetLastError();
					LOG_V << "get device returned false. error: " << error << endl;
					continue;
				}

				const DWORD kMaxDeviceIdSize = 2048;
				DWORD requiredSize;
				wchar_t deviceId[kMaxDeviceIdSize];
				deviceId[0] = 0;
				bool success = ::SetupDiGetDeviceInstanceId( devInfoSet, &devInfo, deviceId, kMaxDeviceIdSize, &requiredSize );
				CI_ASSERT( success );
				CI_ASSERT( requiredSize <= 2048 );

				mDevices.push_back( DeviceInfo() );
				DeviceInfo &devInfo = mDevices.back();
				devInfo.name = string( friendlyName );
				devInfo.usage = DeviceInfo::Usage::Output;
				//devInfo.key = ci::toUtf8( wstring( deviceId ) );
				devInfo.key = devInfo.name;

				devInfo.deviceId = wstring( interfaceDetail->DevicePath );			}
		}

		if (devInfoSet) {
			SetupDiDestroyDeviceInfoList( devInfoSet );
		}
	}

	return mDevices;
}

} // namespace audio2