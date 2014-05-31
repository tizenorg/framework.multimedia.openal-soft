/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include "alMain.h"
#include "AL/al.h"
#include "AL/alc.h"

#include <avsystem.h>
#include <avsys-audio.h>

#ifndef SOUND_MIXER_READ
#define SOUND_MIXER_READ MIXER_READ
#endif
#ifndef SOUND_MIXER_WRITE
#define SOUND_MIXER_WRITE MIXER_WRITE
#endif

//for debug
#define LOG_LEVEL_2
#if defined(LOG_LEVEL_0)
#define func_in()
#define func_out()
#elif defined(LOG_LEVEL_1)
#define func_in()				fprintf(stderr,"<< %s\n", __FUNCTION__)
#define func_out()				fprintf(stderr, ">> %s\n", __FUNCTION__)
#elif defined(LOG_LEVEL_2)
#include <dlog.h>
#define LOG_TAG	"MM_OPENAL"
#define func_in() SLOG(LOG_VERBOSE, LOG_TAG, "<< %s\n", __FUNCTION__)
#define func_out() SLOG(LOG_VERBOSE, LOG_TAG, ">> %s\n", __FUNCTION__)
#endif

static const ALCchar avsystem_Device[] = "avsystem";

typedef struct {
	avsys_audio_param_t		param;
	avsys_handle_t			handle;
	int						data_size;
	ALubyte					*mix_data;
	volatile int			killNow;
	ALboolean				doCapture;
	ALvoid					*thread;
} avsys_data;


static ALuint AvsysProc(ALvoid *ptr)
{
	ALCdevice *pDevice = (ALCdevice*)ptr;
	avsys_data *data;
	ALint remaining = 0;
	ALint wrote;
	ALint lenByte, lenSample;

	ALvoid* WritePtr = NULL;

	func_in();
	if (ptr == NULL)
	{
		AL_PRINT("__thread_input_parameter_is_null\n");
		return 1;
	}

	data = (avsys_data*)pDevice->ExtraData;
	if (data == NULL)
	{
		AL_PRINT("__thread_input_parameter_is_null\n");
		return 1;
	}
	if (data->mix_data == NULL)
	{
		AL_PRINT("____mix_data is null\n");
		return 1;
	}

	while(!data->killNow && pDevice->Connected)
	{
		lenByte = data->data_size;
		lenSample = lenByte / FrameSizeFromDevFmt(pDevice->FmtChans, pDevice->FmtType);
		WritePtr = data->mix_data;

		aluMixData(pDevice, data->mix_data, lenSample);

		while(lenByte > 0 && !data->killNow)
		{
			wrote = avsys_audio_write(data->handle, WritePtr, lenByte);
			if(wrote < 0)
			{
				lenByte =0;
				aluHandleDisconnect(pDevice);
				AL_PRINT("aluHandleDisconnect %d\n", data->handle);
			}
			lenByte -= wrote;
			WritePtr += wrote;
		}
	}

	func_out();
	return 0;
}

static ALuint AvsysCaptureProc(ALvoid *ptr)
{
    (void)ptr;
    return 0;
}

static ALCboolean avsystem_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
	avsys_data *data = NULL;
	int BufferSize =0;
	int	result = ALC_TRUE;

	func_in();

	if(deviceName)
	{
		if(strcmp(deviceName, avsystem_Device) != 0)
			return ALC_FALSE;
		device->szDeviceName = strdup(avsystem_Device);
	}
	else
		device->szDeviceName = strdup(avsystem_Device);

	data = (avsys_data*)calloc(1, sizeof(avsys_data));
	if(data == NULL)
		goto error;

	data->param.mode = AVSYS_AUDIO_MODE_OUTPUT_LOW_LATENCY;
	data->param.priority = AVSYS_AUDIO_PRIORITY_0;
	data->param.channels = ChannelsFromDevFmt(device->FmtChans);;
	data->param.samplerate = device->Frequency;
	data->param.handle_route = 0;
	data->param.vol_type = AVSYS_AUDIO_VOLUME_TYPE_MEDIA;

	switch(device->FmtType)
	{
		case DevFmtByte:
	case DevFmtUByte:
		data->param.format = AVSYS_AUDIO_FORMAT_8BIT;
		break;
	case DevFmtUShort:
		device->FmtType = DevFmtShort;
		/* fall-through */
	case DevFmtShort:
		data->param.format = AVSYS_AUDIO_FORMAT_16BIT;
		break;
#if defined(FORMAT_32)
	case DevFmtFloat:
		data->param.format = AVSYS_AUDIO_FORMAT_32BIT;
		break;
#endif
	default :
		AL_PRINT("Not supported format\n");
		break;
	}

	if(AVSYS_STATE_SUCCESS != avsys_audio_open(&data->param, &data->handle, &BufferSize))
	{
		AL_PRINT("avsys_audio_open() failed\n");
		goto error;
	}
	data->data_size = BufferSize;
	device->ExtraData = data;

	AL_PRINT("avsystem openplayback:%x %x %d %d %d\n", device, data, data->killNow, device->Connected, data->handle);

	func_out();
	return ALC_TRUE;

error:
	if(data != NULL)
	{
		if(data->handle)
		{
			avsys_audio_close(data->handle);
		}
		free(data);
		data = NULL;
	}

	return ALC_FALSE;

}

