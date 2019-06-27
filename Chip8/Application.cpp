#include "Application.h"

#include "Chip8.h"
#include "Beep.h"
#include "Macros.h"


#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <imgui.h>
#include <imgui_impl/imgui_impl_opengl3.h>
#include <imgui_impl/imgui_impl_glfw.h>

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
		m_Chip8 = std::make_unique<Chip8>();
		m_Beep = std::make_unique<Beep>();

		m_Beep->Start([this]() -> float {
			return (m_Chip8->GetSoundTimer() > 0 ? 1.0f : 0.0f) * m_Config.Volume;
		});

	}

	Application::~Application()
	{
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
		gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

		glfwSetWindowUserPointer(m_Window, this);


		/* GLFW callbacks */
		glfwSetWindowSizeCallback(m_Window, &GLFW_WindowSize);
		glfwSetKeyCallback(m_Window, &GLFW_Key);

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
			}


			// Render
			Render();

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

		glfwTerminate();
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
		
		glClear(GL_COLOR_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-vw / 2, vw / 2, -vh / 2, vh / 2, 1, -1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glTranslatef(-m_Chip8->GetScreenWidth() / 2.0f, -m_Chip8->GetScreenHeight() / 2.0f, 0);


		for (int x = 0; x < m_Chip8->GetScreenWidth(); x++)
		{
			for (int y = 0; y < m_Chip8->GetScreenHeight(); y++)
			{
				int ny = m_Chip8->GetScreenHeight() - y - 1;
				glBegin(GL_QUADS);
				{
					glColor3fv(m_Chip8->GetPixel(x, y) ? glm::value_ptr(m_Config.FrontColor) : glm::value_ptr(m_Config.BackColor));
					glVertex2i(x, ny);
					glVertex2i(x + 1, ny);
					glVertex2i(x + 1, ny + 1);
					glVertex2i(x, ny + 1);
				}
				glEnd();
			}
		}
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
}