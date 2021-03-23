#include "beep.h"

#include "macros.h"

#include <spdlog/spdlog.h>

#include <bass.h>


namespace c8
{
	struct beep::impl
	{

		static DWORD CALLBACK WriteStream(HSTREAM handle, int16_t *buffer, DWORD length, void *user)
		{
			impl* impl1 = (impl*)user;
			float freq;

			memset(buffer, 0, length);

			QWORD posByte = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);
			BASS_ChannelGetAttribute(handle, BASS_ATTRIB_FREQ, &freq);

			float timeStep = 1.0f / freq;
			float vol = impl1->m_vol_fn();

			if (vol == 0)
				return length;

			for (size_t i = 0; i < length / sizeof(int16_t); i++)
			{
				impl1->m_time += timeStep;
				buffer[i] = int16_t(std::sin(impl1->m_time * 440 * 2 * 3.141592) * 32768 * vol);
			}

			return length;

		}

		impl()
		{
			if (!BASS_Init(-1, 8000, BASS_DEVICE_MONO, 0, NULL))
				C8_ERRO("Could not initialize BASS");
		}
		~impl()
		{
			BASS_Free();
		}



		void start(const beep::volume_fn& vol_fn)
		{
			m_vol_fn = vol_fn;
			m_time = 0;
			HSTREAM stream;


			// 10ms update period
			BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10); // 10ms update period

			// set default/maximum buffer length to 200ms
			BASS_SetConfig(BASS_CONFIG_BUFFER, 200);

			if (!(stream = BASS_StreamCreate(8000, 1, 0, (STREAMPROC*)&WriteStream, this)))
				C8_ERRO("Could not initialize audio stream");



			BASS_ChannelPlay(stream, FALSE); // start it

		}

		volume_fn m_vol_fn;
		float m_time;

	};



	beep::beep()
	{
		m_impl = std::make_unique<impl>();
	}

	beep::~beep()
	{

	}

	void beep::start(const volume_fn & vol_fn)
	{
		m_impl->start(vol_fn);
	}
}
