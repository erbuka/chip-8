#include "application.h"

#include "chip8.h"
#include "beep.h"
#include "macros.h"


#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <filesystem>
#include <functional>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32


namespace c8
{
	#pragma region Other resources
	static const std::string s_vao_cube_front = "Cube0";
	static const std::string s_vao_cube_right = "Cube1";
	static const std::string s_vao_cube_left = "Cube2";
	static const std::string s_vao_cube_top = "Cube3";
	static const std::string s_vao_cube_bottom = "Cube4";
	static const std::string s_vao_back_plane = "Plane";
	#pragma endregion


	#pragma region Shaders and Programs

	static const std::string s_pr_chip8 = "Shader Chip-8";
	
	static const std::string s_vertex_source = R"(
		#version 330
	
		uniform mat4 uProjection;
		uniform mat4 uModelView;
	
		layout(location = 0) in vec3 aPosition;
		layout(location = 1) in vec3 aNormal;

		out vec3 fsNormal;

		void main() {
			gl_Position = uProjection * uModelView * vec4(aPosition, 1.0);
			fsNormal = (uModelView * vec4(aNormal, 0.0)).xyz;
		}
	)";

	static const std::string s_fragment_source = R"(
		#version 330

		uniform vec3 uAmbientLight;
		uniform vec3 uLightDirection;
		uniform vec4 uColor;

		in vec3 fsNormal;
		
		void main() {
			float factor = max(0.0, dot(uLightDirection, fsNormal));
			gl_FragColor = vec4(uColor.rgb * factor + uColor.rgb * uAmbientLight, uColor.a);
		}
	)";


	#pragma endregion


	#pragma region Shader Loaders

	static unsigned int create_and_link_program(const std::initializer_list<unsigned int>& shaders)
	{
		// Vertex and fragment shaders are successfully compiled.
		// Now time to link them together into a program.
		// Get a program object.
		unsigned int program = glCreateProgram();


		// Attach our shaders to our program
		for (const auto s : shaders)
		{
			glAttachShader(program, s);
		}

		// Link our program
		glLinkProgram(program);

		// Note the different functions here: glGetProgram* instead of glGetShader*.
		GLint is_linked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int *)&is_linked);
		if (is_linked == GL_FALSE)
		{
			GLint max_length = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

			// The maxLength includes the NULL character
			std::vector<char> info_log(max_length);
			glGetProgramInfoLog(program, max_length, &max_length, &info_log[0]);

			// We don't need the program anymore.
			glDeleteProgram(program);

			// Don't leak shaders either.
			for (const auto s : shaders)
			{
				glDeleteShader(s);
			}

			// Use the infoLog as you see fit.
			C8_ERRO("Program linking failed: {0}", info_log.data());

			// In this simple program, we'll just leave
			return 0;
		}

		// Don't leak shaders either.

		for (const auto s : shaders)
		{
			glDetachShader(program, s);
		}


		return program;
	}

	static unsigned int load_shader(unsigned int type, const std::string& source)
	{
		// Create an empty vertex shader handle
		unsigned int shader = glCreateShader(type);

		// Send the vertex shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		const GLchar *src = (const GLchar *)source.c_str();
		glShaderSource(shader, 1, &src, 0);

		// Compile the vertex shader
		glCompileShader(shader);

		GLint is_compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
		if (is_compiled == GL_FALSE)
		{
			GLint max_length = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);

			// The maxLength includes the NULL character
			std::vector<char> info_log(max_length);
			glGetShaderInfoLog(shader, max_length, &max_length, &info_log[0]);

			// We don't need the shader anymore.
			glDeleteShader(shader);

			// Use the infoLog as you see fit.
			C8_ERRO("Shader compilation failed: {0}", info_log.data());

			// In this simple program, we'll just leave

			return 0;
		}

		return shader;
	}

	#pragma endregion


	static const std::string s_chip8_configuration_file = "config.bin";
	static const std::string s_chip8_log_file = "chip8.log";

	static application* get_application(GLFWwindow* window)
	{
		return static_cast<application*>(glfwGetWindowUserPointer(window));
	}

	static void glfw_window_size(GLFWwindow* window, int w, int h)
	{
		get_application(window)->on_resize(w, h);
	}


	static void glfw_key(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto app = get_application(window);
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
		{
			app->on_key_pressed(key);
		}
		else
		{
			app->on_key_released(key);
		}
	}



	application::application()
	{
		// Init Chip
		m_chip8 = std::make_shared<chip8>();

		// Init screen interface
		m_screen = std::make_unique<screen>(m_chip8, 0.2f);
		
		// Init sound
		m_beep = std::make_unique<beep>();
		m_beep->start([this]() -> float {
			return (m_chip8->get_sound_timer() > 0 ? 1.0f : 0.0f) * m_config.volume;
		});


	}

	application::~application()
	{
		for (auto vb : m_vbs)
			GL_CALL(glDeleteBuffers(1, &vb));

		for (const auto &vao : m_vaos)
			GL_CALL(glDeleteVertexArrays(1, &vao.second));
		
		for (const auto &s : m_programs)
			GL_CALL(glDeleteProgram(s.second));

		glfwTerminate();


	}

	template<typename T, size_t N>
	void application::create_vertex_array(const std::string & id, const std::array<T, N>& vertices)
	{
		uint32_t vao, vb;

		GL_CALL(glGenVertexArrays(1, &vao));
		GL_CALL(glGenBuffers(1, &vb));

		GL_CALL(glBindVertexArray(vao));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vb));

		GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glEnableVertexAttribArray(1));

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)12);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

		m_vaos[id] = vao;
		m_vbs.push_back(vb);

	}

	bool application::start()
	{
		/* Setup spdlog */
		#ifndef DEBUG
			spdlog::set_default_logger(spdlog::rotating_logger_mt("chip8", s_chip8_log_file, 512 * 1024, 0, false));
		#endif

		/* Load configuration */
		load_configuration();

		/* Initialize the library */
		if (!glfwInit())
		{
			C8_ERRO("Could not initialize GLFW");
			return false;
		}

		/* Create a windowed mode window and its OpenGL context */
		m_window = glfwCreateWindow(640, 480, "Chip-8", NULL, NULL);
		if (!m_window)
		{
			C8_ERRO("Could not create GLFW window");
			glfwTerminate();
			return false;
		}

		/* Make the window's context current */
		glfwMakeContextCurrent(m_window);
		glfwSetWindowUserPointer(m_window, this);
		gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

		/* GLFW callbacks */
		glfwSetWindowSizeCallback(m_window, &glfw_window_size);
		glfwSetKeyCallback(m_window, &glfw_key);

		/* Init graphics resources */
		init_graphics();

		/* Initialize ImGui */

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForOpenGL(m_window, true);
		ImGui_ImplOpenGL3_Init("#version 130");

		/* Load rom filenames */
		load_rom_filenames();

		/* Main loop */
		bool sound_ready = true;
		auto prev_time = std::chrono::high_resolution_clock::now();
		auto curr_time = std::chrono::high_resolution_clock::now();
		float time_acc = 0.0f;

		while (!glfwWindowShouldClose(m_window))
		{
			curr_time = std::chrono::high_resolution_clock::now();
			std::chrono::duration<float> delta_time = curr_time - prev_time;
			prev_time = curr_time;
			float dt = delta_time.count();
			float clock_period = 1.0f / m_config.clock_frequency;


			// Update CHIP8
			time_acc += dt;
			while (time_acc >= clock_period)
			{
				time_acc -= clock_period;
				m_chip8->clock_cycle(clock_period);
				m_screen->update(clock_period);
			}


			// Render
			m_config.view_mode == e_view_mode::normal ? render() : render_voxel();

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			render_im_gui();

			ImGui::EndFrame();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			/* Swap front and back buffers */
			glfwSwapBuffers(m_window);

			/* Poll for and process events */
			glfwPollEvents();
		}

		store_configuration();

		return 0;
	}

	void application::on_key_pressed(int key)
	{
		uint8_t k;
		if (get_chip8_key(key, k))
		{
			m_chip8->set_key_state(k, true);
		}
	}

	void application::on_key_released(int key)
	{

		uint8_t k;
		if (get_chip8_key(key, k))
		{
			m_chip8->set_key_state(k, false);
		}
	}

	void application::on_resize(int w, int h)
	{
		glViewport(0, 0, w, h);
	}

	void application::load_from_file(const std::string & path)
	{
		namespace fs = std::filesystem;

		std::ifstream is;

		is.open(path, std::ios::binary | std::ios::ate);

		if (!is.good())
		{
			C8_ERRO("Can't open file: {0}", path);
			return;
		}

		auto length = is.tellg();
		char* buffer = new char[length];
		is.seekg(std::ios::beg);
		is.read(buffer, length);
		is.close();

		m_chip8->load(buffer, length);

		delete[] buffer;

		m_current_rom_file = path;

	}

	void application::load(void * data, size_t length)
	{
		m_chip8->load(data, length);
	}

	void application::reset()
	{
		if (!m_current_rom_file.empty())
		{
			load_from_file(m_current_rom_file);
		}
	}

	void application::load_rom_filenames()
	{
		namespace fs = std::filesystem;

		m_rom_files.clear();

		fs::path path = fs::absolute("roms");

		for (auto f : fs::directory_iterator(path))
		{
			if (fs::is_regular_file(f.path()))
			{
				m_rom_files.push_back({
					f.path().filename().string(),
					f.path().string()
				});
			}
		}

	}


	void application::init_graphics()
	{

		m_programs[s_pr_chip8] = create_and_link_program({ load_shader(GL_VERTEX_SHADER, s_vertex_source), load_shader(GL_FRAGMENT_SHADER, s_fragment_source) });
		
		float w = m_chip8->get_screen_width(), h = m_chip8->get_screen_height();

		std::array<float, 6 * 6> cube_front = {
			// Front face
			-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
			0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
			0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
			-0.5f, -0.5, 0.5f, 0.0f, 0.0f, 1.0f,
			0.5f, 0.5, 0.5f, 0.0f, 0.0f, 1.0f,
			-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
		};

		std::array<float, 6 * 6> cube_left = {
			// Right face
			-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
		};

		std::array<float, 6 * 6> cube_right = {
			// Right face
			0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
		};

		std::array<float, 6 * 6> cube_top = {
			// Top face
			-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		};

		std::array<float, 6 * 6> cube_bottom = {
			// Bottom face
			-0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
			0.5f,  -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
			0.5f,  -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
			0.5f,  -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
			-0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
			-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
		};

		std::array<float, 6 * 6> backplane_vertices = {
			-w / 2, -h / 2, 0, 0, 0, 1,
			w / 2, -h / 2, 0, 0, 0, 1,
			w / 2, h / 2, 0, 0, 0, 1,
			-w / 2, -h / 2, 0, 0, 0, 1,
			w / 2, h / 2, 0, 0, 0, 1,
			-w / 2, h / 2, 0, 0, 0, 1
		};

		// Create cube VAO

		create_vertex_array(s_vao_cube_front, cube_front);
		create_vertex_array(s_vao_cube_left, cube_left);
		create_vertex_array(s_vao_cube_right, cube_right);
		create_vertex_array(s_vao_cube_top, cube_top);
		create_vertex_array(s_vao_cube_bottom, cube_bottom);

		// Create back plane
		create_vertex_array(s_vao_back_plane, backplane_vertices);


		
	}

	bool application::get_chip8_key(int key, uint8_t & result)
	{
		if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
		{
			result = key - GLFW_KEY_0;
			return true;
		} 
		else if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
		{
			result = key - GLFW_KEY_KP_0;
			return true;
		}

		

		switch (key)
		{
		case GLFW_KEY_A: result = 0xA; break;
		case GLFW_KEY_B: result = 0xB; break;
		case GLFW_KEY_C: result = 0xC; break;
		case GLFW_KEY_D: result = 0xD; break;
		case GLFW_KEY_E: result = 0xE; break;
		case GLFW_KEY_F: result = 0xF; break;
		default: return false;
		}
		return true;
	
	}

	void application::render()
	{
		int wh, ww;
		int ew = m_chip8->get_screen_width(), eh = m_chip8->get_screen_height();

		glfwGetWindowSize(m_window, &ww, &wh);

		float screen_aspect = float(ww) / wh;
		float emulator_aspect = float(ew) / eh;

		float vw, vh;

		if (screen_aspect > emulator_aspect)
		{
			// Adapt with height

			vh = float(eh);
			vw = vh * screen_aspect;

		}
		else
		{
			// Adapt with width
			vw = float(ew);
			vh = vw / screen_aspect;
		}

		glClearColor(0, 0, 0, 1);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		glClear(GL_COLOR_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-vw / 2, vw / 2, -vh / 2, vh / 2, 1, -1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glTranslatef(-m_chip8->get_screen_width() / 2.0f, -m_chip8->get_screen_height() / 2.0f, 0);
		
		glColor3fv(glm::value_ptr(m_config.back_color));
		glBegin(GL_QUADS);
		{
			glVertex2i(0, 0);
			glVertex2i(ew, 0);
			glVertex2i(ew, eh);
			glVertex2i(0, eh);
		}
		glEnd();

		for (int x = 0; x < m_chip8->get_screen_width(); x++)
		{
			for (int y = 0; y < m_chip8->get_screen_height(); y++)
			{
				int ny = m_chip8->get_screen_height() - y - 1;
				glm::vec4 color = { m_config.front_color , m_screen->get_pixel(x, y) };

				glBegin(GL_QUADS);
				{
					glColor4fv(glm::value_ptr(color));
					glVertex2i(x, ny);
					glVertex2i(x + 1, ny);
					glVertex2i(x + 1, ny + 1);
					glVertex2i(x, ny + 1);
				}
				glEnd();
			}
		}
	}

	void application::render_voxel()
	{
		int ww, wh;
		glfwGetWindowSize(m_window, &ww, &wh);

		int ew = m_chip8->get_screen_width(), eh = m_chip8->get_screen_height();

		float emulator_aspect = float(ew) / eh;
		float screen_aspect = float(ww) / wh;

		float vw, vh;

		if (screen_aspect > emulator_aspect)
		{
			// Adapt with height
			vh = float(eh);
			vw = vh * screen_aspect;
		}
		else
		{
			// Adapt with width
			vw = float(ew);
			vh = vw / screen_aspect;
		}

		float fovY = glm::pi<float>() / 4;

		float distance = (std::cos(fovY / 2) * vh / 2) / std::sin(fovY / 2);

		GL_CALL(glEnable(GL_CULL_FACE));
		GL_CALL(glEnable(GL_DEPTH_TEST));
		glEnable(GL_BLEND);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_CALL(glCullFace(GL_BACK));

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		GL_CALL(glUseProgram(m_programs[s_pr_chip8]));

		glm::mat4 projection = glm::perspective(fovY, screen_aspect, 0.1f, 1000.0f);
		glm::vec3 light_dir = glm::normalize(glm::vec3(0, 0, 1));
		glm::vec3 ambient_light = { 0.2f, 0.2f, 0.2f };

		GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_programs[s_pr_chip8], "uProjection"), 1, GL_FALSE, glm::value_ptr(projection)));
		GL_CALL(glUniform3fv(glGetUniformLocation(m_programs[s_pr_chip8], "uLightDirection"), 1, glm::value_ptr(light_dir)));
		GL_CALL(glUniform3fv(glGetUniformLocation(m_programs[s_pr_chip8], "uAmbientLight"), 1, glm::value_ptr(ambient_light)));

		glm::mat4 model_view;
		model_view = glm::translate(glm::identity<glm::mat4>(), { 0, 0, -(distance + 1.0f) });
		glm::vec4 color = { m_config.back_color, 1.0f };


		glBindVertexArray(m_vaos[s_vao_back_plane]);
		GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_programs[s_pr_chip8], "uModelView"), 1, GL_FALSE, glm::value_ptr(model_view)));
		GL_CALL(glUniform4fv(glGetUniformLocation(m_programs[s_pr_chip8], "uColor"), 1, glm::value_ptr(color)));
		GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));



		for (int x = 0; x < ew; x++)
		{
			for (int y = 0; y < eh; y++)
			{
				float pixel = m_screen->get_pixel(x, y);

				if (pixel > 0)
				{
					color = { m_config.front_color, pixel };
					model_view = glm::translate(glm::identity<glm::mat4>(), { -ew / 2 + x + 0.5f, eh / 2 - y - 0.5f, -(distance + 0.5f) });

					GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_programs[s_pr_chip8], "uModelView"), 1, GL_FALSE, glm::value_ptr(model_view)));
					GL_CALL(glUniform4fv(glGetUniformLocation(m_programs[s_pr_chip8], "uColor"), 1, glm::value_ptr(color)));

					if (x > 0 && m_screen->get_pixel(x - 1, y) == 0)
					{
						glBindVertexArray(m_vaos[s_vao_cube_left]);
						glDrawArrays(GL_TRIANGLES, 0, 6);
					}

					if (x < ew - 1 && m_screen->get_pixel(x + 1, y) == 0)
					{
						glBindVertexArray(m_vaos[s_vao_cube_right]);
						glDrawArrays(GL_TRIANGLES, 0, 6);
					}

					if (y > 0 && m_screen->get_pixel(x, y - 1) == 0)
					{
						glBindVertexArray(m_vaos[s_vao_cube_top]);
						glDrawArrays(GL_TRIANGLES, 0, 6);
					}

					if (y < eh - 1 && m_screen->get_pixel(x, y + 1) == 0)
					{
						glBindVertexArray(m_vaos[s_vao_cube_bottom]);
						glDrawArrays(GL_TRIANGLES, 0, 6);

					}


				}
			}
		}

		for (int x = 0; x < ew; x++)
		{
			for (int y = 0; y < eh; y++)
			{
				float pixel = m_screen->get_pixel(x, y);
				glm::vec4 color = { m_config.front_color, pixel };
				model_view = glm::translate(glm::identity<glm::mat4>(), { -ew / 2 + x + 0.5f, eh / 2 - y - 0.5f, -(distance + 0.5f) });

				GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_programs[s_pr_chip8], "uModelView"), 1, GL_FALSE, glm::value_ptr(model_view)));
				GL_CALL(glUniform4fv(glGetUniformLocation(m_programs[s_pr_chip8], "uColor"), 1, glm::value_ptr(color)));

				glBindVertexArray(m_vaos[s_vao_cube_front]);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		}

		glUseProgram(0);
	}

	void application::render_im_gui()
	{
		ImGui::BeginMainMenuBar();
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::BeginMenu("Load ROM..."))
				{
					for (const auto& rom_file : m_rom_files)
					{
						if (ImGui::MenuItem(rom_file.file_name.c_str()))
						{
							load_from_file(rom_file.path);;
						}
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Reset"))
				{
					reset();
				}

				if (ImGui::BeginMenu("Options"))
				{
					ImGui::SliderInt("Clock Freq. (hz)", &m_config.clock_frequency, 60, 1000);
					ImGui::SliderFloat("Volume", &m_config.volume, 0.0f, 1.0f, "%.1f");
					ImGui::ColorEdit3("Front Color", glm::value_ptr(m_config.front_color));
					ImGui::ColorEdit3("Back Color", glm::value_ptr(m_config.back_color));
					ImGui::Combo("View Mode", (int*)&m_config.view_mode, "Normal\0Voxel");
					if (ImGui::Button("Restore defaults", { -1, 0 }))
					{
						m_config = config();
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
				{
					glfwSetWindowShouldClose(m_window, true);
				}

				ImGui::EndMenu();

			}
		}
		ImGui::EndMainMenuBar();

	}

	void application::load_configuration()
	{
		std::ifstream is(s_chip8_configuration_file, std::ios::binary);

		if (is.good())
		{
			is.read((char*)&m_config, sizeof(config));
		}

		is.close();


	}

	void application::store_configuration()
	{
		std::ofstream os(s_chip8_configuration_file, std::ios::binary);
		os.write((char*)&m_config, sizeof(config));
		os.close();
	}

	screen::screen(const std::shared_ptr<chip8>& chip8, float fadeTime) :
		m_chip(chip8),
		m_fade_time(fadeTime)
	{
		m_screen_data = std::make_unique<float[]>(chip8->get_screen_width() * chip8->get_screen_height());

		for (uint32_t x = 0; x < m_chip->get_screen_width(); x++)
			for (uint32_t y = 0; y < m_chip->get_screen_height(); y++)
				m_screen_data[y * m_chip->get_screen_width() + x] = 0.0f;

	}


	void screen::update(float dt)
	{
		float fade_amount = dt / (m_fade_time + std::numeric_limits<float>::epsilon());
		for (uint32_t x = 0; x < m_chip->get_screen_width(); x++)
		{
			for (uint32_t y = 0; y < m_chip->get_screen_height(); y++)
			{
				auto &v = m_screen_data[y * m_chip->get_screen_width() + x];

				if (m_chip->get_pixel(x, y))
					v = 1.0f;
				else
					v = std::max(0.0f, v - fade_amount);
			}
		}
	}

	float screen::get_pixel(uint8_t x, uint8_t y) const
	{
		if (m_fade_time == 0)
			return m_chip->get_pixel(x, y);
		else
			return m_screen_data[y * m_chip->get_screen_width() + x];
	}
}