#include "audio2/DeviceManagerMsw.h"
#include "audio2/DeviceOutputXAudio.h"
#include "audio2/audio.h"
#include "audio2/msw/util.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")

#include <initguid.h> // must be included before mmdeviceapi.h for the pkey defines to be properly instantiated. Both must be first included from a translation unit.
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <map>

#if defined( CINDER_XAUDIO_2_7 )
// The GUID's needed to query audio interfaces were not exposed in v110_xp sdk, for whatever reason, so I'm defining them here as they are when building with v110.
DEFINE_GUID(DEVINTERFACE_AUDIO_RENDER , 0xe6327cad, 0xdcec, 0x4949, 0xae, 0x8a, 0x99, 0x1e, 0x97, 0x6a, 0x79, 0xd2);
DEFINE_GUID(DEVINTERFACE_AUDIO_CAPTURE, 0x2eef81be, 0x33fa, 0x4800, 0x96, 0x70, 0x1c, 0xd4, 0x74, 0x97, 0x2c, 0x3f);
#endif

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

	string key( ci::toUtf8( idStr ) );
	::CoTaskMemFree( idStr );
	LOG_V << "key: " << key << endl;

	//IPropertyStore *properties;
	//hr = device->OpenPropertyStore( STGM_READ, &properties );
	//CI_ASSERT( hr == S_OK );
	//auto propertiesPtr = msw::makeComUnique( properties );

	//PROPVARIANT nameVar;
	//hr = properties->GetValue( PKEY_Device_FriendlyName, &nameVar );
	//CI_ASSERT( hr == S_OK );


	return getDevice( key );
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
	return getDeviceInfo( key ).name;
}

size_t DeviceManagerMsw::getNumInputChannels( const string &key )
{
	// FIXME: need a way to distinguish inputs and outputs in devInfo 
	return 0;
}

size_t DeviceManagerMsw::getNumOutputChannels( const string &key )
{
	return getDeviceInfo( key ).numChannels;
}

size_t DeviceManagerMsw::getSampleRate( const string &key )
{
	return getDeviceInfo( key ).sampleRate;
}

size_t DeviceManagerMsw::getBlockSize( const string &key )
{
	// ???: I don't know of any way to get a device's preferred blocksize on windows, if it exists.
	// - if it doesn't need a way to tell the user they should not listen to this value,
	//   or we can use a pretty standard default (like 512 or 1024).
	return 0;
}

