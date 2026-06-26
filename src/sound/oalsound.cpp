#include "doomtype.h"

#ifndef NO_SOUND

#include "oalsound.h"

#include <vector>
#include <string.h>
#include <math.h>

#include <al.h>
#include <alc.h>

#include "c_cvars.h"
#include "i_system.h"
#include "v_text.h"

EXTERN_CVAR(Bool, snd_pitched)

#define OAL_PITCH(pitch) (snd_pitched ? ((pitch) / 128.f) : 1.f)

class OpenALSoundStream;
static std::vector<OpenALSoundStream *> ActiveStreams;

namespace
{
	const int STREAM_BUFFER_COUNT = 8;

	struct OpenALSample
	{
		ALuint Buffer;
		int Frequency;
		int Channels;
		int Bits;
		unsigned int Samples;
	};

	struct OpenALChannel
	{
		ALuint Source;
		FISoundChannel *GameChannel;
		float Volume;
		int Flags;
	};

	ALenum FormatFor(int channels, int bits)
	{
		if (channels == 1 && (bits == 8 || bits == -8))
			return AL_FORMAT_MONO8;
		if (channels == 1 && bits == 16)
			return AL_FORMAT_MONO16;
		if (channels == 2 && (bits == 8 || bits == -8))
			return AL_FORMAT_STEREO8;
		if (channels == 2 && bits == 16)
			return AL_FORMAT_STEREO16;
		return 0;
	}

	ALenum StreamFormatFor(int flags)
	{
		const bool mono = !!(flags & SoundStream::Mono);
		if (mono)
			return AL_FORMAT_MONO16;
		return AL_FORMAT_STEREO16;
	}

	int StreamChannelsFor(int flags)
	{
		return (flags & SoundStream::Mono) ? 1 : 2;
	}

	short ClampFloatSample(float sample)
	{
		if (sample < -1.f)
			sample = -1.f;
		else if (sample > 1.f)
			sample = 1.f;
		return static_cast<short>(sample * 32767.f);
	}

	void ConvertStreamTo16(const void *src, int bytes, int flags, std::vector<short> &dst)
	{
		if (flags & SoundStream::Float)
		{
			const int count = bytes / static_cast<int>(sizeof(float));
			const float *in = static_cast<const float *>(src);
			dst.resize(count);
			for (int i = 0; i < count; ++i)
			{
				dst[i] = ClampFloatSample(in[i]);
			}
		}
		else if (flags & SoundStream::Bits32)
		{
			const int count = bytes / static_cast<int>(sizeof(int));
			const int *in = static_cast<const int *>(src);
			dst.resize(count);
			for (int i = 0; i < count; ++i)
			{
				dst[i] = static_cast<short>(in[i] >> 16);
			}
		}
		else if (flags & SoundStream::Bits8)
		{
			const int count = bytes;
			const BYTE *in = static_cast<const BYTE *>(src);
			dst.resize(count);
			for (int i = 0; i < count; ++i)
			{
				dst[i] = static_cast<short>((static_cast<int>(in[i]) - 128) << 8);
			}
		}
		else
		{
			const int count = bytes / static_cast<int>(sizeof(short));
			const short *in = static_cast<const short *>(src);
			dst.assign(in, in + count);
		}
	}

	float ChannelGain(float volume, int flags, float sfxVolume, bool muted)
	{
		const bool noPauseSound = (flags & SNDF_NOPAUSE) != 0;
		return volume * sfxVolume * ((muted && !noPauseSound) ? 0.f : 1.f);
	}

	void ClearALErrors()
	{
		while (alGetError() != AL_NO_ERROR)
		{
		}
	}

	void ResumeIfPaused(ALuint source)
	{
		ALint state = AL_INITIAL;
		alGetSourcei(source, AL_SOURCE_STATE, &state);
		if (state == AL_PAUSED)
		{
			alSourcePlay(source);
		}
	}

	float OpenALMusicVolume = 1.f;
}

struct OpenALSoundRenderer::Impl
{
	ALCdevice *Device;
	ALCcontext *Context;
	FString DeviceName;
	float SfxVolume;
	float MusicVolume;
	int SfxPaused;
	bool Muted;
	bool Valid;
	std::vector<OpenALChannel *> Channels;

	Impl()
		: Device(NULL), Context(NULL), SfxVolume(1.f), MusicVolume(1.f),
		  SfxPaused(0), Muted(false), Valid(false)
	{
	}
};

