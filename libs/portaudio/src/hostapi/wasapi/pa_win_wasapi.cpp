/*
 * Portable Audio I/O Library WASAPI implementation
 * Copyright (c) 2006 David Viens
 *
 * Based on the Open Source API proposed by Ross Bencina
 * Copyright (c) 1999-2002 Ross Bencina, Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */

/** @file
 @ingroup hostaip_src
 @brief WASAPI implementation of support for a host API.

 @note This file is provided as a starting point for implementing support for
 a new host API. IMPLEMENT ME comments are used to indicate functionality
 which much be customised for each implementation.
*/



//these headers are only in Windows SDK CTP Feb 2006 and only work in VC 2005!
#if _MSC_VER >= 1400
#include <windows.h>
#include <MMReg.h>  //must be before other Wasapi headers
#include <strsafe.h>
#include <mmdeviceapi.h>
#include <Avrt.h>
#include <audioclient.h>
#include <KsMedia.h>
#include <functiondiscoverykeys.h>  // PKEY_Device_FriendlyName
#endif



#include "pa_util.h"
#include "pa_allocation.h"
#include "pa_hostapi.h"
#include "pa_stream.h"
#include "pa_cpuload.h"
#include "pa_process.h"



/* prototypes for functions declared in this file */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

PaError PaWinWasapi_Initialize( PaUtilHostApiRepresentation **hostApi, PaHostApiIndex index );

#ifdef __cplusplus
}
#endif /* __cplusplus */




static void Terminate( struct PaUtilHostApiRepresentation *hostApi );
static PaError IsFormatSupported( struct PaUtilHostApiRepresentation *hostApi,
                                  const PaStreamParameters *inputParameters,
                                  const PaStreamParameters *outputParameters,
                                  double sampleRate );
static PaError OpenStream( struct PaUtilHostApiRepresentation *hostApi,
                           PaStream** s,
                           const PaStreamParameters *inputParameters,
                           const PaStreamParameters *outputParameters,
                           double sampleRate,
                           unsigned long framesPerBuffer,
                           PaStreamFlags streamFlags,
                           PaStreamCallback *streamCallback,
                           void *userData );
static PaError CloseStream( PaStream* stream );
static PaError StartStream( PaStream *stream );
static PaError StopStream( PaStream *stream );
static PaError AbortStream( PaStream *stream );
static PaError IsStreamStopped( PaStream *s );
static PaError IsStreamActive( PaStream *stream );
static PaTime GetStreamTime( PaStream *stream );
static double GetStreamCpuLoad( PaStream* stream );
static PaError ReadStream( PaStream* stream, void *buffer, unsigned long frames );
static PaError WriteStream( PaStream* stream, const void *buffer, unsigned long frames );
static signed long GetStreamReadAvailable( PaStream* stream );
static signed long GetStreamWriteAvailable( PaStream* stream );


/* IMPLEMENT ME: a macro like the following one should be used for reporting
 host errors */
#define PA_SKELETON_SET_LAST_HOST_ERROR( errorCode, errorText ) \
    PaUtil_SetLastHostErrorInfo( paInDevelopment, errorCode, errorText )

/* PaWinWasapiHostApiRepresentation - host api datastructure specific to this implementation */



//dummy entry point for other compilers and sdks
//currently built using RC1 SDK (5600)
#if _MSC_VER < 1400

PaError PaWinWasapi_Initialize( PaUtilHostApiRepresentation **hostApi, PaHostApiIndex hostApiIndex ){
    return paNoError;
}

#else




#define MAX_STR_LEN 512

/*
 These are fields that can be gathered from IDevice
 and IAudioDevice PRIOR to Initialize, and done in first pass
 i assume that neither of these will cause the Driver to "load",
 but again, who knows how they implement their stuff
 */
typedef struct PaWinWasapiDeviceInfo
{
    //hmm is it wise to keep a reference until Terminate?
    //TODO Check if that interface requires the driver to be loaded!
    IMMDevice * device;

    //Fields filled from IDevice
    //from GetId
    WCHAR szDeviceID[MAX_STR_LEN];
    //from GetState
    DWORD state;

    //Fields filled from IMMEndpoint'sGetDataFlow
    EDataFlow  flow;

    //Fields filled from IAudioDevice (_prior_ to Initialize)
    //from GetDevicePeriod(
    REFERENCE_TIME  DefaultDevicePeriod;
    REFERENCE_TIME  MinimumDevicePeriod;
    //from GetMixFormat
    WAVEFORMATEX   *MixFormat;//needs to be CoTaskMemFree'd after use!

} PaWinWasapiDeviceInfo;


typedef struct
{
    PaUtilHostApiRepresentation inheritedHostApiRep;
    PaUtilStreamInterface callbackStreamInterface;
    PaUtilStreamInterface blockingStreamInterface;

    PaUtilAllocationGroup *allocations;

    /* implementation specific data goes here */

    //in case we later need the synch
    IMMDeviceEnumerator * enumerator;

    //this is the REAL number of devices, whether they are usefull to PA or not!
    UINT deviceCount;

    WCHAR defaultRenderer [MAX_STR_LEN];
    WCHAR defaultCapturer [MAX_STR_LEN];

    PaWinWasapiDeviceInfo   *devInfo;
}PaWinWasapiHostApiRepresentation;


/* PaWinWasapiStream - a stream data structure specifically for this implementation */

typedef struct PaWinWasapiSubStream{
    IAudioClient        *client;
    WAVEFORMATEXTENSIBLE wavex;
    UINT32               bufferSize;
    REFERENCE_TIME       latency;
    REFERENCE_TIME       period;
    unsigned long framesPerHostCallback; /* just an example */
}PaWinWasapiSubStream;

typedef struct PaWinWasapiStream
{ /* IMPLEMENT ME: rename this */
    PaUtilStreamRepresentation streamRepresentation;
    PaUtilCpuLoadMeasurer cpuLoadMeasurer;
    PaUtilBufferProcessor bufferProcessor;

    /* IMPLEMENT ME:
            - implementation specific data goes here
    */


    //input
	PaWinWasapiSubStream in;
    IAudioCaptureClient *cclient;
    
	//output
	PaWinWasapiSubStream out;
    IAudioRenderClient  *rclient;


    bool running;
    bool closeRequest;

    DWORD dwThreadId;
    HANDLE hThread;

    GUID  session;

}PaWinWasapiStream;

#define PRINT(x) PA_DEBUG(x);