const std::wstring& DeviceManagerMsw::getDeviceId( const std::string &key )
{
	return getDeviceInfo( key ).deviceId;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Private
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManagerMsw::getDevice( const string &key )
{
	return getDeviceInfo( key ).device;
}

DeviceManagerMsw::DeviceInfo& DeviceManagerMsw::getDeviceInfo( const std::string &key )
{
	for( auto& devInfo : getDevices() ) {
		if( key == devInfo.key )
			return devInfo;
	}
	throw AudioDeviceExc( string( "could not find device for key: " ) + key );
}

// TODO: cleanup
// TODO: parse DEVINTERFACE_AUDIO_CAPTURE too
DeviceManagerMsw::DeviceContainerT& DeviceManagerMsw::getDevices()
{
	const size_t kMaxPropertyStringLength = 2048;

	if( mDevices.empty() ) {

		::HDEVINFO devInfoSet;
		::DWORD devCount = 0;
		::SP_DEVINFO_DATA devInfo;
		::SP_DEVICE_INTERFACE_DATA devInterface;

		devInfoSet = ::SetupDiGetClassDevs( &DEVINTERFACE_AUDIO_RENDER, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT );
		if( devInfoSet == INVALID_HANDLE_VALUE ) {
			LOG_E << "INVALID_HANDLE_VALUE, detailed error: " << GetLastError() << endl;
			CI_ASSERT( false );
			return mDevices;
		}

		devInfo.cbSize = sizeof( ::SP_DEVINFO_DATA );
		devInterface.cbSize = sizeof( ::SP_DEVICE_INTERFACE_DATA );

		DWORD deviceIndex = 0;

		map<string, wstring> deviceIdMap; // [name:deviceId]

		while ( true ) {

			if( ! ::SetupDiEnumDeviceInterfaces( devInfoSet, 0, &DEVINTERFACE_AUDIO_RENDER, deviceIndex, &devInterface ) ) {
				DWORD error = GetLastError();
				if( error == ERROR_NO_MORE_ITEMS ) {
					// ok, we're done.
					break;
				} else {
					LOG_V << "get device returned false. error: " << error << endl;
					CI_ASSERT( false );
				}
			}
			deviceIndex++;

			// See how large a buffer we require for the device interface details (ignore error, it should be returning ERROR_INSUFFICIENT_BUFFER)
			DWORD sizeDevInterface;
			::SetupDiGetDeviceInterfaceDetail( devInfoSet, &devInterface, 0, 0, &sizeDevInterface, 0 );

			shared_ptr<::SP_DEVICE_INTERFACE_DETAIL_DATA> interfaceDetail( (::SP_DEVICE_INTERFACE_DETAIL_DATA*)calloc( 1, sizeDevInterface ), free );
			CI_ASSERT( interfaceDetail );

			interfaceDetail->cbSize = sizeof( ::SP_DEVICE_INTERFACE_DETAIL_DATA );
			devInfo.cbSize = sizeof( ::SP_DEVINFO_DATA );
			if( ! ::SetupDiGetDeviceInterfaceDetail( devInfoSet, &devInterface, interfaceDetail.get(), sizeDevInterface, 0, &devInfo ) ) {
				continue;
				DWORD error = ::GetLastError();
				LOG_V << "get device returned false. error: " << error << endl;
			}

			char friendlyName[kMaxPropertyStringLength];
			::DWORD propertyDataType;
			if( ! ::SetupDiGetDeviceRegistryPropertyA( devInfoSet, &devInfo, SPDRP_FRIENDLYNAME, &propertyDataType, (LPBYTE)friendlyName, kMaxPropertyStringLength, 0 ) ) {
				LOG_E << "could not get SPDRP_FRIENDLYNAME. error: " << ::GetLastError() << endl;
				continue;
			}

			// note: I am keeping this code here as record that I have tried these two properties and neither directly correlate with the ID or GUID available through MMDevice

			//char guid[kMaxPropertyStringLength];
			//if( ! SetupDiGetDeviceRegistryPropertyA( devInfoSet, &devInfo, SPDRP_HARDWAREID, &propertyDataType, (LPBYTE)guid, kMaxPropertyStringLength, 0 ) ) {
			//	LOG_E << "could not get SPDRP_CLASSGUID. error: " << GetLastError() << endl;
			//	continue;
			//}

			//DWORD requiredSize;
			//wchar_t deviceId[kMaxPropertyStringLength];
			//deviceId[0] = 0;
			//bool success = ::SetupDiGetDeviceInstanceId( devInfoSet, &devInfo, deviceId, kMaxPropertyStringLength, &requiredSize );
			//CI_ASSERT( success );
			//CI_ASSERT( requiredSize <= kMaxPropertyStringLength );

			//LOG_V << "deviceId: " << ci::toUtf8( wstring( deviceId ) ) << endl;
			//LOG_V << "guid: " << guid << endl;

			string name( friendlyName );
			wstring deviceId( interfaceDetail->DevicePath );

			//mDevices.push_back( DeviceInfo() );
			//DeviceInfo &devInfo = mDevices.back();

			CI_ASSERT( deviceIdMap.find( name ) == deviceIdMap.end() );
			deviceIdMap[name] = deviceId;
		}
		if( devInfoSet )
			::SetupDiDestroyDeviceInfoList( devInfoSet );

		::IMMDeviceEnumerator *enumerator;
		const ::CLSID CLSID_MMDeviceEnumerator = __uuidof( ::MMDeviceEnumerator );
		const ::IID IID_IMMDeviceEnumerator = __uuidof( ::IMMDeviceEnumerator );
		HRESULT hr = CoCreateInstance( CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator );
		CI_ASSERT( hr == S_OK);
		auto enumeratorPtr = msw::makeComUnique( enumerator );

		::IMMDeviceCollection *devices;
		hr = enumerator->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &devices );
		CI_ASSERT( hr == S_OK);
		auto devicesPtr = msw::makeComUnique( devices );

		UINT numDevices;
		hr = devices->GetCount( &numDevices );
		CI_ASSERT( hr == S_OK);

		for ( UINT i = 0; i < numDevices; i++ )	{
			mDevices.push_back( DeviceInfo() );
			DeviceInfo &devInfo = mDevices.back();

			::IMMDevice *device;
			hr = devices->Item( i, &device );
			CI_ASSERT( hr == S_OK);
			auto devicePtr = msw::makeComUnique( device );

			::IPropertyStore *properties;
			hr = device->OpenPropertyStore( STGM_READ, &properties );
			CI_ASSERT( hr == S_OK);
			auto propertiesPtr = msw::makeComUnique( properties );

			::PROPVARIANT nameVar;
			hr = properties->GetValue( PKEY_Device_FriendlyName, &nameVar );
			devInfo.name = ci::toUtf8( nameVar.pwszVal );
			CI_ASSERT( hr == S_OK );

			LPWSTR idStr;
			hr = device->GetId( &idStr );
			CI_ASSERT( hr == S_OK );
			devInfo.key = ci::toUtf8( idStr );
			::CoTaskMemFree( idStr );

			devInfo.usage = DeviceInfo::Usage::Output;
			devInfo.device = DeviceRef( new DeviceOutputXAudio( devInfo.key ) );

			CI_ASSERT( deviceIdMap.find( devInfo.name ) != deviceIdMap.end() );
			devInfo.deviceId = deviceIdMap[devInfo.name];

			// TODO: PKEY_AudioEndpoint_GUID seems the most likely to be available somewhere in SetupDi - try again to look for it
			//PROPVARIANT guidVar;
			//hr = properties->GetValue( PKEY_AudioEndpoint_GUID, &guidVar );
			//CI_ASSERT( hr == S_OK );

			::PROPVARIANT formatVar;
			hr = properties->GetValue( PKEY_AudioEngine_DeviceFormat, &formatVar );
			CI_ASSERT( hr == S_OK );
			::WAVEFORMATEX *format = (::WAVEFORMATEX *)formatVar.blob.pBlobData;

			devInfo.numChannels = format->nChannels;
			devInfo.sampleRate = format->nSamplesPerSec;
		}
	}

	return mDevices;
}

} // namespace audio2