class OpenALSoundStream : public SoundStream
{
public:
	OpenALSoundStream(SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata)
		: Callback(callback), UserData(userdata), BufferBytes(buffbytes), Flags(flags),
		  SampleRate(samplerate), Channels(StreamChannelsFor(flags)), Format(StreamFormatFor(flags)),
		  Source(0), Playing(false), Looping(false), Ended(false), Paused(false),
		  Volume(1.f), SamplesQueued(0), SamplesProcessed(0)
	{
		alGenSources(1, &Source);
		alGenBuffers(STREAM_BUFFER_COUNT, Buffers);
		RawBuffer.resize(BufferBytes);
		ActiveStreams.push_back(this);
	}

	~OpenALSoundStream()
	{
		for (size_t i = 0; i < ActiveStreams.size(); ++i)
		{
			if (ActiveStreams[i] == this)
			{
				ActiveStreams.erase(ActiveStreams.begin() + i);
				break;
			}
		}
		Stop();
		if (Source != 0)
		{
			alDeleteSources(1, &Source);
		}
		alDeleteBuffers(STREAM_BUFFER_COUNT, Buffers);
	}

	bool Play(bool looping, float volume)
	{
		Stop();
		Looping = looping;
		Ended = false;
		Paused = false;
		Playing = true;
		Volume = volume;
		SamplesQueued = 0;
		SamplesProcessed = 0;
		ApplyVolume();
		alSourcei(Source, AL_SOURCE_RELATIVE, AL_TRUE);

		int queued = 0;
		for (int i = 0; i < STREAM_BUFFER_COUNT; ++i)
		{
			if (FillBuffer(Buffers[i]))
			{
				alSourceQueueBuffers(Source, 1, &Buffers[i]);
				queued++;
			}
		}

		if (queued == 0)
		{
			Playing = false;
			Ended = true;
			return false;
		}

		alSourcePlay(Source);
		return alGetError() == AL_NO_ERROR;
	}

	void Stop()
	{
		if (Source == 0)
		{
			return;
		}

		alSourceStop(Source);
		UnqueueProcessed(true);
		ALint queued = 0;
		alGetSourcei(Source, AL_BUFFERS_QUEUED, &queued);
		while (queued-- > 0)
		{
			ALuint buffer = 0;
			alSourceUnqueueBuffers(Source, 1, &buffer);
		}
		Playing = false;
		Paused = false;
	}

	void SetVolume(float volume)
	{
		Volume = volume;
		ApplyVolume();
	}

	void MusicVolumeChanged()
	{
		ApplyVolume();
	}

	bool SetPaused(bool paused)
	{
		Paused = paused;
		if (!Playing)
		{
			return false;
		}
		if (paused)
		{
			alSourcePause(Source);
		}
		else
		{
			alSourcePlay(Source);
		}
		return true;
	}

	unsigned int GetPosition()
	{
		Pump();
		ALint offset = 0;
		alGetSourcei(Source, AL_SAMPLE_OFFSET, &offset);
		const unsigned int samples = SamplesProcessed + (offset > 0 ? offset : 0);
		return SampleRate > 0 ? samples * 1000 / SampleRate : 0;
	}

	bool IsEnded()
	{
		Pump();
		return Ended;
	}

	FString GetStats()
	{
		FString stats;
		ALint queued = 0;
		ALint processed = 0;
		ALint state = AL_STOPPED;
		alGetSourcei(Source, AL_BUFFERS_QUEUED, &queued);
		alGetSourcei(Source, AL_BUFFERS_PROCESSED, &processed);
		alGetSourcei(Source, AL_SOURCE_STATE, &state);
		stats.Format("OpenAL stream: %d queued, %d processed, %s", queued, processed,
			state == AL_PLAYING ? "playing" : state == AL_PAUSED ? "paused" : "stopped");
		return stats;
	}

	void Pump()
	{
		if (!Playing)
		{
			return;
		}

		UnqueueProcessed(false);

		ALint queued = 0;
		ALint state = AL_STOPPED;
		alGetSourcei(Source, AL_BUFFERS_QUEUED, &queued);
		alGetSourcei(Source, AL_SOURCE_STATE, &state);

		if (queued == 0)
		{
			Playing = false;
			Ended = true;
			return;
		}

		if (!Paused && state != AL_PLAYING)
		{
			alSourcePlay(Source);
		}
	}

private:
	void ApplyVolume()
	{
		alSourcef(Source, AL_GAIN, Volume * OpenALMusicVolume);
	}