void
logAUDCLNT_E(HRESULT res){

    char *text = 0;
    switch(res){
        case S_OK: return; break;
        case E_POINTER                              :text ="E_POINTER"; break;
        case E_INVALIDARG                           :text ="E_INVALIDARG"; break;

        case AUDCLNT_E_NOT_INITIALIZED              :text ="AUDCLNT_E_NOT_INITIALIZED"; break;
        case AUDCLNT_E_ALREADY_INITIALIZED          :text ="AUDCLNT_E_ALREADY_INITIALIZED"; break;
        case AUDCLNT_E_WRONG_ENDPOINT_TYPE          :text ="AUDCLNT_E_WRONG_ENDPOINT_TYPE"; break;
        case AUDCLNT_E_DEVICE_INVALIDATED           :text ="AUDCLNT_E_DEVICE_INVALIDATED"; break;
        case AUDCLNT_E_NOT_STOPPED                  :text ="AUDCLNT_E_NOT_STOPPED"; break;
        case AUDCLNT_E_BUFFER_TOO_LARGE             :text ="AUDCLNT_E_BUFFER_TOO_LARGE"; break;
        case AUDCLNT_E_OUT_OF_ORDER                 :text ="AUDCLNT_E_OUT_OF_ORDER"; break;
        case AUDCLNT_E_UNSUPPORTED_FORMAT           :text ="AUDCLNT_E_UNSUPPORTED_FORMAT"; break;
        case AUDCLNT_E_INVALID_SIZE                 :text ="AUDCLNT_E_INVALID_SIZE"; break;
        case AUDCLNT_E_DEVICE_IN_USE                :text ="AUDCLNT_E_DEVICE_IN_USE"; break;
        case AUDCLNT_E_BUFFER_OPERATION_PENDING     :text ="AUDCLNT_E_BUFFER_OPERATION_PENDING"; break;
        case AUDCLNT_E_THREAD_NOT_REGISTERED        :text ="AUDCLNT_E_THREAD_NOT_REGISTERED"; break;      
		case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED   :text ="AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED"; break;
        case AUDCLNT_E_ENDPOINT_CREATE_FAILED       :text ="AUDCLNT_E_ENDPOINT_CREATE_FAILED"; break;
        case AUDCLNT_E_SERVICE_NOT_RUNNING          :text ="AUDCLNT_E_SERVICE_NOT_RUNNING"; break;
     //  case AUDCLNT_E_CPUUSAGE_EXCEEDED            :text ="AUDCLNT_E_CPUUSAGE_EXCEEDED"; break;
     //Header error?
        case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED     :text ="AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED"; break;
        case AUDCLNT_E_EXCLUSIVE_MODE_ONLY          :text ="AUDCLNT_E_EXCLUSIVE_MODE_ONLY"; break;
        case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL :text ="AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL"; break;
        case AUDCLNT_E_EVENTHANDLE_NOT_SET          :text ="AUDCLNT_E_EVENTHANDLE_NOT_SET"; break;
        case AUDCLNT_E_INCORRECT_BUFFER_SIZE        :text ="AUDCLNT_E_INCORRECT_BUFFER_SIZE"; break;
        case AUDCLNT_E_BUFFER_SIZE_ERROR            :text ="AUDCLNT_E_BUFFER_SIZE_ERROR"; break;
        case AUDCLNT_S_BUFFER_EMPTY                 :text ="AUDCLNT_S_BUFFER_EMPTY"; break;
        case AUDCLNT_S_THREAD_ALREADY_REGISTERED    :text ="AUDCLNT_S_THREAD_ALREADY_REGISTERED"; break;
        default:
            text =" dunno!";
            return ;
        break;

    }
    PRINT(("WASAPI ERROR HRESULT: 0x%X : %s\n",res,text));
}

inline double
nano100ToMillis(const REFERENCE_TIME &ref){
    //  1 nano = 0.000000001 seconds
    //100 nano = 0.0000001   seconds
    //100 nano = 0.0001   milliseconds
    return ((double)ref)*0.0001;
}

inline double
nano100ToSeconds(const REFERENCE_TIME &ref){
    //  1 nano = 0.000000001 seconds
    //100 nano = 0.0000001   seconds
    //100 nano = 0.0001   milliseconds
    return ((double)ref)*0.0000001;
}

#ifndef IF_FAILED_JUMP
#define IF_FAILED_JUMP(hr, label) if(FAILED(hr)) goto label;
#endif



//AVRT is the new "multimedia schedulling stuff"

typedef BOOL   (WINAPI *FAvRtCreateThreadOrderingGroup) (PHANDLE,PLARGE_INTEGER,GUID*,PLARGE_INTEGER);
typedef BOOL   (WINAPI *FAvRtDeleteThreadOrderingGroup) (HANDLE);
typedef BOOL   (WINAPI *FAvRtWaitOnThreadOrderingGroup) (HANDLE);
typedef HANDLE (WINAPI *FAvSetMmThreadCharacteristics)  (LPCTSTR,LPDWORD);
typedef BOOL   (WINAPI *FAvSetMmThreadPriority)         (HANDLE,AVRT_PRIORITY);

HMODULE  hDInputDLL = 0;
FAvRtCreateThreadOrderingGroup pAvRtCreateThreadOrderingGroup=0;
FAvRtDeleteThreadOrderingGroup pAvRtDeleteThreadOrderingGroup=0;
FAvRtWaitOnThreadOrderingGroup pAvRtWaitOnThreadOrderingGroup=0;
FAvSetMmThreadCharacteristics  pAvSetMmThreadCharacteristics=0;
FAvSetMmThreadPriority         pAvSetMmThreadPriority=0;

#define setupPTR(fun, type, name)  {                                                        \
                                        fun = (type) GetProcAddress(hDInputDLL,name);       \
                                        if(fun == NULL) {                                   \
                                            PRINT(("GetProcAddr failed for %s" ,name));     \
                                            return false;                                   \
                                        }                                                   \
                                    }                                                       \

bool
setupAVRT(){

    hDInputDLL = LoadLibraryA("avrt.dll");
    if(hDInputDLL == NULL)
        return false;

    setupPTR(pAvRtCreateThreadOrderingGroup, FAvRtCreateThreadOrderingGroup, "AvRtCreateThreadOrderingGroup");
    setupPTR(pAvRtDeleteThreadOrderingGroup, FAvRtDeleteThreadOrderingGroup, "AvRtDeleteThreadOrderingGroup");
    setupPTR(pAvRtWaitOnThreadOrderingGroup, FAvRtWaitOnThreadOrderingGroup, "AvRtWaitOnThreadOrderingGroup");
    setupPTR(pAvSetMmThreadCharacteristics,  FAvSetMmThreadCharacteristics,  "AvSetMmThreadCharacteristicsA");
    setupPTR(pAvSetMmThreadPriority,         FAvSetMmThreadPriority,         "AvSetMmThreadPriority");

    return true;
}