static ALCboolean avsystem_reset_playback(ALCdevice *device)
{
	avsys_data *data = NULL;
	avsys_audio_volume_t avsys_volume;
	int loaded_volume = 9;
	func_in();
	if(device == NULL)
	{
		AL_PRINT("input parameter is null [%d][%s]\n", __LINE__,__func__);
		return ALC_FALSE;
	}
	if(device->ExtraData == NULL)
	{
		AL_PRINT("input parameter is null [%d][%s]\n", __LINE__,__func__);
		return ALC_FALSE;
	}
	data = (avsys_data*)device->ExtraData;

	data->mix_data = calloc(1, data->data_size);
	if(data->mix_data == NULL)
	{
		AL_PRINT("Memory Allocation failed\n");
		return ALC_FALSE;
	}
	device->UpdateSize = data->data_size /FrameSizeFromDevFmt(device->FmtChans, device->FmtType);
	SetDefaultChannelOrder(device);

	data->thread = StartThread(AvsysProc, device);
	if(data->thread == NULL)
	{
		AL_PRINT("Could not create playback thread\n");
		return ALC_FALSE;
	}
	func_out();
	return ALC_TRUE;
}

static void avsystem_stop_playback(ALCdevice *device)
{
	avsys_data *data = (avsys_data*)device->ExtraData;
	func_in();
	if(data->thread)
	{
		data->killNow = 1;
		StopThread(data->thread);
		AL_PRINT("Thread Stopped\n");
		data->thread = NULL;
	}

	free(data->mix_data);
	data->mix_data = NULL;

	func_out();
}

static void avsystem_close_playback(ALCdevice *device)
{
	avsys_data *data = (avsys_data*)device->ExtraData;

	func_in();

	avsys_audio_drain(data->handle);
	avsys_audio_close(data->handle);

	free(data);
	device->ExtraData = NULL;

	func_out();
}

static ALCboolean avsystem_open_capture(ALCdevice *device, const ALCchar *deviceName, ALCuint frequency, ALCenum format, ALCsizei SampleSize)
{
	func_in();
	(void)device;
	(void)deviceName;
	func_out();
	return ALC_FALSE;
}

static void avsystem_close_capture(ALCdevice *device)
{
	func_in();
	(void)device;
	func_out();
}

static void avsystem_start_capture(ALCdevice *pDevice)
{
	func_in();
	(void)pDevice;
	func_out();
}

static void avsystem_stop_capture(ALCdevice *pDevice)
{
	func_in();
	(void)pDevice;
	func_out();
}

static void avsystem_capture_samples(ALCdevice *pDevice, ALCvoid *pBuffer, ALCuint lSamples)
{
	func_in();
	(void)pDevice;
	(void)pBuffer;
	(void)lSamples;
	func_out();
}

static ALCuint avsystem_available_samples(ALCdevice *pDevice)
{
	func_in();
	(void)pDevice;
	return 0;
	func_out();
}

BackendFuncs avsys_funcs = {
    avsystem_open_playback,
    avsystem_close_playback,
    avsystem_reset_playback,
    avsystem_stop_playback,
    avsystem_open_capture,
    avsystem_close_capture,
    avsystem_start_capture,
    avsystem_stop_capture,
    avsystem_capture_samples,
    avsystem_available_samples,
};

void alc_avsystem_init(BackendFuncs *func_list)
{
	func_in();
	*func_list = avsys_funcs;
	func_out();
}

void alc_avsystem_deinit(void)
{
	func_in();
	func_out();
}

void alc_avsystem_probe(int type)
{
	if(type == DEVICE_PROBE)
		AppendDeviceList(avsystem_Device);
	else if(type == ALL_DEVICE_PROBE)
		AppendAllDeviceList(avsystem_Device);
	else if(type == CAPTURE_DEVICE_PROBE)
		AppendCaptureDeviceList(avsystem_Device);

}

ALCboolean alcDeviceSuspend_avsystem(ALCdevice *device)
{
	int errorcode;

	func_in();

	if (device == NULL)
		return ALC_FALSE;

	avsys_data *data = (avsys_data*)device->ExtraData;
	if (data == NULL)
		return ALC_FALSE;

	AL_PRINT("avsys: %x %x %d %d %d\n", device, data, data->thread, data->mix_data, data->handle);

	if(data->thread)
	{
		data->killNow = 1;
		StopThread(data->thread);
		AL_PRINT("Thread Stopped\n");
		data->thread = NULL;
	}

	if (data->mix_data)
	{
		free(data->mix_data);
		data->mix_data = NULL;
	}

	if (data->handle != NULL) {
		avsys_audio_drain(data->handle);
		avsys_audio_close(data->handle);
		AL_PRINT("avsys_audio_close:%d", data->handle);
		data->handle = NULL;
	}

	func_out();

	return ALC_TRUE;
}

ALCboolean alcDeviceResume_avsystem(ALCdevice *device)
{
	ALCboolean ret = ALC_TRUE;
	int BufferSize = 0;

	func_in();
	if (device == NULL) {
		AL_PRINT("alc_device_resume_avsystem() failed:device is null\n");
		return ALC_FALSE;
	}

	avsys_data *data = (avsys_data*)device->ExtraData;
	if (data == NULL) {
	AL_PRINT("alc_device_resume_avsystem() failed:data is null\n");
	return ALC_FALSE;
	}

	if(AVSYS_STATE_SUCCESS != avsys_audio_open(&data->param, &data->handle, &BufferSize))
	{
		AL_PRINT("avsys_audio_open() failed\n");
		return ALC_FALSE;
	}
	data->data_size = BufferSize;
	data->killNow = 0;

	ret = avsystem_reset_playback(device);
	if (ret == ALC_FALSE)
	{
		AL_PRINT("avsystem_reset_playback() failed\n");
		return ALC_FALSE;
	}

	func_out();

	return ALC_TRUE;

}