	bool FillBuffer(ALuint buffer)
	{
		if (RawBuffer.empty())
		{
			return false;
		}

		bool ok = Callback(this, &RawBuffer[0], BufferBytes, UserData);
		if (!ok && Looping)
		{
			ok = Callback(this, &RawBuffer[0], BufferBytes, UserData);
		}
		if (!ok)
		{
			if (!(Flags & SoundStream::Float))
			{
				return false;
			}
			memset(&RawBuffer[0], 0, BufferBytes);
		}

		ConvertStreamTo16(&RawBuffer[0], BufferBytes, Flags, ConvertedBuffer);
		if (ConvertedBuffer.empty())
		{
			return false;
		}
		alBufferData(buffer, Format, &ConvertedBuffer[0],
			static_cast<ALsizei>(ConvertedBuffer.size() * sizeof(short)), SampleRate);
		SamplesQueued += static_cast<unsigned int>(ConvertedBuffer.size() / Channels);
		return alGetError() == AL_NO_ERROR;
	}

	void UnqueueProcessed(bool all)
	{
		ALint processed = 0;
		if (all)
		{
			alGetSourcei(Source, AL_BUFFERS_QUEUED, &processed);
		}
		else
		{
			alGetSourcei(Source, AL_BUFFERS_PROCESSED, &processed);
		}

		while (processed-- > 0)
		{
			ALuint buffer = 0;
			ALint size = 0;
			ALint bits = 0;
			ALint channels = 0;
			alSourceUnqueueBuffers(Source, 1, &buffer);
			alGetBufferi(buffer, AL_SIZE, &size);
			alGetBufferi(buffer, AL_BITS, &bits);
			alGetBufferi(buffer, AL_CHANNELS, &channels);
			if (bits > 0 && channels > 0)
			{
				SamplesProcessed += size / (channels * (bits / 8));
			}
			if (!all && !Ended && FillBuffer(buffer))
			{
				alSourceQueueBuffers(Source, 1, &buffer);
			}
		}
	}

	SoundStreamCallback Callback;
	void *UserData;
	int BufferBytes;
	int Flags;
	int SampleRate;
	int Channels;
	ALenum Format;
	ALuint Source;
	ALuint Buffers[STREAM_BUFFER_COUNT];
	std::vector<BYTE> RawBuffer;
	std::vector<short> ConvertedBuffer;
	bool Playing;
	bool Looping;
	bool Ended;
	bool Paused;
	float Volume;
	unsigned int SamplesQueued;
	unsigned int SamplesProcessed;
};

OpenALSoundRenderer::OpenALSoundRenderer()
	: Data(new Impl)
{
	Data->Device = alcOpenDevice(NULL);
	if (Data->Device == NULL)
	{
		Printf(TEXTCOLOR_RED"OpenAL: could not open default device.\n");
		return;
	}

	Data->Context = alcCreateContext(Data->Device, NULL);
	if (Data->Context == NULL || !alcMakeContextCurrent(Data->Context))
	{
		Printf(TEXTCOLOR_RED"OpenAL: could not create context.\n");
		return;
	}

	const ALCchar *name = alcGetString(Data->Device, ALC_DEVICE_SPECIFIER);
	Data->DeviceName = name != NULL ? name : "default";
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	Data->Valid = true;
	Printf("I_InitSound: Initializing OpenAL on %s\n", Data->DeviceName.GetChars());
}

OpenALSoundRenderer::~OpenALSoundRenderer()
{
	for (size_t i = 0; i < Data->Channels.size(); ++i)
	{
		alSourceStop(Data->Channels[i]->Source);
		alDeleteSources(1, &Data->Channels[i]->Source);
		delete Data->Channels[i];
	}
	Data->Channels.clear();

	if (Data->Context != NULL)
	{
		alcMakeContextCurrent(NULL);
		alcDestroyContext(Data->Context);
	}
	if (Data->Device != NULL)
	{
		alcCloseDevice(Data->Device);
	}
	delete Data;
}

bool OpenALSoundRenderer::IsValid()
{
	return Data->Valid;
}

void OpenALSoundRenderer::SetSfxVolume(float volume)
{
	Data->SfxVolume = volume;
}