PaError PaWinWasapi_Initialize( PaUtilHostApiRepresentation **hostApi, PaHostApiIndex hostApiIndex )
{
    if (!setupAVRT()){
        PRINT(("Windows WASAPI : No AVRT! (not VISTA?)"));
        return paNoError;
    }

    CoInitialize(NULL);

    PaError result = paNoError;
    PaWinWasapiHostApiRepresentation *paWasapi;
    PaDeviceInfo *deviceInfoArray;

    paWasapi = (PaWinWasapiHostApiRepresentation*)PaUtil_AllocateMemory( sizeof(PaWinWasapiHostApiRepresentation) );
    if( !paWasapi ){
        result = paInsufficientMemory;
        goto error;
    }

    paWasapi->allocations = PaUtil_CreateAllocationGroup();
    if( !paWasapi->allocations ){
        result = paInsufficientMemory;
        goto error;
    }

    *hostApi = &paWasapi->inheritedHostApiRep;
    (*hostApi)->info.structVersion = 1;
    (*hostApi)->info.type = paWASAPI;
    (*hostApi)->info.name = "Windows WASAPI";
    (*hostApi)->info.deviceCount = 0;   //so far, we must investigate each
    (*hostApi)->info.defaultInputDevice  = paNoDevice;  /* IMPLEMENT ME */
    (*hostApi)->info.defaultOutputDevice = paNoDevice; /* IMPLEMENT ME */


    HRESULT hResult = S_OK;
    IMMDeviceCollection* spEndpoints=0;
    paWasapi->enumerator = 0;

    if (!setupAVRT()){
        PRINT(("Windows WASAPI : No AVRT! (not VISTA?)"));
        goto error;
    }

    hResult = CoCreateInstance(
             __uuidof(MMDeviceEnumerator), NULL,CLSCTX_INPROC_SERVER,
             __uuidof(IMMDeviceEnumerator),
             (void**)&paWasapi->enumerator);

    IF_FAILED_JUMP(hResult, error);

    //getting default device ids in the eMultimedia "role"
    {
        {
            IMMDevice* defaultRenderer=0;
            hResult = paWasapi->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultRenderer);
            IF_FAILED_JUMP(hResult, error);
            WCHAR* pszDeviceId = NULL;
            hResult = defaultRenderer->GetId(&pszDeviceId);
            IF_FAILED_JUMP(hResult, error);
            StringCchCopyW(paWasapi->defaultRenderer, MAX_STR_LEN-1, pszDeviceId);
            CoTaskMemFree(pszDeviceId);
            defaultRenderer->Release();
        }

        {
            IMMDevice* defaultCapturer=0;
            hResult = paWasapi->enumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &defaultCapturer);
            IF_FAILED_JUMP(hResult, error);
            WCHAR* pszDeviceId = NULL;
            hResult = defaultCapturer->GetId(&pszDeviceId);
            IF_FAILED_JUMP(hResult, error);
            StringCchCopyW(paWasapi->defaultCapturer, MAX_STR_LEN-1, pszDeviceId);
            CoTaskMemFree(pszDeviceId);
            defaultCapturer->Release();
        }
    }


    hResult = paWasapi->enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &spEndpoints);
    IF_FAILED_JUMP(hResult, error);

    hResult = spEndpoints->GetCount(&paWasapi->deviceCount);
    IF_FAILED_JUMP(hResult, error);

    paWasapi->devInfo = new PaWinWasapiDeviceInfo[paWasapi->deviceCount];
    {
        for (size_t step=0;step<paWasapi->deviceCount;++step)
            memset(&paWasapi->devInfo[step],0,sizeof(PaWinWasapiDeviceInfo));
    }



    if( paWasapi->deviceCount > 0 )
    {
        (*hostApi)->deviceInfos = (PaDeviceInfo**)PaUtil_GroupAllocateMemory(
                paWasapi->allocations, sizeof(PaDeviceInfo*) * paWasapi->deviceCount );
        if( !(*hostApi)->deviceInfos ){
            result = paInsufficientMemory;
            goto error;
        }

        /* allocate all device info structs in a contiguous block */
        deviceInfoArray = (PaDeviceInfo*)PaUtil_GroupAllocateMemory(
                paWasapi->allocations, sizeof(PaDeviceInfo) * paWasapi->deviceCount );
        if( !deviceInfoArray ){
            result = paInsufficientMemory;
            goto error;
        }

        for( UINT i=0; i < paWasapi->deviceCount; ++i ){

            PaDeviceInfo *deviceInfo = &deviceInfoArray[i];
            deviceInfo->structVersion = 2;
            deviceInfo->hostApi = hostApiIndex;

            hResult = spEndpoints->Item(i, &paWasapi->devInfo[i].device);
            IF_FAILED_JUMP(hResult, error);

            //getting ID
            {
                WCHAR* pszDeviceId = NULL;
                hResult = paWasapi->devInfo[i].device->GetId(&pszDeviceId);
                IF_FAILED_JUMP(hResult, error);
                StringCchCopyW(paWasapi->devInfo[i].szDeviceID, MAX_STR_LEN-1, pszDeviceId);
                CoTaskMemFree(pszDeviceId);

                if (lstrcmpW(paWasapi->devInfo[i].szDeviceID, paWasapi->defaultCapturer)==0){
                    //we found the default input!
                    (*hostApi)->info.defaultInputDevice = (*hostApi)->info.deviceCount;
                }
                if (lstrcmpW(paWasapi->devInfo[i].szDeviceID, paWasapi->defaultRenderer)==0){
                    //we found the default output!
                    (*hostApi)->info.defaultOutputDevice = (*hostApi)->info.deviceCount;
                }
            }

            DWORD state=0;
            hResult = paWasapi->devInfo[i].device->GetState(&paWasapi->devInfo[i].state);
            IF_FAILED_JUMP(hResult, error);

            if (paWasapi->devInfo[i].state != DEVICE_STATE_ACTIVE){
                PRINT(("WASAPI device:%d is not currently available (state:%d)\n",i,state));
                //spDevice->Release();
                //continue;
            }

            {
                IPropertyStore* spProperties;
                hResult = paWasapi->devInfo[i].device->OpenPropertyStore(STGM_READ, &spProperties);
                IF_FAILED_JUMP(hResult, error);

                //getting "Friendly" Name
                {
                    PROPVARIANT value;
                    PropVariantInit(&value);
                    hResult = spProperties->GetValue(PKEY_Device_FriendlyName, &value);
                    IF_FAILED_JUMP(hResult, error);
                    deviceInfo->name = 0;
                    char* deviceName = (char*)PaUtil_GroupAllocateMemory( paWasapi->allocations, MAX_STR_LEN + 1 );
                    if( !deviceName ){
                        result = paInsufficientMemory;
                        goto error;
                    }
					if (value.pwszVal)
						wcstombs(deviceName,   value.pwszVal,MAX_STR_LEN-1); //todo proper size	
					else{
						sprintf(deviceName,"baddev%d",i);
					}

                    deviceInfo->name = deviceName;
                    PropVariantClear(&value);
                }

#if 0
                DWORD numProps = 0;
                hResult = spProperties->GetCount(&numProps);
                IF_FAILED_JUMP(hResult, error);
                {
                    for (DWORD i=0;i<numProps;++i){
                        PROPERTYKEY pkey;
                        hResult = spProperties->GetAt(i,&pkey);

                        PROPVARIANT value;
                        PropVariantInit(&value);
                        hResult = spProperties->GetValue(pkey, &value);

                        switch(value.vt){
                            case 11:
                                PRINT(("property*%u*\n",value.ulVal));
                            break;
                            case 19:
                                PRINT(("property*%d*\n",value.boolVal));
                            break;
                            case 31:
                            {
                                char temp[512];
                                wcstombs(temp,    value.pwszVal,MAX_STR_LEN-1);
                                PRINT(("property*%s*\n",temp));
                            }
                            break;
                            default:break;
                        }

                        PropVariantClear(&value);
                    }
                }
#endif

                /*  These look interresting... but they are undocumented
                PKEY_AudioEndpoint_FormFactor
                PKEY_AudioEndpoint_ControlPanelPageProvider
                PKEY_AudioEndpoint_Association
                PKEY_AudioEndpoint_PhysicalSpeakerConfig
                PKEY_AudioEngine_DeviceFormat
                */
                spProperties->Release();
            }


            //getting the Endpoint data
            {
                IMMEndpoint *endpoint=0;
                hResult = paWasapi->devInfo[i].device->QueryInterface(__uuidof(IMMEndpoint),(void **)&endpoint);
                if (SUCCEEDED(hResult)){
                    hResult = endpoint->GetDataFlow(&paWasapi->devInfo[i].flow);
                    endpoint->Release();
                }
            }

            //Getting a temporary IAudioDevice for more fields
            //we make sure NOT to call Initialize yet!
            {
                IAudioClient *myClient=0;

                hResult = paWasapi->devInfo[i].device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, (void**)&myClient);
                IF_FAILED_JUMP(hResult, error);

                hResult = myClient->GetDevicePeriod(
                    &paWasapi->devInfo[i].DefaultDevicePeriod,
                    &paWasapi->devInfo[i].MinimumDevicePeriod);
                IF_FAILED_JUMP(hResult, error);

                hResult = myClient->GetMixFormat(&paWasapi->devInfo[i].MixFormat);

                IF_FAILED_JUMP(hResult, error);
                myClient->Release();
            }

            //we can now fill in portaudio device data
            deviceInfo->maxInputChannels  = 0;  //for now
            deviceInfo->maxOutputChannels = 0;  //for now

            switch(paWasapi->devInfo[i].flow){
                case eRender:
                    //hum not exaclty maximum, more like "default"
                    deviceInfo->maxOutputChannels = paWasapi->devInfo[i].MixFormat->nChannels;

                    deviceInfo->defaultHighOutputLatency = nano100ToSeconds(paWasapi->devInfo[i].DefaultDevicePeriod);
                    deviceInfo->defaultLowOutputLatency  = nano100ToSeconds(paWasapi->devInfo[i].MinimumDevicePeriod);
                break;
                case eCapture:
                    //hum not exaclty maximum, more like "default"
                    deviceInfo->maxInputChannels  = paWasapi->devInfo[i].MixFormat->nChannels;

                    deviceInfo->defaultHighInputLatency = nano100ToSeconds(paWasapi->devInfo[i].DefaultDevicePeriod);
                    deviceInfo->defaultLowInputLatency  = nano100ToSeconds(paWasapi->devInfo[i].MinimumDevicePeriod);
                break;
                default:
                    PRINT(("WASAPI device:%d bad Data FLow! \n",i));
                    goto error;
                break;
            }

            deviceInfo->defaultSampleRate = (double)paWasapi->devInfo[i].MixFormat->nSamplesPerSec;

            (*hostApi)->deviceInfos[i] = deviceInfo;
            ++(*hostApi)->info.deviceCount;
        }
    }

    spEndpoints->Release();

    (*hostApi)->Terminate = Terminate;
    (*hostApi)->OpenStream = OpenStream;
    (*hostApi)->IsFormatSupported = IsFormatSupported;

    PaUtil_InitializeStreamInterface( &paWasapi->callbackStreamInterface, CloseStream, StartStream,
                                      StopStream, AbortStream, IsStreamStopped, IsStreamActive,
                                      GetStreamTime, GetStreamCpuLoad,
                                      PaUtil_DummyRead, PaUtil_DummyWrite,
                                      PaUtil_DummyGetReadAvailable, PaUtil_DummyGetWriteAvailable );

    PaUtil_InitializeStreamInterface( &paWasapi->blockingStreamInterface, CloseStream, StartStream,
                                      StopStream, AbortStream, IsStreamStopped, IsStreamActive,
                                      GetStreamTime, PaUtil_DummyGetCpuLoad,
                                      ReadStream, WriteStream, GetStreamReadAvailable, GetStreamWriteAvailable );

    return result;

