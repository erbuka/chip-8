#include "beep.h"

#include "macros.h"

#include <spdlog/spdlog.h>

#include <glm/ext.hpp>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>


namespace c8
{

	struct beep::impl
	{

		static constexpr ma_uint32 s_sample_rate = 48000;
		static constexpr ma_uint32 s_channels = 2;
		static constexpr float s_samples_per_second = 1.0f / s_sample_rate;

		uint64_t m_current_frame = 0;
		ma_device m_device;
		volume_fn m_vol_fn;

		static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			// In playback mode copy data to pOutput. In capture mode read data from pInput. In full-duplex mode, both
			// pOutput and pInput will be valid and you can move data from pInput into pOutput. Never process more than
			// frameCount frames.
			auto impl0 = static_cast<impl*>(pDevice->pUserData);

			const auto vol = impl0->m_vol_fn();
			const auto outFloat = static_cast<float*>(pOutput);

			for (size_t s = 0; s < frameCount; ++s)
			{
				for (size_t c = 0; c < s_channels; ++c)
				{
					float time = impl0->m_current_frame * s_samples_per_second;
					float value = std::sin(time * glm::pi<float>() * 2.0f * 440.0f) * vol;
					outFloat[s * s_channels + c] = value;
				}
				impl0->m_current_frame++;
			}

		}

		impl() {}

		void start(const volume_fn& fn)
		{
			m_vol_fn = fn;

			ma_device_config config = ma_device_config_init(ma_device_type_playback);
			config.playback.format = ma_format_f32;			
			config.playback.channels = s_channels;			
			config.sampleRate = s_sample_rate;				
			config.dataCallback = impl::data_callback;		
			config.pUserData = this;						

			if (ma_device_init(NULL, &config, &m_device) != MA_SUCCESS) {
				spdlog::error("Failed to initialize audio device");
				return;
			}

			ma_device_start(&m_device);     // The device is sleeping by default so you'll need to start it manually.

		}

		~impl()
		{
			ma_device_uninit(&m_device);    // This will stop the device so no need to do that manually.
		}
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
