#pragma once

#include <memory>
#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct GLFWwindow;

namespace c8
{
	class Chip8;
	class AudioPlayer;
	class Beep;

	class Application
	{
	public:
		Application();
		~Application();

		bool Start();

		void OnKeyPressed(int key);
		void OnKeyReleased(int key);

		void OnResize(int w, int h);

		void LoadFromFile(const std::string& path);
		void Load(void *data, size_t length);
		void Reset();

		void LoadRomFilenames();


	private:

		struct RomFile
		{
			std::string fileName;
			std::string path;
		};

		struct Config
		{
			int ClockFrequency = 500;
			glm::vec3 FrontColor = { 1, 1, 1 };
			glm::vec3 BackColor = { 0.2, 0.2, 0.2 };
			float Volume = 0.8f;
		} m_Config;

		bool GetChip8Key(int key, uint8_t& result);

		void Render();
		void RenderImGui();

		void LoadConfiguration();
		void StoreConfiguration();
		
		std::unique_ptr<Chip8> m_Chip8;
		std::unique_ptr<Beep> m_Beep;

		GLFWwindow* m_Window;

		std::vector<RomFile> m_RomFiles;
		std::string m_CurrentRomFile;
	};
}