error:

    if (spEndpoints)
        spEndpoints->Release();

    if (paWasapi->enumerator)
        paWasapi->enumerator->Release();

    if( paWasapi )
    {
        if( paWasapi->allocations )
        {
            PaUtil_FreeAllAllocations( paWasapi->allocations );
            PaUtil_DestroyAllocationGroup( paWasapi->allocations );
        }

        PaUtil_FreeMemory( paWasapi );
    }
    return result;
}


static void Terminate( struct PaUtilHostApiRepresentation *hostApi )
{
    PaWinWasapiHostApiRepresentation *paWasapi = (PaWinWasapiHostApiRepresentation*)hostApi;

    paWasapi->enumerator->Release();

    for (UINT i=0;i<paWasapi->deviceCount;++i){
        PaWinWasapiDeviceInfo *info = &paWasapi->devInfo[i];

        if (info->device)
            info->device->Release();

        if (info->MixFormat)
            CoTaskMemFree(info->MixFormat);
    }
    delete [] paWasapi->devInfo;

    CoUninitialize();

    if( paWasapi->allocations ){
        PaUtil_FreeAllAllocations( paWasapi->allocations );
        PaUtil_DestroyAllocationGroup( paWasapi->allocations );
    }

    PaUtil_FreeMemory( paWasapi );
}

static void
LogWAVEFORMATEXTENSIBLE(const WAVEFORMATEXTENSIBLE &in){

    const WAVEFORMATEX *old = (WAVEFORMATEX *)&in;

	switch (old->wFormatTag){
		case WAVE_FORMAT_EXTENSIBLE:{

			PRINT(("wFormatTag=WAVE_FORMAT_EXTENSIBLE\n"));

			if (in.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT){
				PRINT(("SubFormat=KSDATAFORMAT_SUBTYPE_IEEE_FLOAT\n"));
			}
			else if (in.SubFormat == KSDATAFORMAT_SUBTYPE_PCM){
				PRINT(("SubFormat=KSDATAFORMAT_SUBTYPE_PCM\n"));
			}
			else{
				PRINT(("SubFormat=CUSTOM GUID{%d:%d:%d:%d%d%d%d%d%d%d%d}\n",	
											in.SubFormat.Data1,
											in.SubFormat.Data2,
											in.SubFormat.Data3,
											(int)in.SubFormat.Data4[0],
											(int)in.SubFormat.Data4[1],
											(int)in.SubFormat.Data4[2],
											(int)in.SubFormat.Data4[3],
											(int)in.SubFormat.Data4[4],
											(int)in.SubFormat.Data4[5],
											(int)in.SubFormat.Data4[6],
											(int)in.SubFormat.Data4[7]));
			}
			PRINT(("Samples.wValidBitsPerSample=%d\n",  in.Samples.wValidBitsPerSample));
			PRINT(("dwChannelMask=0x%X\n",in.dwChannelMask));
		}break;
		
		case WAVE_FORMAT_PCM:        PRINT(("wFormatTag=WAVE_FORMAT_PCM\n")); break;
		case WAVE_FORMAT_IEEE_FLOAT: PRINT(("wFormatTag=WAVE_FORMAT_IEEE_FLOAT\n")); break;
		default : PRINT(("wFormatTag=UNKNOWN(%d)\n",old->wFormatTag)); break;
	}

	PRINT(("nChannels      =%d\n",old->nChannels)); 
	PRINT(("nSamplesPerSec =%d\n",old->nSamplesPerSec));  
	PRINT(("nAvgBytesPerSec=%d\n",old->nAvgBytesPerSec));  
	PRINT(("nBlockAlign    =%d\n",old->nBlockAlign));  
	PRINT(("wBitsPerSample =%d\n",old->wBitsPerSample));  
	PRINT(("cbSize         =%d\n",old->cbSize));  
}



/*
 WAVEFORMATXXX is always interleaved
 */
static PaSampleFormat
waveformatToPaFormat(const WAVEFORMATEXTENSIBLE &in){

    const WAVEFORMATEX *old = (WAVEFORMATEX *)&in;

    switch (old->wFormatTag){

        case WAVE_FORMAT_EXTENSIBLE:
        {
            if (in.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT){
                if (in.Samples.wValidBitsPerSample == 32)
                    return paFloat32;
                else
                    return paCustomFormat;
            }
            else if (in.SubFormat == KSDATAFORMAT_SUBTYPE_PCM){
                switch (old->wBitsPerSample){
                    case 32: return paInt32; break;
                    case 24: return paInt24;break;
                    case  8: return paUInt8;break;
                    case 16: return paInt16;break;
                    default: return paCustomFormat;break;
                }
            }
            else
                return paCustomFormat;
        }
        break;

        case WAVE_FORMAT_IEEE_FLOAT:
            return paFloat32;
        break;

        case WAVE_FORMAT_PCM:
        {
            switch (old->wBitsPerSample){
                case 32: return paInt32; break;
                case 24: return paInt24;break;
                case  8: return paUInt8;break;
                case 16: return paInt16;break;
                default: return paCustomFormat;break;
            }
        }
        break;

        default:
            return paCustomFormat;
        break;
    }

    return paCustomFormat;
}



