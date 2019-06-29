#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct GLFWwindow;

namespace c8
{
	class Chip8;
	class AudioPlayer;
	class Beep;

	class Screen
	{
	public:
		Screen(const std::shared_ptr<Chip8>& chip8, float fadeTime);

		void Update(float dt);
		float GetPixel(uint8_t x, uint8_t y) const;

	private:
		float m_FadeTime;
		std::unique_ptr<float[]> m_ScreenData;
		std::shared_ptr<Chip8> m_Chip;
	};


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

		enum class EViewMode : uint32_t
		{
			Noraml = 0,
			Voxel = 1
		};

		struct RomFile
		{
			std::string fileName;
			std::string path;
		};

		struct Config
		{
			int ClockFrequency = 500;
			glm::vec3 FrontColor = { 1, 1, 1 };
			glm::vec3 BackColor = { 0.1, 0.4, 0.1 };
			float Volume = 0.8f;
			EViewMode ViewMode = EViewMode::Voxel;

		} m_Config;

		void InitGraphics();

		bool GetChip8Key(int key, uint8_t& result);

		void Render();
		void Render3D();
		void RenderImGui();

		void LoadConfiguration();
		void StoreConfiguration();

		template<typename T, size_t N>
		void CreateVertexArray(const std::string& id, const std::array<T, N>& vertices);
		
		std::shared_ptr<Chip8> m_Chip8;
		std::unique_ptr<Screen> m_Screen;
		std::unique_ptr<Beep> m_Beep;

		GLFWwindow* m_Window;

		std::vector<RomFile> m_RomFiles;
		std::string m_CurrentRomFile;

		std::unordered_map<std::string, uint32_t> m_Programs;
		std::unordered_map<std::string, uint32_t> m_Vaos;
		std::vector<uint32_t> m_Vbs;
	};
	



}