void OpenALSoundRenderer::SetMusicVolume(float volume)
{
	Data->MusicVolume = volume;
	OpenALMusicVolume = volume;
	for (size_t i = 0; i < ActiveStreams.size(); ++i)
	{
		ActiveStreams[i]->MusicVolumeChanged();
	}
}

SoundHandle OpenALSoundRenderer::LoadSound(BYTE *sfxdata, int length)
{
	SoundHandle retval = { NULL };
	DPrintf("OpenAL backend cannot decode compressed SFX yet (%d bytes).\n", length);
	return retval;
}

SoundHandle OpenALSoundRenderer::LoadSoundRaw(BYTE *sfxdata, int length, int frequency, int channels, int bits, int loopstart, int loopend)
{
	SoundHandle retval = { NULL };
	ALenum format = FormatFor(channels, bits);
	if (length <= 0 || format == 0 || frequency <= 0)
	{
		return retval;
	}

	std::vector<BYTE> converted;
	const BYTE *data = sfxdata;
	if (bits == 8)
	{
		converted.assign(sfxdata, sfxdata + length);
		data = &converted[0];
	}

	OpenALSample *sample = new OpenALSample;
	sample->Frequency = frequency;
	sample->Channels = channels;
	sample->Bits = bits;
	sample->Samples = length / (channels * ((bits == 16) ? 2 : 1));
	sample->Buffer = 0;

	ClearALErrors();
	alGenBuffers(1, &sample->Buffer);
	alBufferData(sample->Buffer, format, data, length, frequency);
	if (alGetError() != AL_NO_ERROR)
	{
		alDeleteBuffers(1, &sample->Buffer);
		delete sample;
		return retval;
	}

	retval.data = sample;
	return retval;
}

void OpenALSoundRenderer::UnloadSound(SoundHandle sfx)
{
	OpenALSample *sample = static_cast<OpenALSample *>(sfx.data);
	if (sample != NULL)
	{
		alDeleteBuffers(1, &sample->Buffer);
		delete sample;
	}
}

unsigned int OpenALSoundRenderer::GetMSLength(SoundHandle sfx)
{
	OpenALSample *sample = static_cast<OpenALSample *>(sfx.data);
	if (sample == NULL || sample->Frequency == 0)
	{
		return 250;
	}
	return sample->Samples * 1000 / sample->Frequency;
}

unsigned int OpenALSoundRenderer::GetSampleLength(SoundHandle sfx)
{
	OpenALSample *sample = static_cast<OpenALSample *>(sfx.data);
	return sample != NULL ? sample->Samples : 0;
}

float OpenALSoundRenderer::GetOutputRate()
{
	return 44100.f;
}

SoundStream *OpenALSoundRenderer::CreateStream(SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata)
{
	if (callback == NULL || buffbytes <= 0 || samplerate <= 0)
	{
		return NULL;
	}
	return new OpenALSoundStream(callback, buffbytes, flags, samplerate, userdata);
}

SoundStream *OpenALSoundRenderer::OpenStream(const char *filename, int flags, int offset, int length)
{
	return NULL;
}

FISoundChannel *OpenALSoundRenderer::StartSound(SoundHandle sfx, float vol, int pitch, int chanflags, FISoundChannel *reuse_chan)
{
	OpenALSample *sample = static_cast<OpenALSample *>(sfx.data);
	if (sample == NULL)
	{
		return NULL;
	}

	ALuint source = 0;
	ClearALErrors();
	alGenSources(1, &source);
	if (alGetError() != AL_NO_ERROR)
	{
		return NULL;
	}

	alSourcei(source, AL_BUFFER, sample->Buffer);
	alSourcei(source, AL_LOOPING, (chanflags & SNDF_LOOP) ? AL_TRUE : AL_FALSE);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcef(source, AL_GAIN, ChannelGain(vol, chanflags, Data->SfxVolume, Data->Muted));
	alSourcef(source, AL_PITCH, OAL_PITCH(pitch));
	if (alGetError() != AL_NO_ERROR)
	{
		alDeleteSources(1, &source);
		return NULL;
	}

	FISoundChannel *gamechan = reuse_chan != NULL ? reuse_chan : S_GetChannel(reinterpret_cast<void *>(static_cast<uintptr_t>(source)));
	gamechan->SysChannel = reinterpret_cast<void *>(static_cast<uintptr_t>(source));
	MarkStartTime(gamechan);

	OpenALChannel *chan = new OpenALChannel;
	chan->Source = source;
	chan->GameChannel = gamechan;
	chan->Volume = vol;
	chan->Flags = chanflags;
	Data->Channels.push_back(chan);

	ClearALErrors();
	alSourcePlay(source);
	if (alGetError() != AL_NO_ERROR)
	{
		StopChannel(gamechan);
		return NULL;
	}
	if (Data->SfxPaused != 0 && !(chanflags & SNDF_NOPAUSE))
	{
		alSourcePause(source);
	}
	return gamechan;
}