static PaError
waveformatFromParams(WAVEFORMATEXTENSIBLE &wav,
                          const PaStreamParameters * params,
                          double sampleRate){

    size_t bytesPerSample = 0;
    switch( params->sampleFormat & ~paNonInterleaved ){
        case paFloat32:
        case paInt32: bytesPerSample=4;break;
        case paInt16: bytesPerSample=2;break;
        case paInt24: bytesPerSample=3;break;
        case paInt8:
        case paUInt8: bytesPerSample=1;break;
        case paCustomFormat:
        default: return paSampleFormatNotSupported;break;
    }

    memset(&wav,0,sizeof(WAVEFORMATEXTENSIBLE));

    WAVEFORMATEX *old    = (WAVEFORMATEX *)&wav;
    old->nChannels       = (WORD)params->channelCount;
    old->nSamplesPerSec  = (DWORD)sampleRate;
    old->wBitsPerSample  = bytesPerSample*8;
    old->nAvgBytesPerSec = old->nSamplesPerSec * old->nChannels * bytesPerSample;
    old->nBlockAlign     = (WORD)(old->nChannels * bytesPerSample);

    //WAVEFORMATEX
    if (params->channelCount <=2 && (bytesPerSample == 2 || bytesPerSample == 1)){
        old->cbSize          = 0;
        old->wFormatTag      = WAVE_FORMAT_PCM;
    }
    //WAVEFORMATEXTENSIBLE
    else{
        old->wFormatTag = WAVE_FORMAT_EXTENSIBLE;

        old->cbSize = sizeof (WAVEFORMATEXTENSIBLE) - sizeof (WAVEFORMATEX);

        if ((params->sampleFormat & ~paNonInterleaved) == paFloat32)
            wav.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        else
            wav.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

        wav.Samples.wValidBitsPerSample = old->wBitsPerSample; //no extra padding!

        switch(params->channelCount){
            case 1:  wav.dwChannelMask = SPEAKER_FRONT_CENTER; break;
            case 2:  wav.dwChannelMask = 0x1 | 0x2; break;
            case 4:  wav.dwChannelMask = 0x1 | 0x2 | 0x10 | 0x20; break;
            case 6:  wav.dwChannelMask = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20; break;
            case 8:  wav.dwChannelMask = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80; break;
            default: wav.dwChannelMask = 0; break;
        }
    }

    return paNoError;
}


enum PaWasapiFormatAnswer {PWFA_OK,PWFA_NO,PWFA_SUGGESTED};


static PaWasapiFormatAnswer 
IsFormatSupportedInternal(IAudioClient * myClient, WAVEFORMATEXTENSIBLE &wavex){

	PaWasapiFormatAnswer answer = PWFA_OK;

    WAVEFORMATEX *closestMatch=0;
    HRESULT hResult = myClient->IsFormatSupported(
        //AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_SHAREMODE_SHARED,
        (WAVEFORMATEX*)&wavex,&closestMatch);

	if (hResult == S_OK)
		answer = PWFA_OK;
    else if (closestMatch){
        WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*)closestMatch;
		
		if (closestMatch->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
			memcpy(&wavex,closestMatch,sizeof(WAVEFORMATEXTENSIBLE));
		else
			memcpy(&wavex,closestMatch,sizeof(WAVEFORMATEX));

        CoTaskMemFree(closestMatch);
		answer = PWFA_SUGGESTED;
	
	}else if (hResult != S_OK){
		logAUDCLNT_E(hResult);
		answer = PWFA_NO;
	}

	return answer;
}


static PaError IsFormatSupported( struct PaUtilHostApiRepresentation *hostApi,
                                  const PaStreamParameters *inputParameters,
                                  const PaStreamParameters *outputParameters,
                                  double sampleRate )
{
    int inputChannelCount, outputChannelCount;
    PaSampleFormat inputSampleFormat, outputSampleFormat;

    if( inputParameters )
    {
        inputChannelCount = inputParameters->channelCount;
        inputSampleFormat = inputParameters->sampleFormat;

        /* all standard sample formats are supported by the buffer adapter,
            this implementation doesn't support any custom sample formats */
        if( inputSampleFormat & paCustomFormat )
            return paSampleFormatNotSupported;

        /* unless alternate device specification is supported, reject the use of
            paUseHostApiSpecificDeviceSpecification */

        if( inputParameters->device == paUseHostApiSpecificDeviceSpecification )
            return paInvalidDevice;

        /* check that input device can support inputChannelCount */
        if( inputChannelCount > hostApi->deviceInfos[ inputParameters->device ]->maxInputChannels )
            return paInvalidChannelCount;

        /* validate inputStreamInfo */
        if( inputParameters->hostApiSpecificStreamInfo )
            return paIncompatibleHostApiSpecificStreamInfo; /* this implementation doesn't use custom stream info */


        PaWinWasapiHostApiRepresentation *paWasapi = (PaWinWasapiHostApiRepresentation*)hostApi;

        WAVEFORMATEXTENSIBLE wavex;
        waveformatFromParams(wavex,inputParameters,sampleRate);
	
		IAudioClient *myClient=0;
		HRESULT hResult = paWasapi->devInfo[inputParameters->device].device->Activate(
			__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, (void**)&myClient);
		if (hResult != S_OK){
			logAUDCLNT_E(hResult);
			return paInvalidDevice;
		}

		PaWasapiFormatAnswer answer = IsFormatSupportedInternal(myClient,wavex);
		myClient->Release();

		switch (answer){
			case PWFA_OK: break;
			case PWFA_NO: return paSampleFormatNotSupported;
			case PWFA_SUGGESTED:
			{
				PRINT(("Suggested format:"));
				LogWAVEFORMATEXTENSIBLE(wavex);
				if (wavex.Format.nSamplesPerSec == (DWORD)sampleRate){
					//no problem its a format issue only
				}
				else{
					return paInvalidSampleRate;
				}
			}
		}


    }
    else
    {
        inputChannelCount = 0;
    }

    if( outputParameters )
    {
        outputChannelCount = outputParameters->channelCount;
        outputSampleFormat = outputParameters->sampleFormat;

        /* all standard sample formats are supported by the buffer adapter,
            this implementation doesn't support any custom sample formats */
        if( outputSampleFormat & paCustomFormat )
            return paSampleFormatNotSupported;

        /* unless alternate device specification is supported, reject the use of
            paUseHostApiSpecificDeviceSpecification */

        if( outputParameters->device == paUseHostApiSpecificDeviceSpecification )
            return paInvalidDevice;

        /* check that output device can support outputChannelCount */
        if( outputChannelCount > hostApi->deviceInfos[ outputParameters->device ]->maxOutputChannels )
            return paInvalidChannelCount;

        /* validate outputStreamInfo */
        if( outputParameters->hostApiSpecificStreamInfo )
            return paIncompatibleHostApiSpecificStreamInfo; /* this implementation doesn't use custom stream info */


        PaWinWasapiHostApiRepresentation *paWasapi = (PaWinWasapiHostApiRepresentation*)hostApi;

        WAVEFORMATEXTENSIBLE wavex;
        waveformatFromParams(wavex,outputParameters,sampleRate);
	
		IAudioClient *myClient=0;
		HRESULT hResult = paWasapi->devInfo[outputParameters->device].device->Activate(
			__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, (void**)&myClient);
		if (hResult != S_OK){
			logAUDCLNT_E(hResult);
			return paInvalidDevice;
		}

		PaWasapiFormatAnswer answer = IsFormatSupportedInternal(myClient,wavex);
		myClient->Release();

		switch (answer){
			case PWFA_OK: break;
			case PWFA_NO: return paSampleFormatNotSupported;
			case PWFA_SUGGESTED:
			{
				PRINT(("Suggested format:"));
				LogWAVEFORMATEXTENSIBLE(wavex);
				if (wavex.Format.nSamplesPerSec == (DWORD)sampleRate){
					//no problem its a format issue only
				}
				else{
					return paInvalidSampleRate;
				}
			}
		}


    }
    else
    {
        outputChannelCount = 0;
    }


    return paFormatIsSupported;
}



