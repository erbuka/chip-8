#pragma once

#include <memory>

namespace c8
{
	class chip8
	{
	public:
		chip8();
		~chip8();

		// Todo think about excluding this from PI
		void set_pixel(uint8_t x, uint8_t y, bool value);
		bool get_pixel(uint8_t x, uint8_t y) const;

		uint8_t get_register(uint8_t i) const;
		uint8_t get_register_count() const;

		uint8_t get_sound_timer() const;

		void clock_cycle(float delta_time);

		void load(void * rom_data, size_t length);

		void set_key_state(uint8_t key, bool pressed);

		uint8_t get_screen_width() const;
		uint8_t get_screen_height() const;


	private:
		struct impl;
		std::unique_ptr<impl> m_impl;
	};
}