FISoundChannel *OpenALSoundRenderer::StartSound3D(SoundHandle sfx, SoundListener *listener, float vol, FRolloffInfo *rolloff, float distscale, int pitch, int priority, const FVector3 &pos, const FVector3 &vel, int channum, int chanflags, FISoundChannel *reuse_chan)
{
	FISoundChannel *chan = StartSound(sfx, vol, pitch, chanflags, reuse_chan);
	if (chan != NULL)
	{
		OpenALChannel *oalchan = static_cast<OpenALChannel *>(NULL);
		for (size_t i = 0; i < Data->Channels.size(); ++i)
		{
			if (Data->Channels[i]->GameChannel == chan)
			{
				oalchan = Data->Channels[i];
				break;
			}
		}
		if (oalchan != NULL)
		{
			alSourcei(oalchan->Source, AL_SOURCE_RELATIVE, AL_FALSE);
			alSource3f(oalchan->Source, AL_POSITION, pos[0], pos[1], pos[2]);
			alSource3f(oalchan->Source, AL_VELOCITY, vel[0], vel[1], vel[2]);
			if (rolloff != NULL)
			{
				alSourcef(oalchan->Source, AL_REFERENCE_DISTANCE, rolloff->MinDistance * distscale);
				alSourcef(oalchan->Source, AL_MAX_DISTANCE, rolloff->MaxDistance * distscale);
			}
		}
	}
	return chan;
}

void OpenALSoundRenderer::StopChannel(FISoundChannel *chan)
{
	if (chan == NULL || chan->SysChannel == NULL)
	{
		return;
	}
	ALuint source = static_cast<ALuint>(reinterpret_cast<uintptr_t>(chan->SysChannel));
	for (size_t i = 0; i < Data->Channels.size(); ++i)
	{
		if (Data->Channels[i]->Source == source)
		{
			alSourceStop(source);
			alDeleteSources(1, &source);
			chan->SysChannel = NULL;
			S_ChannelEnded(chan);
			delete Data->Channels[i];
			Data->Channels.erase(Data->Channels.begin() + i);
			return;
		}
	}
}

void OpenALSoundRenderer::ChannelVolume(FISoundChannel *chan, float volume)
{
	if (chan != NULL && chan->SysChannel != NULL)
	{
		ALuint source = static_cast<ALuint>(reinterpret_cast<uintptr_t>(chan->SysChannel));
		for (size_t i = 0; i < Data->Channels.size(); ++i)
		{
			if (Data->Channels[i]->Source == source)
			{
				Data->Channels[i]->Volume = volume;
				alSourcef(source, AL_GAIN, ChannelGain(volume, Data->Channels[i]->Flags, Data->SfxVolume, Data->Muted));
				break;
			}
		}
	}
}

void OpenALSoundRenderer::MarkStartTime(FISoundChannel *chan)
{
	if (chan != NULL)
	{
		chan->StartTime.AsOne = I_MSTime();
	}
}

unsigned int OpenALSoundRenderer::GetPosition(FISoundChannel *chan)
{
	if (chan == NULL || chan->SysChannel == NULL)
	{
		return 0;
	}
	ALuint source = static_cast<ALuint>(reinterpret_cast<uintptr_t>(chan->SysChannel));
	ALint offset = 0;
	alGetSourcei(source, AL_SAMPLE_OFFSET, &offset);
	return offset > 0 ? offset : 0;
}

float OpenALSoundRenderer::GetAudibility(FISoundChannel *chan)
{
	if (chan == NULL || chan->SysChannel == NULL)
	{
		return 0.f;
	}
	ALuint source = static_cast<ALuint>(reinterpret_cast<uintptr_t>(chan->SysChannel));
	ALfloat gain = 0.f;
	alGetSourcef(source, AL_GAIN, &gain);
	return gain;
}

void OpenALSoundRenderer::Sync(bool sync)
{
}