/* see pa_hostapi.h for a list of validity guarantees made about OpenStream parameters */

static PaError OpenStream( struct PaUtilHostApiRepresentation *hostApi,
                           PaStream** s,
                           const PaStreamParameters *inputParameters,
                           const PaStreamParameters *outputParameters,
                           double sampleRate,
                           unsigned long framesPerBuffer,
                           PaStreamFlags streamFlags,
                           PaStreamCallback *streamCallback,
                           void *userData )
{
    PaError result = paNoError;
    PaWinWasapiHostApiRepresentation *paWasapi = (PaWinWasapiHostApiRepresentation*)hostApi;
    PaWinWasapiStream *stream = 0;
    int inputChannelCount, outputChannelCount;
    PaSampleFormat inputSampleFormat, outputSampleFormat;
    PaSampleFormat hostInputSampleFormat, hostOutputSampleFormat;


    stream = (PaWinWasapiStream*)PaUtil_AllocateMemory( sizeof(PaWinWasapiStream) );
    if( !stream ){
        result = paInsufficientMemory;
        goto error;
    }

    if( inputParameters )
    {
        inputChannelCount = inputParameters->channelCount;
        inputSampleFormat = inputParameters->sampleFormat;

        /* unless alternate device specification is supported, reject the use of
            paUseHostApiSpecificDeviceSpecification */

        if( inputParameters->device == paUseHostApiSpecificDeviceSpecification )
            return paInvalidDevice;

        /* check that input device can support inputChannelCount */
        if( inputChannelCount > hostApi->deviceInfos[ inputParameters->device ]->maxInputChannels )
            return paInvalidChannelCount;

        /* validate inputStreamInfo */
        if( inputParameters->hostApiSpecificStreamInfo )
            return paIncompatibleHostApiSpecificStreamInfo; /* this implementation doesn't use custom stream info */


        PaWinWasapiDeviceInfo &info = paWasapi->devInfo[inputParameters->device];

        HRESULT hResult = info.device->Activate(
            __uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,
            (void**)&stream->in.client);

        if (hResult != S_OK)
            return paInvalidDevice;

        waveformatFromParams(stream->in.wavex,outputParameters,sampleRate);
		
		PaWasapiFormatAnswer answer = IsFormatSupportedInternal(stream->in.client,
			                                                    stream->in.wavex);

		switch (answer){
			case PWFA_OK: break;
			case PWFA_NO: return paSampleFormatNotSupported;
			case PWFA_SUGGESTED:
			{
				PRINT(("Suggested format:"));
				LogWAVEFORMATEXTENSIBLE(stream->in.wavex);
				if (stream->in.wavex.Format.nSamplesPerSec == (DWORD)sampleRate){
					//no problem its a format issue only
				}
				else{
					return paInvalidSampleRate;
				}
			}
		}

        //stream->out.period = info.DefaultDevicePeriod;
        stream->in.period = info.MinimumDevicePeriod;

        hResult = stream->in.client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            //AUDCLNT_SHAREMODE_EXCLUSIVE,
            0,  //no flags
            stream->in.period*3, //tripple buffer
            0,//stream->out.period,
            (WAVEFORMATEX*)&stream->in.wavex,
            &stream->session
            );

        if (hResult != S_OK){
            logAUDCLNT_E(hResult);
            return paInvalidDevice;
        }

        hResult = stream->in.client->GetBufferSize(&stream->in.bufferSize);
        if (hResult != S_OK)
            return paInvalidDevice;

        hResult = stream->in.client->GetStreamLatency(&stream->in.latency);
        if (hResult != S_OK)
            return paInvalidDevice;

        double periodsPerSecond = 1.0/nano100ToSeconds(stream->in.period);
        double samplesPerPeriod = (double)(stream->in.wavex.Format.nSamplesPerSec)/periodsPerSecond;

        //this is the number of samples that are required at each period
        stream->in.framesPerHostCallback = (unsigned long)samplesPerPeriod;//unrelated to channels

        /* IMPLEMENT ME - establish which  host formats are available */
        hostInputSampleFormat =
            PaUtil_SelectClosestAvailableFormat( waveformatToPaFormat(stream->in.wavex), inputSampleFormat );
	}
    else
    {
        inputChannelCount = 0;
        inputSampleFormat = hostInputSampleFormat = paInt16; /* Surpress 'uninitialised var' warnings. */
    }

    if( outputParameters )
    {
        outputChannelCount = outputParameters->channelCount;
        outputSampleFormat = outputParameters->sampleFormat;

        /* unless alternate device specification is supported, reject the use of
            paUseHostApiSpecificDeviceSpecification */

        if( outputParameters->device == paUseHostApiSpecificDeviceSpecification )
            return paInvalidDevice;

        /* check that output device can support inputChannelCount */
        if( outputChannelCount > hostApi->deviceInfos[ outputParameters->device ]->maxOutputChannels )
            return paInvalidChannelCount;

        /* validate outputStreamInfo */
        if( outputParameters->hostApiSpecificStreamInfo )
            return paIncompatibleHostApiSpecificStreamInfo; /* this implementation doesn't use custom stream info */


        PaWinWasapiDeviceInfo &info = paWasapi->devInfo[outputParameters->device];

        HRESULT hResult = info.device->Activate(
            __uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,
            (void**)&stream->out.client);

        if (hResult != S_OK)
            return paInvalidDevice;

        waveformatFromParams(stream->out.wavex,outputParameters,sampleRate);
		
		PaWasapiFormatAnswer answer = IsFormatSupportedInternal(stream->out.client,
			                                                    stream->out.wavex);

		switch (answer){
			case PWFA_OK: break;
			case PWFA_NO: return paSampleFormatNotSupported;
			case PWFA_SUGGESTED:
			{
				PRINT(("Suggested format:"));
				LogWAVEFORMATEXTENSIBLE(stream->out.wavex);
				if (stream->out.wavex.Format.nSamplesPerSec == (DWORD)sampleRate){
					//no problem its a format issue only
				}
				else{
					return paInvalidSampleRate;
				}
			}
		}

        //stream->out.period = info.DefaultDevicePeriod;
        stream->out.period = info.MinimumDevicePeriod;

        hResult = stream->out.client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            //AUDCLNT_SHAREMODE_EXCLUSIVE,
            0,  //no flags
            stream->out.period*3, //tripple buffer
            0,//stream->out.period,
            (WAVEFORMATEX*)&stream->out.wavex,
            &stream->session
            );

        if (hResult != S_OK){
            logAUDCLNT_E(hResult);
            return paInvalidDevice;
        }

        hResult = stream->out.client->GetBufferSize(&stream->out.bufferSize);
        if (hResult != S_OK)
            return paInvalidDevice;

        hResult = stream->out.client->GetStreamLatency(&stream->out.latency);
        if (hResult != S_OK)
            return paInvalidDevice;

        double periodsPerSecond = 1.0/nano100ToSeconds(stream->out.period);
        double samplesPerPeriod = (double)(stream->out.wavex.Format.nSamplesPerSec)/periodsPerSecond;

        //this is the number of samples that are required at each period
        stream->out.framesPerHostCallback = (unsigned long)samplesPerPeriod;//unrelated to channels

        /* IMPLEMENT ME - establish which  host formats are available */
        hostOutputSampleFormat =
            PaUtil_SelectClosestAvailableFormat( waveformatToPaFormat(stream->out.wavex), outputSampleFormat );
    }
    else
    {
        outputChannelCount = 0;
        outputSampleFormat = hostOutputSampleFormat = paInt16; /* Surpress 'uninitialized var' warnings. */
    }



    /*
        IMPLEMENT ME:

        ( the following two checks are taken care of by PaUtil_InitializeBufferProcessor() FIXME - checks needed? )

            - check that input device can support inputSampleFormat, or that
                we have the capability to convert from outputSampleFormat to
                a native format

            - check that output device can support outputSampleFormat, or that
                we have the capability to convert from outputSampleFormat to
                a native format

            - if a full duplex stream is requested, check that the combination
                of input and output parameters is supported

            - check that the device supports sampleRate

            - alter sampleRate to a close allowable rate if possible / necessary

            - validate suggestedInputLatency and suggestedOutputLatency parameters,
                use default values where necessary
    */



    /* validate platform specific flags */
    if( (streamFlags & paPlatformSpecificFlags) != 0 )
        return paInvalidFlag; /* unexpected platform specific flag */



    if( streamCallback )
    {
        PaUtil_InitializeStreamRepresentation( &stream->streamRepresentation,
                                               &paWasapi->callbackStreamInterface, streamCallback, userData );
    }
    else
    {
        PaUtil_InitializeStreamRepresentation( &stream->streamRepresentation,
                                               &paWasapi->blockingStreamInterface, streamCallback, userData );
    }

    PaUtil_InitializeCpuLoadMeasurer( &stream->cpuLoadMeasurer, sampleRate );


	if (outputParameters && inputParameters){

		//serious problem #1
		if (stream->in.period != stream->out.period){
			PRINT(("OpenStream: period discrepancy\n"));
			goto error;
		}

		//serious problem #2
		if (stream->out.framesPerHostCallback != stream->in.framesPerHostCallback){
			PRINT(("OpenStream: framesPerHostCallback discrepancy\n"));
			goto error;
		}
	}

	unsigned long framesPerHostCallback = (outputParameters)?
		stream->out.framesPerHostCallback: 
		stream->in.framesPerHostCallback;

    /* we assume a fixed host buffer size in this example, but the buffer processor
        can also support bounded and unknown host buffer sizes by passing
        paUtilBoundedHostBufferSize or paUtilUnknownHostBufferSize instead of
        paUtilFixedHostBufferSize below. */

    result =  PaUtil_InitializeBufferProcessor( &stream->bufferProcessor,
              inputChannelCount, inputSampleFormat, hostInputSampleFormat,
              outputChannelCount, outputSampleFormat, hostOutputSampleFormat,
              sampleRate, streamFlags, framesPerBuffer,
              framesPerHostCallback, paUtilFixedHostBufferSize,
              streamCallback, userData );
    if( result != paNoError )
        goto error;


    /*
        IMPLEMENT ME: initialise the following fields with estimated or actual
        values.
    */
    stream->streamRepresentation.streamInfo.inputLatency =
            PaUtil_GetBufferProcessorInputLatency(&stream->bufferProcessor)
			+ ((inputParameters)?nano100ToSeconds(stream->in.latency) :0);

    stream->streamRepresentation.streamInfo.outputLatency =
            PaUtil_GetBufferProcessorOutputLatency(&stream->bufferProcessor)
			+ ((outputParameters)?nano100ToSeconds(stream->out.latency) :0);

    stream->streamRepresentation.streamInfo.sampleRate = sampleRate;


    *s = (PaStream*)stream;


    return result;

