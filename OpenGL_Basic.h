#pragma once

// include basic libraries to make OpenGL work on Windows
#include <Windows.h>
#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

namespace def
{
	// Struct that helps to handle state of each key
	struct KeyState
	{
		bool bPressed;
		bool bReleased;
		bool bHeld;
	};
	
	class OpenGL_Basic
	{
	private:

		// We don't handle keyboard keys or mouse buttons here, because this method is static
		static LRESULT CALLBACK WinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			switch (msg)
			{
			case WM_DESTROY:
				PostQuitMessage(0);
				DestroyWindow(hWnd);
				return 0;
			}

			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

		// These methods do some OpenGL stuff for Windows
		void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC)
		{
			PIXELFORMATDESCRIPTOR pfd;

			/* get the device context (DC) */
			*hDC = GetDC(hwnd);

			/* set the pixel format for the DC */
			ZeroMemory(&pfd, sizeof(pfd));

			pfd.nSize = sizeof(pfd);
			pfd.nVersion = 1;
			pfd.dwFlags = PFD_DRAW_TO_WINDOW |
				PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.cColorBits = 24;
			pfd.cDepthBits = 16;
			pfd.iLayerType = PFD_MAIN_PLANE;

			SetPixelFormat(*hDC, ChoosePixelFormat(*hDC, &pfd), &pfd);

			/* create and enable the render context (RC) */
			*hRC = wglCreateContext(*hDC);

			wglMakeCurrent(*hDC, *hRC);
		}

		void DisableOpenGL(HWND hwnd, HDC hDC, HGLRC hRC)
		{
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(hRC);
			ReleaseDC(hwnd, hDC);
		}

	public:
		virtual bool OnBeforeMainLoop() = 0;
		virtual bool OnFrameUpdate() = 0;

		bool Run(int screen_width, int screen_height, const wchar_t* sTitle, bool bFullScreen = false)
		{
			nScreenWidth = screen_width;
			nScreenHeight = screen_height;

			// Prepare window

			WNDCLASS wc;
			wc.style = CS_HREDRAW | CS_VREDRAW;
			wc.cbClsExtra = 0;
			wc.cbWndExtra = 0;

			wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

			wc.hIcon = LoadIcon(GetModuleHandle(NULL), nullptr);

			wc.lpszClassName = sTitle;
			wc.lpszMenuName = nullptr;

			wc.hInstance = GetModuleHandle(NULL);

			wc.lpfnWndProc = WinProc;

			RegisterClass(&wc);

			DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
			DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_THICKFRAME;

			HDC hDC;
			HGLRC hRC;

			float fTopLeftX = CW_USEDEFAULT;
			float fTopLeftY = CW_USEDEFAULT;

			hWnd = nullptr;

			if (bFullScreen)
			{
				dwExStyle = 0;
				dwStyle = WS_VISIBLE | WS_POPUP;

				HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
				MONITORINFO mi = { sizeof(mi) };

				if (!GetMonitorInfo(hMon, &mi)) return 1;

				nScreenWidth = mi.rcMonitor.right;
				nScreenHeight = mi.rcMonitor.bottom;

				fTopLeftX = 0.0f;
				fTopLeftY = 0.0f;
			}

			hWnd = CreateWindowEx(dwExStyle, sTitle, sTitle, dwStyle,
				fTopLeftX, fTopLeftY, nScreenWidth, nScreenHeight, nullptr, nullptr, GetModuleHandle(NULL), nullptr);

			if (!hWnd)
			{
				MessageBox(0, L"Failed To Create Window!", 0, 0);
				return false;
			}

			ShowWindow(hWnd, SW_SHOW);

			// Prepare OpenGL

			EnableOpenGL(hWnd, &hDC, &hRC);
			
			glEnable(GL_DEPTH_TEST);

			glFrustum(-1, 1, -1, 1, 2, 80);

			if (!OnBeforeMainLoop())
				return false;

			for (int i = 0; i < 256; i++)
				keys[i] = { false, false , false };

			SetWindowText(hWnd, sTitle);

			MSG msg = { 0 };
			while (msg.message != WM_QUIT)
			{
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (msg.message == WM_QUIT)
						break;
					else
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
				else
				{
					// Handle state of each key
					for (int i = 0; i < 256; i++)
					{
						keyNewState[i] = GetAsyncKeyState(i);

						keys[i].bPressed = false;
						keys[i].bReleased = false;

						if (keyNewState[i] != keyOldState[i])
						{
							if (keyNewState[i] & 0x8000)
							{
								keys[i].bPressed = !keys[i].bHeld;
								keys[i].bHeld = true;
							}
							else
							{
								keys[i].bReleased = true;
								keys[i].bHeld = false;
							}
						}

						keyOldState[i] = keyNewState[i];
					}

					glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

					glPushMatrix();

					if (!OnFrameUpdate())
						return false;

					glPopMatrix();

					SwapBuffers(hDC);

					Sleep(1);
				}
			}

			DisableOpenGL(hWnd, hDC, hRC);
		}

	private:
		int nScreenWidth;
		int nScreenHeight;

		KeyState keys[256];

		short keyNewState[256]{ 0 };
		short keyOldState[256]{ 0 };

		HWND hWnd;

	public:
		KeyState GetKey(short key)
		{
			return keys[key];
		}

		int GetScreenWidth()
		{
			return nScreenWidth;
		}

		int GetScreenHeight()
		{
			return nScreenHeight;
		}
	};
}
