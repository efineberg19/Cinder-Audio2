/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "audio2/msw/DeviceManagerWasapi.h"
#include "audio2/msw/DeviceOutputXAudio.h"
#include "audio2/msw/DeviceInputWasapi.h"
#include "audio2/audio.h"
#include "audio2/msw/util.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"
#include "cinder/msw/CinderMsw.h"

#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")

#include <initguid.h> // must be included before mmdeviceapi.h for the pkey defines to be properly instantiated. Both must be first included from a translation unit.
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#if defined( _USING_V110_SDK71_ )
// The GUID's needed to query audio interfaces were not exposed in v110_xp sdk, for whatever reason, so I'm defining them here as they are when building with v110.
DEFINE_GUID(DEVINTERFACE_AUDIO_RENDER , 0xe6327cad, 0xdcec, 0x4949, 0xae, 0x8a, 0x99, 0x1e, 0x97, 0x6a, 0x79, 0xd2);
DEFINE_GUID(DEVINTERFACE_AUDIO_CAPTURE, 0x2eef81be, 0x33fa, 0x4800, 0x96, 0x70, 0x1c, 0xd4, 0x74, 0x97, 0x2c, 0x3f);
#endif

using namespace std;

namespace audio2 { namespace msw {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerWasapi
// ----------------------------------------------------------------------------------------------------

// TODO: consider if lazy-loading the devices container will improve startup time

DeviceRef DeviceManagerWasapi::getDefaultOutput()
{
	::IMMDeviceEnumerator *enumerator;
	HRESULT hr = ::CoCreateInstance( __uuidof(::MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(::IMMDeviceEnumerator), (void**)&enumerator );
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

	return findDeviceByKey( key );
}

DeviceRef DeviceManagerWasapi::getDefaultInput()
{
	::IMMDeviceEnumerator *enumerator;
	HRESULT hr = ::CoCreateInstance( __uuidof(::MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(::IMMDeviceEnumerator), (void**)&enumerator );
	CI_ASSERT( hr == S_OK );
	auto enumeratorPtr = msw::makeComUnique( enumerator );

	::IMMDevice *device;
	hr = enumerator->GetDefaultAudioEndpoint( eCapture, eConsole, &device );
	CI_ASSERT( hr == S_OK );

	auto devicePtr = msw::makeComUnique( device );

	::LPWSTR idStr;
	device->GetId( &idStr );
	CI_ASSERT( idStr );

	string key( ci::toUtf8( idStr ) );
	::CoTaskMemFree( idStr );
	LOG_V << "key: " << key << endl;

	return findDeviceByKey( key );
}

const std::vector<DeviceRef>& DeviceManagerWasapi::getDevices()
{
	if( mDevices.empty() ) {
		parseDevices( DeviceInfo::Usage::Input );
		parseDevices( DeviceInfo::Usage::Output );
	}
	return mDevices;
}

void DeviceManagerWasapi::setActiveDevice( const string &key )
{
	CI_ASSERT( 0 && "TODO" ); // umm, might not be anything needing doing here
}

std::string DeviceManagerWasapi::getName( const string &key )
{
	return getDeviceInfo( key ).name;
}

size_t DeviceManagerWasapi::getNumInputChannels( const string &key )
{
	// FIXME: need a way to distinguish inputs and outputs in devInfo 
	return 0;
}

size_t DeviceManagerWasapi::getNumOutputChannels( const string &key )
{
	return getDeviceInfo( key ).numChannels;
}

size_t DeviceManagerWasapi::getSampleRate( const string &key )
{
	return getDeviceInfo( key ).sampleRate;
}

size_t DeviceManagerWasapi::getNumFramesPerBlock( const string &key )
{
	// ???: I don't know of any way to get a device's preferred blocksize on windows, if it exists.
	// - if it doesn't need a way to tell the user they should not listen to this value,
	//   or we can use a pretty standard default (like 512 or 1024).
	// - IAudioClient::GetBufferSize seems to be a possiblity, needs to be activated first
	return 512;
}

const std::wstring& DeviceManagerWasapi::getDeviceId( const std::string &key )
{
	return getDeviceInfo( key ).deviceId;
}

shared_ptr<::IMMDevice> DeviceManagerWasapi::getIMMDevice( const std::string &key )
{
	::IMMDeviceEnumerator *enumerator;
	HRESULT hr = ::CoCreateInstance( __uuidof(::MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(::IMMDeviceEnumerator), (void**)&enumerator );
	CI_ASSERT( hr == S_OK );
	auto enumeratorPtr = msw::makeComUnique( enumerator );

	::IMMDevice *device;
	const wstring &endpointId = getDeviceInfo( key ).endpointId;
	hr = enumerator->GetDevice( endpointId.c_str(), &device );
	CI_ASSERT( hr == S_OK );

	return 	ci::msw::makeComShared( device );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Private
// ----------------------------------------------------------------------------------------------------

DeviceManagerWasapi::DeviceInfo& DeviceManagerWasapi::getDeviceInfo( const std::string &key )
{
	CI_ASSERT( ! mDeviceInfoArray.empty() );

	for( auto& devInfo : mDeviceInfoArray ) {
		if( key == devInfo.key )
			return devInfo;
	}
	throw AudioDeviceExc( string( "could not find device info for key: " ) + key );
}

//DeviceManagerWasapi::DeviceContainerT& DeviceManagerWasapi::getDevices()
//{
//	if( mDevices.empty() ) {
//		parseDevices( DeviceInfo::Usage::Input );
//		parseDevices( DeviceInfo::Usage::Output );
//	}
//	return mDevices;
//}

// This call is performed twice because a separate Device subclass is used for input and output
// and by using eRender / eCapture instead of eAll when enumerating the endpoints, it is easier
// to distinguish between the two.
void DeviceManagerWasapi::parseDevices( DeviceInfo::Usage usage )
{
	const size_t kMaxPropertyStringLength = 2048;

	CONST ::GUID *devInterfaceGuid = ( usage == DeviceInfo::Usage::Input ? &DEVINTERFACE_AUDIO_CAPTURE : &DEVINTERFACE_AUDIO_RENDER );
	::HDEVINFO devInfoSet = ::SetupDiGetClassDevs( devInterfaceGuid, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT );
	if( devInfoSet == INVALID_HANDLE_VALUE ) {
		LOG_E << "INVALID_HANDLE_VALUE, detailed error: " << GetLastError() << endl;
		CI_ASSERT( false );
		return;
	}

	::SP_DEVICE_INTERFACE_DATA devInterface = {0};
	devInterface.cbSize = sizeof( ::SP_DEVICE_INTERFACE_DATA );
	DWORD deviceIndex = 0;
	vector<wstring> deviceIds;

	while ( true ) {
		if( ! ::SetupDiEnumDeviceInterfaces( devInfoSet, 0, devInterfaceGuid, deviceIndex, &devInterface ) ) {
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

		::SP_DEVINFO_DATA devInfo = {0};
		devInfo.cbSize = sizeof( ::SP_DEVINFO_DATA );
		if( ! ::SetupDiGetDeviceInterfaceDetail( devInfoSet, &devInterface, interfaceDetail.get(), sizeDevInterface, 0, &devInfo ) ) {
			continue;
			DWORD error = ::GetLastError();
			LOG_V << "get device returned false. error: " << error << endl;
		}

		deviceIds.push_back( wstring( interfaceDetail->DevicePath ) );
	}
	if( devInfoSet )
		::SetupDiDestroyDeviceInfoList( devInfoSet );

	::IMMDeviceEnumerator *enumerator;
	const ::CLSID CLSID_MMDeviceEnumerator = __uuidof( ::MMDeviceEnumerator );
	const ::IID IID_IMMDeviceEnumerator = __uuidof( ::IMMDeviceEnumerator );
	HRESULT hr = CoCreateInstance( CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator );
	CI_ASSERT( hr == S_OK);
	auto enumeratorPtr = msw::makeComUnique( enumerator );

	::EDataFlow dataFlow = ( usage == DeviceInfo::Usage::Input ? eCapture : eRender );
	::IMMDeviceCollection *devices;
	hr = enumerator->EnumAudioEndpoints( dataFlow, DEVICE_STATE_ACTIVE, &devices );
	CI_ASSERT( hr == S_OK);
	auto devicesPtr = msw::makeComUnique( devices );

	UINT numDevices;
	hr = devices->GetCount( &numDevices );
	CI_ASSERT( hr == S_OK);

	for ( UINT i = 0; i < numDevices; i++ )	{
		mDeviceInfoArray.push_back( DeviceInfo() );
		DeviceInfo &devInfo = mDeviceInfoArray.back();
		devInfo.usage = usage;

		::IMMDevice *deviceImm;
		hr = devices->Item( i, &deviceImm );
		CI_ASSERT( hr == S_OK);
		auto devicePtr = msw::makeComUnique( deviceImm );

		::IPropertyStore *properties;
		hr = deviceImm->OpenPropertyStore( STGM_READ, &properties );
		CI_ASSERT( hr == S_OK);
		auto propertiesPtr = msw::makeComUnique( properties );

		::PROPVARIANT nameVar;
		hr = properties->GetValue( PKEY_Device_FriendlyName, &nameVar );
		CI_ASSERT( hr == S_OK );
		devInfo.name = ci::toUtf8( nameVar.pwszVal );

		LPWSTR endpointIdLpwStr;
		hr = deviceImm->GetId( &endpointIdLpwStr );
		CI_ASSERT( hr == S_OK );
		devInfo.endpointId = wstring( endpointIdLpwStr );
		devInfo.key = ci::toUtf8( devInfo.endpointId );
		::CoTaskMemFree( endpointIdLpwStr );
		
		// Wasapi's device Id is actually a subset of the one xaudio needs, so we find and use the match.
		// TODO: probably should just do this for output, since input is fine working with the key 
		for( auto it = deviceIds.begin(); it != deviceIds.end(); ++it ) {
			if( it->find( devInfo.endpointId ) != wstring::npos ) {
				devInfo.deviceId = *it;
				deviceIds.erase( it );
				break;
			}
		}

		::PROPVARIANT formatVar;
		hr = properties->GetValue( PKEY_AudioEngine_DeviceFormat, &formatVar );
		CI_ASSERT( hr == S_OK );
		::WAVEFORMATEX *format = (::WAVEFORMATEX *)formatVar.blob.pBlobData;

		devInfo.numChannels = format->nChannels;
		devInfo.sampleRate = format->nSamplesPerSec;


		DeviceRef device = ( usage == DeviceInfo::Usage::Input ? DeviceRef( new DeviceInputWasapi( devInfo.key ) ) : DeviceRef( new DeviceOutputXAudio( devInfo.key ) ) );
		mDevices.push_back( device );
	}
}

}} // namespace audio2::msw