error:
    if( stream )
        PaUtil_FreeMemory( stream );

    return result;
}



/*
    When CloseStream() is called, the multi-api layer ensures that
    the stream has already been stopped or aborted.
*/

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

static PaError CloseStream( PaStream* s )
{
    PaError result = paNoError;
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /*
        IMPLEMENT ME:
            - additional stream closing + cleanup
    */

    SAFE_RELEASE(stream->out.client);
    SAFE_RELEASE(stream->in.client);
    SAFE_RELEASE(stream->cclient);
    SAFE_RELEASE(stream->rclient);
    CloseHandle(stream->hThread);

    PaUtil_TerminateBufferProcessor( &stream->bufferProcessor );
    PaUtil_TerminateStreamRepresentation( &stream->streamRepresentation );
    PaUtil_FreeMemory( stream );

    return result;
}

VOID ProcThread(void *client);

static PaError StartStream( PaStream *s )
{
    PaError result = paNoError;
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    PaUtil_ResetBufferProcessor( &stream->bufferProcessor );
	
	HRESULT hResult=S_OK;

	if (stream->out.client){
		hResult = stream->out.client->GetService(__uuidof(IAudioRenderClient),(void**)&stream->rclient);
		logAUDCLNT_E(hResult);
		if (hResult!=S_OK)
			return paUnanticipatedHostError;
	}
	
	if (stream->in.client){
	 hResult = stream->in.client->GetService(__uuidof(IAudioCaptureClient),(void**)&stream->cclient);
		logAUDCLNT_E(hResult);
		if (hResult!=S_OK)
			return paUnanticipatedHostError;
	}

    // Create a thread for this client.
    stream->hThread = CreateThread(
        NULL,              // no security attribute
        0,                 // default stack size
        (LPTHREAD_START_ROUTINE) ProcThread,
        (LPVOID) stream,    // thread parameter
        0,                 // not suspended
        &stream->dwThreadId);      // returns thread ID

    if (stream->hThread == NULL)
        return paUnanticipatedHostError;

    return paNoError;
}


static PaError StopStream( PaStream *s )
{
    PaError result = paNoError;
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /* suppress unused variable warnings */
    stream->closeRequest = true;
    //todo something MUCH better than this
    while(stream->closeRequest)
        Sleep(100);

    /* IMPLEMENT ME, see portaudio.h for required behavior */

    stream->running = false;

    return result;
}


static PaError AbortStream( PaStream *s )
{
    PaError result = paNoError;
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /* suppress unused variable warnings */
    stream->closeRequest = true;
    //todo something MUCH better than this
    while(stream->closeRequest)
        Sleep(100);

    /* IMPLEMENT ME, see portaudio.h for required behavior */

    return result;
}


static PaError IsStreamStopped( PaStream *s )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    return !stream->running;
}


static PaError IsStreamActive( PaStream *s )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;
    return stream->running;
}


static PaTime GetStreamTime( PaStream *s )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /* suppress unused variable warnings */
    (void) stream;

    /* IMPLEMENT ME, see portaudio.h for required behavior*/

	//this is lame ds and mme does the same thing, quite useless method imho
	//why dont we fetch the time in the pa callbacks?
	//at least its doing to be clocked to something
    return PaUtil_GetTime();
}


static double GetStreamCpuLoad( PaStream* s )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    return PaUtil_GetCpuLoad( &stream->cpuLoadMeasurer );
}


/*
    As separate stream interfaces are used for blocking and callback
    streams, the following functions can be guaranteed to only be called
    for blocking streams.
*/

static PaError ReadStream( PaStream* s,
                           void *buffer,
                           unsigned long frames )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /* suppress unused variable warnings */
    (void) buffer;
    (void) frames;
    (void) stream;

    /* IMPLEMENT ME, see portaudio.h for required behavior*/

    return paNoError;
}


