#pragma once

#include <memory>

namespace c8
{
	class Chip8
	{
	public:
		Chip8();
		~Chip8();

		void SetPixel(uint8_t x, uint8_t y, bool value);
		bool GetPixel(uint8_t x, uint8_t y) const;

		uint8_t GetRegister(uint8_t i) const;
		uint8_t GetRegisterCount() const;

		uint8_t GetSoundTimer() const;

		void ClockCycle(float deltaTime);

		void Load(void * romData, size_t length);

		void SetKeyState(uint8_t key, bool pressed);

		uint8_t GetScreenWidth() const;
		uint8_t GetScreenHeight() const;


	private:
		struct Impl;
		std::unique_ptr<Impl> m_Impl;
	};
}