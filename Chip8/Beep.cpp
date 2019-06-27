#include "Beep.h"

#include "Macros.h"

#include <spdlog/spdlog.h>

#include <bass.h>


namespace c8
{
	struct Beep::Impl
	{

		static DWORD CALLBACK WriteStream(HSTREAM handle, int16_t *buffer, DWORD length, void *user)
		{
			Impl* impl = (Impl*)user;
			float freq;

			memset(buffer, 0, length);

			QWORD posByte = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);
			BASS_ChannelGetAttribute(handle, BASS_ATTRIB_FREQ, &freq);

			float timeStep = 1.0f / freq;
			float vol = impl->VolFn();

			if (vol == 0)
				return length;

			for (size_t i = 0; i < length / sizeof(int16_t); i++)
			{
				impl->Time += timeStep;
				buffer[i] = int16_t(std::sin(impl->Time * 440 * 2 * 3.141592) * 32768 * vol);
			}

			return length;

		}

		Impl()
		{
			if (!BASS_Init(-1, 8000, BASS_DEVICE_MONO, 0, NULL))
				C8_ERRO("Could not initialize BASS");
		}
		~Impl()
		{
			BASS_Free();
		}



		void Start(const Beep::VolumeFn& volFn)
		{
			VolFn = volFn;
			Time = 0;
			HSTREAM stream;


			// 10ms update period
			BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10); // 10ms update period

			// set default/maximum buffer length to 200ms
			BASS_SetConfig(BASS_CONFIG_BUFFER, 200);

			if (!(stream = BASS_StreamCreate(8000, 1, 0, (STREAMPROC*)&WriteStream, this)))
				C8_ERRO("Could not initialize audio stream");



			BASS_ChannelPlay(stream, FALSE); // start it

		}

		VolumeFn VolFn;
		float Time;

	};



	Beep::Beep()
	{
		m_Impl = std::make_unique<Impl>();
	}

	Beep::~Beep()
	{

	}

	void Beep::Start(const VolumeFn & volFn)
	{
		m_Impl->Start(volFn);
	}
}