static PaError WriteStream( PaStream* s,
                            const void *buffer,
                            unsigned long frames )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /* suppress unused variable warnings */
    (void) buffer;
    (void) frames;
    (void) stream;

    /* IMPLEMENT ME, see portaudio.h for required behavior*/

    return paNoError;
}


static signed long GetStreamReadAvailable( PaStream* s )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /* suppress unused variable warnings */
    (void) stream;

    /* IMPLEMENT ME, see portaudio.h for required behavior*/

    return 0;
}


static signed long GetStreamWriteAvailable( PaStream* s )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)s;

    /* suppress unused variable warnings */
    (void) stream;

    /* IMPLEMENT ME, see portaudio.h for required behavior*/

    return 0;
}



/*
    ExampleHostProcessingLoop() illustrates the kind of processing which may
    occur in a host implementation.

*/
static void WaspiHostProcessingLoop( void *inputBuffer,  long inputFrames,
                                     void *outputBuffer, long outputFrames,
                                     void *userData )
{
    PaWinWasapiStream *stream = (PaWinWasapiStream*)userData;
    PaStreamCallbackTimeInfo timeInfo = {0,0,0}; /* IMPLEMENT ME */
    int callbackResult;
    unsigned long framesProcessed;

    PaUtil_BeginCpuLoadMeasurement( &stream->cpuLoadMeasurer );


    /*
        IMPLEMENT ME:
            - generate timing information
            - handle buffer slips
    */

    /*
        If you need to byte swap or shift inputBuffer to convert it into a
        portaudio format, do it here.
    */



    PaUtil_BeginBufferProcessing( &stream->bufferProcessor, &timeInfo, 0 /* IMPLEMENT ME: pass underflow/overflow flags when necessary */ );

    /*
        depending on whether the host buffers are interleaved, non-interleaved
        or a mixture, you will want to call PaUtil_SetInterleaved*Channels(),
        PaUtil_SetNonInterleaved*Channel() or PaUtil_Set*Channel() here.
    */

    if( stream->bufferProcessor.inputChannelCount > 0 )
    {
        PaUtil_SetInputFrameCount( &stream->bufferProcessor, inputFrames );
        PaUtil_SetInterleavedInputChannels( &stream->bufferProcessor,
            0, /* first channel of inputBuffer is channel 0 */
            inputBuffer,
            0 ); /* 0 - use inputChannelCount passed to init buffer processor */
    }

    if( stream->bufferProcessor.outputChannelCount > 0 )
    {
        PaUtil_SetOutputFrameCount( &stream->bufferProcessor, outputFrames);
        PaUtil_SetInterleavedOutputChannels( &stream->bufferProcessor,
            0, /* first channel of outputBuffer is channel 0 */
            outputBuffer,
            0 ); /* 0 - use outputChannelCount passed to init buffer processor */
    }

    /* you must pass a valid value of callback result to PaUtil_EndBufferProcessing()
        in general you would pass paContinue for normal operation, and
        paComplete to drain the buffer processor's internal output buffer.
        You can check whether the buffer processor's output buffer is empty
        using PaUtil_IsBufferProcessorOuputEmpty( bufferProcessor )
    */
    callbackResult = paContinue;
    framesProcessed = PaUtil_EndBufferProcessing( &stream->bufferProcessor, &callbackResult );


    /*
        If you need to byte swap or shift outputBuffer to convert it to
        host format, do it here.
    */

    PaUtil_EndCpuLoadMeasurement( &stream->cpuLoadMeasurer, framesProcessed );


    if( callbackResult == paContinue )
    {
        /* nothing special to do */
    }
    else if( callbackResult == paAbort )
    {
        /* IMPLEMENT ME - finish playback immediately  */

        /* once finished, call the finished callback */
        if( stream->streamRepresentation.streamFinishedCallback != 0 )
            stream->streamRepresentation.streamFinishedCallback( stream->streamRepresentation.userData );
    }
    else
    {
        /* User callback has asked us to stop with paComplete or other non-zero value */

        /* IMPLEMENT ME - finish playback once currently queued audio has completed  */

        /* once finished, call the finished callback */
        if( stream->streamRepresentation.streamFinishedCallback != 0 )
            stream->streamRepresentation.streamFinishedCallback( stream->streamRepresentation.userData );
    }
}



VOID
ProcThread(void *param){

	HRESULT hResult;

    DWORD stuff=0;
    HANDLE thCarac = pAvSetMmThreadCharacteristics("Pro Audio",&stuff);
    if (!thCarac){
        PRINT(("AvSetMmThreadCharacteristics failed!\n"));
    }

    BOOL prio = pAvSetMmThreadPriority(thCarac,AVRT_PRIORITY_NORMAL);
    if (!prio){
        PRINT(("AvSetMmThreadPriority failed!\n"));
    }


    PaWinWasapiStream *stream = (PaWinWasapiStream*)param;

    HANDLE context;
    GUID threadOrderGUID;
    memset(&threadOrderGUID,0,sizeof(GUID));
    LARGE_INTEGER large;

    large.QuadPart = stream->out.period;

    BOOL ok = pAvRtCreateThreadOrderingGroup(&context,
        &large,
        &threadOrderGUID,
#ifdef _DEBUG
        0 //THREAD_ORDER_GROUP_INFINITE_TIMEOUT
#else
        0 //default is 5 times the 2nd param
#endif
        //TEXT("Audio")
        );

    if (!ok){
        PRINT(("AvRtCreateThreadOrderingGroup failed!\n"));
    }

	//debug
    {
        HANDLE hh       = GetCurrentThread();
        int  currprio   = GetThreadPriority(hh);
        DWORD currclass = GetPriorityClass(GetCurrentProcess());
        PRINT(("currprio 0x%X currclass 0x%X\n",currprio,currclass));
    }


    //fill up initial buffer latency??

	if (stream->out.client){
		hResult = stream->out.client->Start();
		if (hResult != S_OK)
			logAUDCLNT_E(hResult);
	}

    stream->running = true;

    while(!stream->closeRequest){
        BOOL answer = pAvRtWaitOnThreadOrderingGroup(context);
        if (!answer){
            PRINT(("AvRtWaitOnThreadOrderingGroup failed\n"));
        }

        unsigned long usingBS = stream->out.framesPerHostCallback;

        UINT32 padding=0;
        hResult = stream->out.client->GetCurrentPadding(&padding);
        logAUDCLNT_E(hResult);

        //buffer full dont pursue
        if (padding == stream->out.bufferSize)
            continue;

        //if something is already inside
        if (padding > 0){
            usingBS = stream->out.bufferSize-padding;
            if (usingBS > stream->out.framesPerHostCallback){
                //PRINT(("underflow! %d\n",usingBS));
            }
            else if (usingBS < stream->out.framesPerHostCallback){
                //PRINT(("overflow! %d\n",usingBS));
            }
        }
        else
            usingBS = stream->out.framesPerHostCallback;


        BYTE*indata =0;
        BYTE*outdata=0;

        hResult = stream->rclient->GetBuffer(usingBS,&outdata);

        if (hResult != S_OK || !outdata) {
            logAUDCLNT_E(hResult);
			continue;
        }

        WaspiHostProcessingLoop(indata, usingBS
			                   ,outdata,usingBS,stream);

        hResult = stream->rclient->ReleaseBuffer(usingBS,0);
        if (hResult != S_OK)
            logAUDCLNT_E(hResult);

    }


    BOOL bRes = pAvRtDeleteThreadOrderingGroup(context);
    if (!bRes){
        PRINT(("AvRtDeleteThreadOrderingGroup failure\n"));
    }

    stream->closeRequest = false;
}




#endif //VC 2005