void OpenALSoundRenderer::SetSfxPaused(bool paused, int slot)
{
	if (paused)
		Data->SfxPaused |= 1 << slot;
	else
		Data->SfxPaused &= ~(1 << slot);

	for (size_t i = 0; i < Data->Channels.size(); ++i)
	{
		if (Data->SfxPaused != 0 && !(Data->Channels[i]->Flags & SNDF_NOPAUSE))
			alSourcePause(Data->Channels[i]->Source);
		else
			ResumeIfPaused(Data->Channels[i]->Source);
	}
}

void OpenALSoundRenderer::SetInactive(EInactiveState inactive)
{
	Data->Muted = inactive == INACTIVE_Mute;
	for (size_t i = 0; i < Data->Channels.size(); ++i)
	{
		if (inactive == INACTIVE_Complete)
			alSourcePause(Data->Channels[i]->Source);
		else if (Data->SfxPaused == 0)
			ResumeIfPaused(Data->Channels[i]->Source);
		alSourcef(Data->Channels[i]->Source, AL_GAIN, ChannelGain(Data->Channels[i]->Volume, Data->Channels[i]->Flags, Data->SfxVolume, Data->Muted));
	}
}

void OpenALSoundRenderer::UpdateSoundParams3D(SoundListener *listener, FISoundChannel *chan, bool areasound, const FVector3 &pos, const FVector3 &vel)
{
	if (chan != NULL && chan->SysChannel != NULL)
	{
		ALuint source = static_cast<ALuint>(reinterpret_cast<uintptr_t>(chan->SysChannel));
		alSource3f(source, AL_POSITION, pos[0], pos[1], pos[2]);
		alSource3f(source, AL_VELOCITY, vel[0], vel[1], vel[2]);
	}
}

void OpenALSoundRenderer::UpdateListener(SoundListener *listener)
{
	if (listener == NULL || !listener->valid)
	{
		return;
	}
	const float angle = listener->angle;
	const ALfloat orientation[6] =
	{
		cosf(angle), 0.f, sinf(angle),
		0.f, -1.f, 0.f
	};
	alListener3f(AL_POSITION, listener->position[0], listener->position[1], listener->position[2]);
	alListener3f(AL_VELOCITY, listener->velocity[0], listener->velocity[1], listener->velocity[2]);
	alListenerfv(AL_ORIENTATION, orientation);
}

void OpenALSoundRenderer::UpdateSounds()
{
	for (size_t i = 0; i < ActiveStreams.size(); ++i)
	{
		ActiveStreams[i]->Pump();
	}

	for (size_t i = 0; i < Data->Channels.size(); )
	{
		ALint state = AL_STOPPED;
		alGetSourcei(Data->Channels[i]->Source, AL_SOURCE_STATE, &state);
		if (state == AL_STOPPED)
		{
			OpenALChannel *chan = Data->Channels[i];
			alDeleteSources(1, &chan->Source);
			if (chan->GameChannel != NULL)
			{
				chan->GameChannel->SysChannel = NULL;
				S_ChannelEnded(chan->GameChannel);
			}
			delete chan;
			Data->Channels.erase(Data->Channels.begin() + i);
		}
		else
		{
			++i;
		}
	}
}

void OpenALSoundRenderer::PrintStatus()
{
	Printf("OpenAL sound module active.\n");
	Printf("Device: %s\n", Data->DeviceName.GetChars());
	Printf("Channels: %u active, streams: %u active\n", (unsigned)Data->Channels.size(), (unsigned)ActiveStreams.size());
	Printf("Muted: %s, pause mask: 0x%x\n", Data->Muted ? "yes" : "no", Data->SfxPaused);
}

void OpenALSoundRenderer::PrintDriversList()
{
	const ALCchar *devices = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
	if (devices == NULL)
	{
		Printf("OpenAL device enumeration is unavailable.\n");
		return;
	}
	for (const ALCchar *device = devices; *device != 0; device += strlen(device) + 1)
	{
		Printf("%s\n", device);
	}
}

FString OpenALSoundRenderer::GatherStats()
{
	FString stats;
	stats.Format("OpenAL: %u active channels, %u active streams, muted %s, pause mask 0x%x",
		(unsigned)Data->Channels.size(),
		(unsigned)ActiveStreams.size(),
		Data->Muted ? "yes" : "no",
		Data->SfxPaused);
	return stats;
}

#endif
