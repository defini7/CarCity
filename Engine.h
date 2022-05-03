#pragma once

#include <Windows.h>
#include <iostream>
#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

#define HInstance() GetModuleHandle(NULL)

namespace def
{
	struct KeyState
	{
		bool bPressed;
		bool bReleased;
		bool bHeld;
	};

	struct vf3d
	{
		float x;
		float y;
		float z;
	};

	struct vf2d
	{
		float x;
		float y;
	};

	struct vi2d
	{
		int x;
		int y;

		friend bool operator<(const vi2d& lhs, const vi2d& rhs)
		{
			return lhs.x < rhs.x && lhs.y < rhs.y;
		}
	};

	const double PI = 2.0 * acos(0.0);

	class Poison
	{
	public:
		Poison()
		{
			sTitle = L"Poison Sample";
			
			bFullScreen = false;

			fMouseX = 0.0f;
			fMouseY = 0.0f;
		}

		virtual ~Poison()
		{

		}

	public:
		virtual bool Start() = 0;
		virtual bool Update() = 0;
		virtual void Destroy()
		{
			return;
		}

	private:
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
		bool Run(int screen_width, int screen_height, std::wstring title, bool full_screen = false)
		{
			nScreenWidth = screen_width;
			nScreenHeight = screen_height;

			bFullScreen = full_screen;

			sTitle = title;

			WNDCLASS wc;
			wc.style = CS_HREDRAW | CS_VREDRAW;
			wc.cbClsExtra = 0;
			wc.cbWndExtra = 0;

			wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

			wc.hIcon = LoadIcon(HInstance(), nullptr);

			wc.lpszClassName = sTitle.c_str();
			wc.lpszMenuName = nullptr;

			wc.hInstance = HInstance();

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

			hWnd = CreateWindowEx(dwExStyle, sTitle.c_str(), sTitle.c_str(), dwStyle,
				fTopLeftX, fTopLeftY, nScreenWidth, nScreenHeight, nullptr, nullptr, HInstance(), nullptr);

			if (!hWnd)
			{
				MessageBox(0, L"Failed To Create Window!", 0, 0);
				return false;
			}

			ShowWindow(hWnd, SW_SHOW);

			EnableOpenGL(hWnd, &hDC, &hRC);
			
			glEnable(GL_DEPTH_TEST);

			glFrustum(-1, 1, -1, 1, 2, 80);

			if (!Start())
				return false;

			for (int i = 0; i < 256; i++)
				keys[i] = { false, false , false };

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

					POINT cursor;
					GetCursorPos(&cursor);
					ScreenToClient(hWnd, &cursor);

					fMouseX = cursor.x;
					fMouseY = cursor.y;

					SetWindowText(hWnd, LPWSTR(sTitle.c_str()));

					glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

					glPushMatrix();

					if (!Update())
						return false;

					glPopMatrix();

					SwapBuffers(hDC);

					Sleep(1);
				}
			}

			Destroy();
			DisableOpenGL(hWnd, hDC, hRC);
		}

	private:
		int nScreenWidth;
		int nScreenHeight;

		float fMouseX;
		float fMouseY;

		KeyState keys[256];

		short keyNewState[256]{ 0 };
		short keyOldState[256]{ 0 };

		std::wstring sTitle;

		bool bFullScreen;

		HWND hWnd;

	public:
		bool IsFocused()
		{
			return GetForegroundWindow() == hWnd;
		}

		KeyState GetKey(short key)
		{
			return keys[key];
		}

		float GetMouseX()
		{
			return fMouseX;
		}

		float GetMouseY()
		{
			return fMouseY;
		}

		vf2d GetMouse()
		{
			return vf2d(fMouseX, fMouseY);
		}

		int GetScreenWidth()
		{
			return nScreenWidth;
		}

		int GetScreenHeight()
		{
			return nScreenHeight;
		}

		vi2d GetScreenSize()
		{
			return vi2d(nScreenWidth, nScreenHeight);
		}

	};

}
