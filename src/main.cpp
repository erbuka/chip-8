#include "application.h"
#include "chip8.h"

static int EntryPoint()
{
	c8::application app;
	return app.start();
}
#ifdef DEBUG
	int main(int argc, char **argv)
	{
		return EntryPoint();
	}
#else
	#ifdef _WIN32
		
		#include <Windows.h>

		int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
		{
			return EntryPoint();
		}

	#else 
		int main(int argc, char **argv)
		{
			return EntryPoint();
		}
	#endif
#endif