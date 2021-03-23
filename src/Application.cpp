#include "Application.h"

#include "Chip8.h"
#include "Beep.h"
#include "Macros.h"


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
	static const std::string VAO_CUBE_FRONT = "Cube0";
	static const std::string VAO_CUBE_RIGHT = "Cube1";
	static const std::string VAO_CUBE_LEFT = "Cube2";
	static const std::string VAO_CUBE_TOP = "Cube3";
	static const std::string VAO_CUBE_BOTTOM = "Cube4";
	static const std::string VAO_BACK_PLANE = "Plane";
	#pragma endregion


	#pragma region Shaders and Programs

	static const std::string PR_CHIP8 = "Shader Chip-8";
	
	static const std::string VERTEX_SOURCE = R"(
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

	static const std::string FRAGMENT_SOURCE = R"(
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

	static unsigned int CreateAndLinkProgram(const std::initializer_list<unsigned int>& shaders)
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
		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<char> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

			// We don't need the program anymore.
			glDeleteProgram(program);

			// Don't leak shaders either.
			for (const auto s : shaders)
			{
				glDeleteShader(s);
			}

			// Use the infoLog as you see fit.
			C8_ERRO("Program linking failed: {0}", infoLog.data());

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

	static unsigned int LoadShader(unsigned int type, const std::string& source)
	{
		// Create an empty vertex shader handle
		unsigned int shader = glCreateShader(type);

		// Send the vertex shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		const GLchar *src = (const GLchar *)source.c_str();
		glShaderSource(shader, 1, &src, 0);

		// Compile the vertex shader
		glCompileShader(shader);

		GLint isCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<char> infoLog(maxLength);
			glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

			// We don't need the shader anymore.
			glDeleteShader(shader);

			// Use the infoLog as you see fit.
			C8_ERRO("Shader compilation failed: {0}", infoLog.data());

			// In this simple program, we'll just leave

			return 0;
		}

		return shader;
	}

	#pragma endregion


	static const std::string Chip8_ConfigurationFile = "config.bin";
	static const std::string Chip8_LogFile = "chip8.log";

	static Application* GetApplication(GLFWwindow* window)
	{
		return static_cast<Application*>(glfwGetWindowUserPointer(window));
	}

	static void GLFW_WindowSize(GLFWwindow* window, int w, int h)
	{
		GetApplication(window)->OnResize(w, h);
	}


	static void GLFW_Key(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto app = GetApplication(window);
		if (action == GLFW_PRESS || action == GLFW_REPEAT)
		{
			app->OnKeyPressed(key);
		}
		else
		{
			app->OnKeyReleased(key);
		}
	}



	Application::Application()
	{
		// Init Chip
		m_Chip8 = std::make_shared<Chip8>();

		// Init screen interface
		m_Screen = std::make_unique<Screen>(m_Chip8, 0.2f);
		
		// Init sound
		m_Beep = std::make_unique<Beep>();
		m_Beep->Start([this]() -> float {
			return (m_Chip8->GetSoundTimer() > 0 ? 1.0f : 0.0f) * m_Config.Volume;
		});


	}

	Application::~Application()
	{
		for (auto vb : m_Vbs)
			GL_CALL(glDeleteBuffers(1, &vb));

		for (const auto &vao : m_Vaos)
			GL_CALL(glDeleteVertexArrays(1, &vao.second));
		
		for (const auto &s : m_Programs)
			GL_CALL(glDeleteProgram(s.second));

		glfwTerminate();


	}

	template<typename T, size_t N>
	void Application::CreateVertexArray(const std::string & id, const std::array<T, N>& vertices)
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

		m_Vaos[id] = vao;
		m_Vbs.push_back(vb);

	}

	bool Application::Start()
	{
		/* Setup spdlog */
		#ifndef DEBUG
			spdlog::set_default_logger(spdlog::rotating_logger_mt("chip8", Chip8_LogFile, 512 * 1024, 0, false));
		#endif

		/* Load configuration */
		LoadConfiguration();

		/* Initialize the library */
		if (!glfwInit())
		{
			C8_ERRO("Could not initialize GLFW");
			return false;
		}

		/* Create a windowed mode window and its OpenGL context */
		m_Window = glfwCreateWindow(640, 480, "Chip-8", NULL, NULL);
		if (!m_Window)
		{
			C8_ERRO("Could not create GLFW window");
			glfwTerminate();
			return false;
		}

		/* Make the window's context current */
		glfwMakeContextCurrent(m_Window);
		glfwSetWindowUserPointer(m_Window, this);
		gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

		/* GLFW callbacks */
		glfwSetWindowSizeCallback(m_Window, &GLFW_WindowSize);
		glfwSetKeyCallback(m_Window, &GLFW_Key);

		/* Init graphics resources */
		InitGraphics();

		/* Initialize ImGui */

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
		ImGui_ImplOpenGL3_Init("#version 130");

		/* Load rom filenames */
		LoadRomFilenames();

		/* Main loop */
		bool soundReady = true;
		auto prevTime = std::chrono::high_resolution_clock::now();
		auto currTime = std::chrono::high_resolution_clock::now();
		float timeAcc = 0.0f;

		while (!glfwWindowShouldClose(m_Window))
		{
			currTime = std::chrono::high_resolution_clock::now();
			std::chrono::duration<float> deltaTime = currTime - prevTime;
			prevTime = currTime;
			float dt = deltaTime.count();
			float clockPeriod = 1.0f / m_Config.ClockFrequency;


			// Update CHIP8
			timeAcc += dt;
			while (timeAcc >= clockPeriod)
			{
				timeAcc -= clockPeriod;
				m_Chip8->ClockCycle(clockPeriod);
				m_Screen->Update(clockPeriod);
			}


			// Render
			m_Config.ViewMode == EViewMode::Noraml ? Render() : Render3D();

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			RenderImGui();

			ImGui::EndFrame();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			/* Swap front and back buffers */
			glfwSwapBuffers(m_Window);

			/* Poll for and process events */
			glfwPollEvents();
		}

		StoreConfiguration();

		return 0;
	}

	void Application::OnKeyPressed(int key)
	{
		uint8_t k;
		if (GetChip8Key(key, k))
		{
			m_Chip8->SetKeyState(k, true);
		}
	}

	void Application::OnKeyReleased(int key)
	{

		uint8_t k;
		if (GetChip8Key(key, k))
		{
			m_Chip8->SetKeyState(k, false);
		}
	}

	void Application::OnResize(int w, int h)
	{
		glViewport(0, 0, w, h);
	}

	void Application::LoadFromFile(const std::string & path)
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

		m_Chip8->Load(buffer, length);

		delete[] buffer;

		m_CurrentRomFile = path;

	}

	void Application::Load(void * data, size_t length)
	{
		m_Chip8->Load(data, length);
	}

	void Application::Reset()
	{
		if (!m_CurrentRomFile.empty())
		{
			LoadFromFile(m_CurrentRomFile);
		}
	}

	void Application::LoadRomFilenames()
	{
		namespace fs = std::filesystem;

		m_RomFiles.clear();

		fs::path path = fs::absolute("roms");

		for (auto f : fs::directory_iterator(path))
		{
			if (fs::is_regular_file(f.path()))
			{
				m_RomFiles.push_back({
					f.path().filename().string(),
					f.path().string()
				});
			}
		}

	}


	void Application::InitGraphics()
	{

		m_Programs[PR_CHIP8] = CreateAndLinkProgram({ LoadShader(GL_VERTEX_SHADER, VERTEX_SOURCE), LoadShader(GL_FRAGMENT_SHADER, FRAGMENT_SOURCE) });
		
		float w = m_Chip8->GetScreenWidth(), h = m_Chip8->GetScreenHeight();

		std::array<float, 6 * 6> cubeFront = {
			// Front face
			-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
			0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
			0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
			-0.5f, -0.5, 0.5f, 0.0f, 0.0f, 1.0f,
			0.5f, 0.5, 0.5f, 0.0f, 0.0f, 1.0f,
			-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
		};

		std::array<float, 6 * 6> cubeLeft = {
			// Right face
			-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
			-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
		};

		std::array<float, 6 * 6> cubeRight = {
			// Right face
			0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
		};

		std::array<float, 6 * 6> cubeTop = {
			// Top face
			-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		};

		std::array<float, 6 * 6> cubeBottom = {
			// Bottom face
			-0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
			0.5f,  -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
			0.5f,  -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
			0.5f,  -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
			-0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
			-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
		};

		std::array<float, 6 * 6> backplaneVertices = {
			-w / 2, -h / 2, 0, 0, 0, 1,
			w / 2, -h / 2, 0, 0, 0, 1,
			w / 2, h / 2, 0, 0, 0, 1,
			-w / 2, -h / 2, 0, 0, 0, 1,
			w / 2, h / 2, 0, 0, 0, 1,
			-w / 2, h / 2, 0, 0, 0, 1
		};

		// Create cube VAO

		CreateVertexArray(VAO_CUBE_FRONT, cubeFront);
		CreateVertexArray(VAO_CUBE_LEFT, cubeLeft);
		CreateVertexArray(VAO_CUBE_RIGHT, cubeRight);
		CreateVertexArray(VAO_CUBE_TOP, cubeTop);
		CreateVertexArray(VAO_CUBE_BOTTOM, cubeBottom);

		// Create back plane
		CreateVertexArray(VAO_BACK_PLANE, backplaneVertices);


		
	}

	bool Application::GetChip8Key(int key, uint8_t & result)
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

		return false;
	}

	void Application::Render()
	{
		int wh, ww;
		int ew = m_Chip8->GetScreenWidth(), eh = m_Chip8->GetScreenHeight();

		glfwGetWindowSize(m_Window, &ww, &wh);

		float screenAspect = float(ww) / wh;
		float emulatorAspect = float(ew) / eh;

		float vw, vh;

		if (screenAspect > emulatorAspect)
		{
			// Adapt with height

			vh = float(eh);
			vw = vh * screenAspect;

		}
		else
		{
			// Adapt with width
			vw = float(ew);
			vh = vw / screenAspect;
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
		glTranslatef(-m_Chip8->GetScreenWidth() / 2.0f, -m_Chip8->GetScreenHeight() / 2.0f, 0);
		
		glColor3fv(glm::value_ptr(m_Config.BackColor));
		glBegin(GL_QUADS);
		{
			glVertex2i(0, 0);
			glVertex2i(ew, 0);
			glVertex2i(ew, eh);
			glVertex2i(0, eh);
		}
		glEnd();

		for (int x = 0; x < m_Chip8->GetScreenWidth(); x++)
		{
			for (int y = 0; y < m_Chip8->GetScreenHeight(); y++)
			{
				int ny = m_Chip8->GetScreenHeight() - y - 1;
				glm::vec4 color = { m_Config.FrontColor , m_Screen->GetPixel(x, y) };

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

	void Application::Render3D()
	{
		int ww, wh;
		glfwGetWindowSize(m_Window, &ww, &wh);

		int ew = m_Chip8->GetScreenWidth(), eh = m_Chip8->GetScreenHeight();

		float emulatorAspect = float(ew) / eh;
		float screenAspect = float(ww) / wh;

		float vw, vh;

		if (screenAspect > emulatorAspect)
		{
			// Adapt with height
			vh = float(eh);
			vw = vh * screenAspect;
		}
		else
		{
			// Adapt with width
			vw = float(ew);
			vh = vw / screenAspect;
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

		GL_CALL(glUseProgram(m_Programs[PR_CHIP8]));

		glm::mat4 projection = glm::perspective(fovY, screenAspect, 0.1f, 1000.0f);
		glm::vec3 lightDir = glm::normalize(glm::vec3(0, 0, 1));
		glm::vec3 ambientLight = { 0.2f, 0.2f, 0.2f };

		GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uProjection"), 1, GL_FALSE, glm::value_ptr(projection)));
		GL_CALL(glUniform3fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uLightDirection"), 1, glm::value_ptr(lightDir)));
		GL_CALL(glUniform3fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uAmbientLight"), 1, glm::value_ptr(ambientLight)));

		glm::mat4 modelView;
		modelView = glm::translate(glm::identity<glm::mat4>(), { 0, 0, -(distance + 1.0f) });
		glm::vec4 color = { m_Config.BackColor, 1.0f };


		glBindVertexArray(m_Vaos[VAO_BACK_PLANE]);
		GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uModelView"), 1, GL_FALSE, glm::value_ptr(modelView)));
		GL_CALL(glUniform4fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uColor"), 1, glm::value_ptr(color)));
		GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));



		for (int x = 0; x < ew; x++)
		{
			for (int y = 0; y < eh; y++)
			{
				float pixel = m_Screen->GetPixel(x, y);

				if (pixel > 0)
				{
					color = { m_Config.FrontColor, pixel };
					modelView = glm::translate(glm::identity<glm::mat4>(), { -ew / 2 + x + 0.5f, eh / 2 - y - 0.5f, -(distance + 0.5f) });

					GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uModelView"), 1, GL_FALSE, glm::value_ptr(modelView)));
					GL_CALL(glUniform4fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uColor"), 1, glm::value_ptr(color)));

					if (x > 0 && m_Screen->GetPixel(x - 1, y) == 0)
					{
						glBindVertexArray(m_Vaos[VAO_CUBE_LEFT]);
						glDrawArrays(GL_TRIANGLES, 0, 6);
					}

					if (x < ew - 1 && m_Screen->GetPixel(x + 1, y) == 0)
					{
						glBindVertexArray(m_Vaos[VAO_CUBE_RIGHT]);
						glDrawArrays(GL_TRIANGLES, 0, 6);
					}

					if (y > 0 && m_Screen->GetPixel(x, y - 1) == 0)
					{
						glBindVertexArray(m_Vaos[VAO_CUBE_TOP]);
						glDrawArrays(GL_TRIANGLES, 0, 6);
					}

					if (y < eh - 1 && m_Screen->GetPixel(x, y + 1) == 0)
					{
						glBindVertexArray(m_Vaos[VAO_CUBE_BOTTOM]);
						glDrawArrays(GL_TRIANGLES, 0, 6);

					}


				}
			}
		}

		for (int x = 0; x < ew; x++)
		{
			for (int y = 0; y < eh; y++)
			{
				float pixel = m_Screen->GetPixel(x, y);
				glm::vec4 color = { m_Config.FrontColor, pixel };
				modelView = glm::translate(glm::identity<glm::mat4>(), { -ew / 2 + x + 0.5f, eh / 2 - y - 0.5f, -(distance + 0.5f) });

				GL_CALL(glUniformMatrix4fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uModelView"), 1, GL_FALSE, glm::value_ptr(modelView)));
				GL_CALL(glUniform4fv(glGetUniformLocation(m_Programs[PR_CHIP8], "uColor"), 1, glm::value_ptr(color)));

				glBindVertexArray(m_Vaos[VAO_CUBE_FRONT]);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		}

		glUseProgram(0);
	}

	void Application::RenderImGui()
	{
		ImGui::BeginMainMenuBar();
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::BeginMenu("Load ROM..."))
				{
					for (const auto& romFile : m_RomFiles)
					{
						if (ImGui::MenuItem(romFile.fileName.c_str()))
						{
							LoadFromFile(romFile.path);;
						}
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Reset"))
				{
					Reset();
				}

				if (ImGui::BeginMenu("Options"))
				{
					ImGui::SliderInt("Clock Freq. (hz)", &m_Config.ClockFrequency, 60, 1000);
					ImGui::SliderFloat("Volume", &m_Config.Volume, 0.0f, 1.0f, "%.1f");
					ImGui::ColorEdit3("Front Color", glm::value_ptr(m_Config.FrontColor));
					ImGui::ColorEdit3("Back Color", glm::value_ptr(m_Config.BackColor));
					ImGui::Combo("View Mode", (int*)&m_Config.ViewMode, "Normal\0Voxel");
					if (ImGui::Button("Restore defaults", { -1, 0 }))
					{
						m_Config = Config();
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
				{
					glfwSetWindowShouldClose(m_Window, true);
				}

				ImGui::EndMenu();

			}
		}
		ImGui::EndMainMenuBar();

	}

	void Application::LoadConfiguration()
	{
		std::ifstream is(Chip8_ConfigurationFile, std::ios::binary);

		if (is.good())
		{
			is.read((char*)&m_Config, sizeof(Config));
		}

		is.close();


	}

	void Application::StoreConfiguration()
	{
		std::ofstream os(Chip8_ConfigurationFile, std::ios::binary);
		os.write((char*)&m_Config, sizeof(Config));
		os.close();
	}

	Screen::Screen(const std::shared_ptr<Chip8>& chip8, float fadeTime) :
		m_Chip(chip8),
		m_FadeTime(fadeTime)
	{
		m_ScreenData = std::make_unique<float[]>(chip8->GetScreenWidth() * chip8->GetScreenHeight());

		for (uint32_t x = 0; x < m_Chip->GetScreenWidth(); x++)
			for (uint32_t y = 0; y < m_Chip->GetScreenHeight(); y++)
				m_ScreenData[y * m_Chip->GetScreenWidth() + x] = 0.0f;

	}


	void Screen::Update(float dt)
	{
		float fadeAmount = dt / (m_FadeTime + std::numeric_limits<float>::epsilon());
		for (uint32_t x = 0; x < m_Chip->GetScreenWidth(); x++)
		{
			for (uint32_t y = 0; y < m_Chip->GetScreenHeight(); y++)
			{
				auto &v = m_ScreenData[y * m_Chip->GetScreenWidth() + x];

				if (m_Chip->GetPixel(x, y))
					v = 1.0f;
				else
					v = std::max(0.0f, v - fadeAmount);
			}
		}
	}

	float Screen::GetPixel(uint8_t x, uint8_t y) const
	{
		if (m_FadeTime == 0)
			return m_Chip->GetPixel(x, y);
		else
			return m_ScreenData[y * m_Chip->GetScreenWidth() + x];
	}
}