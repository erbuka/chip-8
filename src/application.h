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
	class chip8;
	class audio_player;
	class beep;

	class screen
	{
	public:
		screen(const std::shared_ptr<chip8>& chip8, float fadeTime);

		void update(float dt);
		float get_pixel(uint8_t x, uint8_t y) const;

	private:
		float m_fade_time;
		std::unique_ptr<float[]> m_screen_data;
		std::shared_ptr<chip8> m_chip;
	};


	class application
	{
	public:
		application();
		~application();

		bool start();

		void on_key_pressed(int key);
		void on_key_released(int key);

		void on_resize(int w, int h);

		void load_from_file(const std::string& path);
		void load(void *data, size_t length);
		void reset();

		void load_rom_filenames();


	private:

		enum class e_view_mode : uint32_t
		{
			normal = 0,
			voxel = 1
		};

		struct rom_file
		{
			std::string file_name;
			std::string path;
		};

		struct config
		{
			int clock_frequency = 500;
			glm::vec3 front_color = { 1, 1, 1 };
			glm::vec3 back_color = { 0.1, 0.4, 0.1 };
			float volume = 0.8f;
			e_view_mode view_mode = e_view_mode::voxel;

		} m_config;

		void init_graphics();

		bool get_chip8_key(int key, uint8_t& result);

		void render();
		void render_voxel();
		void render_im_gui();

		void load_configuration();
		void store_configuration();

		template<typename T, size_t N>
		void create_vertex_array(const std::string& id, const std::array<T, N>& vertices);
		
		std::shared_ptr<chip8> m_chip8;
		std::unique_ptr<screen> m_screen;
		std::unique_ptr<beep> m_beep;

		GLFWwindow* m_window;

		std::vector<rom_file> m_rom_files;
		std::string m_current_rom_file;

		std::unordered_map<std::string, uint32_t> m_programs;
		std::unordered_map<std::string, uint32_t> m_vaos;
		std::vector<uint32_t> m_vbs;
	};
	



}