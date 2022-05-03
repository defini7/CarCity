#define _CRT_SECURE_NO_WARNINGS

#include "Engine.h"

#include <map>
#include <fstream>

// I am using stb_image as image loader
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Include Lua 5.4.2 library
extern "C"
{
#include "lua542/include/lua.h"
#include "lua542/include/lauxlib.h"
#include "lua542/include/lualib.h"
}

// It's only for Visual Studio
#ifdef _WIN32
#pragma comment(lib, "lua542/liblua54.a")
#endif

/*
* Camera only has a position
* and could not be rotated
*/
struct sCamera
{
	def::vf3d pos;
};

/*
* Here could be multiple players.
* Every player has a position and could be rotated.
*/
struct sPlayer
{
	def::vf2d pos;

	float angle;
	float acceleration;
};

/* 
* Every platform is a sTile.
* Each has a texture and could be rotated.
*/
struct sTile
{
	bool isEmpty;
	int height;

	int textureIndex;
	GLuint texture;
	float textureAngle; // in degrees
};

// Every tile in a game
enum TILES
{
	GRASS,
	ROAD_TILE,
	ROAD,
	ROAD_CROSSWALK,
	INTERSECTION,
	RIGHT_ROAD,
	BUILDING1_FLOOR,
	BUILDING1_ROOF,
	CAR // keep it always at the bottom
};

class Example : public def::Poison
{
public:
	Example(lua_State* state, int world_width, int world_height)
	{
		L = state;

		nWorldWidth = world_width;
		nWorldHeight = world_height;
	}

protected:
	bool Start() override
	{
		auto get_from_table = [&](const char* key)
		{
			lua_pushstring(L, key);
			lua_gettable(L, -2);
			const char* value = lua_tostring(L, -1);
			lua_pop(L, 1);
			return value;
		};

		// Initialization of map
		for (int i = -nWorldWidth * nWorldHeight; i < nWorldWidth * nWorldHeight; i++)
			tWorld[i] = { true, 0, 0, GRASS, 0.0f };

		player.pos.x = 0.0f;
		player.pos.y = 0.0f;

		player.angle = 0.0f;

		camera.pos.x = 0.0f;
		camera.pos.y = 0.0f;
		camera.pos.z = 10.0f;

		// Loading textures
		lua_getglobal(L, "Textures");

		if (lua_istable(L, -1))
		{
#define t(name) get_from_table(name)
			textures[GRASS] = LoadTexture(t("Grass"));
			textures[ROAD_TILE] = LoadTexture(t("RoadTile"));
			textures[ROAD] = LoadTexture(t("Road"));
			textures[ROAD_CROSSWALK] = LoadTexture(t("RoadCrosswalk"));
			textures[INTERSECTION] = LoadTexture(t("Intersection"));
			textures[RIGHT_ROAD] = LoadTexture(t("RightRoad"));
			textures[CAR] = LoadTexture(t("Car"), true);

			textures[BUILDING1_FLOOR] = LoadTexture(t("Building1Floor"));
			textures[BUILDING1_ROOF] = LoadTexture(t("Building1Roof"));
#undef t
		}

		// Loading level
		LoadLevel();

		return true;
	}

	bool Update() override
	{
		camera.pos.x = player.pos.x;
		camera.pos.y = player.pos.y;

		glTranslatef(-camera.pos.x, -camera.pos.y, -camera.pos.z);

		// Build a building
		if (GetKey(VK_LBUTTON).bPressed)
		{
			int p = vSelectedArea.y * nWorldWidth + vSelectedArea.x;

			tWorld[p].isEmpty = false;
			tWorld[p].height++;

			if (tWorld[p].height > 20)
				tWorld[p].height = 20;
		}

		// Break a building
		if (GetKey(VK_RBUTTON).bPressed)
		{
			int p = vSelectedArea.y * nWorldWidth + vSelectedArea.x;

			tWorld[p].height--;

			if (tWorld[p].height <= 0)
			{
				tWorld[p].isEmpty = true;
				tWorld[p].height = 0;
			}
		}

		// Change tile in selected area
		if (GetKey(L'E').bPressed)
		{
			int p = vSelectedArea.y * nWorldWidth + vSelectedArea.x;

			if (tWorld[p].textureIndex < 5)
				tWorld[p].texture = textures[++tWorld[p].textureIndex];
		}

		if (GetKey(L'Q').bPressed)
		{
			int p = vSelectedArea.y * nWorldWidth + vSelectedArea.x;

			if (tWorld[p].textureIndex > 0)
				tWorld[p].texture = textures[--tWorld[p].textureIndex];
		}

		// Rotate tile in selected area
		if (GetKey(L'O').bPressed)
		{
			int p = vSelectedArea.y * nWorldWidth + vSelectedArea.x;
			tWorld[p].textureAngle -= 90.0f;
		}

		if (GetKey(L'P').bPressed)
		{
			int p = vSelectedArea.y * nWorldWidth + vSelectedArea.x;
			tWorld[p].textureAngle += 90.0f;
		}

		// Zoom world
		if (GetKey(L'Z').bHeld)
			camera.pos.z += 0.1f;

		if (GetKey(L'X').bHeld)
			camera.pos.z -= 0.1f;

		if (camera.pos.z < 5.0f)
			camera.pos.z = 5.0f;

		if (camera.pos.z > 17.5f)
			camera.pos.z = 17.5f;

		// Move player
		if (GetKey(VK_LEFT).bHeld)
			player.angle -= (float)def::PI;

		if (GetKey(VK_RIGHT).bHeld)
			player.angle += (float)def::PI;

		auto calculate_possible = [&](float& x, float& y)
		{
			float fInRadians = player.angle * (float)def::PI / 180.0f;

			x = sinf(fInRadians) * 0.1f;
			y = cosf(fInRadians) * 0.1f;
		};

		if (GetKey(VK_UP).bHeld)
		{
			float fPossibleX;
			float fPossibleY;

			calculate_possible(fPossibleX, fPossibleY);

			player.pos.x += fPossibleX;
			player.pos.y += fPossibleY;

			if (player.acceleration < 0.95f)
				player.acceleration += 0.05f;
		}
		else
		{
			float fPossibleX;
			float fPossibleY;

			calculate_possible(fPossibleX, fPossibleY);

			player.pos.x += fPossibleX * player.acceleration;
			player.pos.y += fPossibleY * player.acceleration;

			if (player.acceleration > 0.05f)
				player.acceleration -= 0.05f;
		}

		// Move selected area...
		if (GetKey(L'A').bPressed)
			vSelectedArea.x -= 1;

		if (GetKey(L'D').bPressed)
			vSelectedArea.x += 1;

		if (GetKey(L'S').bPressed)
			vSelectedArea.y -= 1;

		if (GetKey(L'W').bPressed)
			vSelectedArea.y += 1;

		// ... and draw it
		DrawSelectedArea();

		// Draw the whole world
		for (int i = -nWorldWidth / 2; i < nWorldWidth / 2; i++)
			for (int j = -nWorldHeight / 2; j < nWorldHeight / 2; j++)
			{
				int p = j * nWorldWidth + i;

				if (i > player.pos.x + 10 || j > player.pos.y + 10 || i < player.pos.x - 10 || j < player.pos.y - 10)
					continue;

				if (!tWorld[p].isEmpty && tWorld[p].height > 0)
				{
					for (int k = 0; k < tWorld[p].height; k++)
						DrawBuildingFloor(i, j, (float)k, textures[BUILDING1_FLOOR]);
					
					glPushMatrix();
					DrawTile(i, j, (float)tWorld[p].height + 1.0f - 0.99f, textures[BUILDING1_ROOF], tWorld[p].textureAngle, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
					glPopMatrix();
				}
				
				glPushMatrix();
				DrawTile(i, j, 0.0f, textures[tWorld[p].texture - 1], tWorld[p].textureAngle, { -1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f });
				glPopMatrix();
			}

		// Draw player
		glPushMatrix();
			glTranslatef(-0.5f, -0.5f, 0.0f);
			DrawTile(player.pos.x, player.pos.y, 0.01f, textures[CAR], -player.angle + 90.0f, { -0.9f, 0.5f, 0.0f }, { 0.9f, 0.5f, 0.0f }, { 0.9f, -0.5f, 0.0f }, { -0.9f, -0.5f, 0.0f });
		glPopMatrix();

		return true;
	}

	void Destroy() override
	{
		glDisable(GL_TEXTURE_2D);
	}

private:
	lua_State* L = nullptr;

	std::map<int, sTile> tWorld;
	def::vi2d vSelectedArea;

	GLuint textures[9];

	sCamera camera;
	sPlayer player;

	int nWorldWidth;
	int nWorldHeight;

	bool LoadLevel()
	{
		// Get map from config.lua and fill with that a std::map

		lua_getglobal(L, "WorldMap");

		if (lua_istable(L, -1))
		{
			int x = -nWorldWidth / 2;
			int y = -nWorldHeight / 2;

			for (int k = nWorldHeight / 2; k > -nWorldHeight / 2; k--)
			{
				lua_pushnumber(L, k);
				lua_gettable(L, -2);
				const char* pMapLine = lua_tostring(L, -1);
				lua_pop(L, 1);

				for (int i = 0; i < nWorldWidth; i++, x++)
				{
					int p = y * nWorldWidth + x;
					switch (pMapLine[i])
					{
					case '-':
						tWorld[p].texture = textures[ROAD];
						tWorld[p].textureIndex = ROAD;
						break;

					case '|':
						tWorld[p].texture = textures[ROAD];
						tWorld[p].textureAngle = -90.0f;
						tWorld[p].textureIndex = ROAD;
						break;

					case '@':
						tWorld[p].texture = textures[ROAD_TILE];
						tWorld[p].textureIndex = ROAD_TILE;
						break;

					case '=':
						tWorld[p].texture = textures[ROAD_CROSSWALK];
						tWorld[p].textureIndex = ROAD_CROSSWALK;
						break;

					case '"':
						tWorld[p].texture = textures[ROAD_CROSSWALK];
						tWorld[p].textureAngle = -90.0f;
						tWorld[p].textureIndex = ROAD_CROSSWALK;
						break;

					case '>':
						tWorld[p].texture = textures[RIGHT_ROAD];
						tWorld[p].textureIndex = RIGHT_ROAD;
						break;

					case '<':
						tWorld[p].texture = textures[RIGHT_ROAD];
						tWorld[p].textureAngle = -90.0f;
						tWorld[p].textureIndex = RIGHT_ROAD;
						break;

					case '+':
						tWorld[p].texture = textures[INTERSECTION];
						tWorld[p].textureIndex = INTERSECTION;
						break;

					case '#':
						tWorld[p].texture = textures[GRASS];
						tWorld[p].textureIndex = GRASS;
						break;

					case '\0':
						break;

					default:
						if (std::isdigit(pMapLine[i]))
						{
							tWorld[p].height = (int)pMapLine[i] - 48;
							tWorld[p].isEmpty = false;
						}
						else
							return true;
					}

				}

				x = -nWorldWidth / 2;
				if (++y == nWorldHeight / 2)
					return true;
			}
		}
		
		return true;
	}

	GLuint LoadTexture(const char* sFilename, bool bClamp = false)
	{
		int nWidth, nHeight, nChannels;

		stbi_set_flip_vertically_on_load(1);
		unsigned char* data = stbi_load(sFilename, &nWidth, &nHeight, &nChannels, 0);

		if (!data)
		{
			std::cerr << stbi_failure_reason() << "\n";
			assert(false && "stbi_failure_reason");
		}

		GLuint nTextureId = 0;
		GLenum format = 0;

		switch (nChannels)
		{
		case 1: format = GL_RED; break;
		case 3: format = GL_RGB; break;
		case 4: format = GL_RGBA; break;
		default: assert(false && "Unsupported color format!");
		}

		glGenTextures(1, &nTextureId);
		glBindTexture(GL_TEXTURE_2D, nTextureId);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			format,
			nWidth,
			nHeight,
			0,
			format,
			GL_UNSIGNED_BYTE,
			data
		);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		if (bClamp)
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		
		stbi_image_free(data);

		return nTextureId;
	}

	void DrawTile(float x, float y, float z, GLuint texture, float angle, def::vf3d top_left, def::vf3d top_right, def::vf3d bottom_right, def::vf3d bottom_left)
	{
		glTranslatef(x, y, z);

		glRotatef(angle, 0.0f, 0.0f, 1.0f);

		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);

		glColor3f(1.0f, 1.0f, 1.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBindTexture(GL_TEXTURE_2D, texture);
			
		glBegin(GL_QUADS);
			glTexCoord3f(-1.0f, 1.0f, 0.0f);  glVertex3f(top_left.x, top_left.y, top_left.z);				// default: -1,  1, 0
			glTexCoord3f(1.0f, 1.0f, 0.0f);   glVertex3f(top_right.x, top_right.y, top_right.z);			// default:  1,  1, 0
			glTexCoord3f(1.0f, -1.0f, 0.0f);  glVertex3f(bottom_right.x, bottom_right.y, bottom_right.z);	// default:  1, -1, 0
			glTexCoord3f(-1.0f, -1.0f, 0.0f); glVertex3f(bottom_left.x, bottom_left.y, bottom_left.z);		// default: -1, -1, 0
		glEnd();

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void DrawBuildingFloor(float x, float y, float z, GLuint texture)
	{
		glTranslatef(x, y, z);

		DrawTile(0.0f, 0.0f, 0.0f, texture, 0.0f,
			{ 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }
		);

		DrawTile(0.0f, 0.0f, 0.0f, texture, 0.0f,
			{ 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }
		);

		DrawTile(0.0f, 0.0f, 0.0f, texture, 0.0f,
			{ 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }
		);

		DrawTile(0.0f, 0.0f, 0.0f, texture, 0.0f,
			{ 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }
		);

		glTranslatef(-x, -y, -z);
	}

	void DrawSelectedArea()
	{
		glTranslatef(vSelectedArea.x, vSelectedArea.y, 0.0f);

		glBegin(GL_LINES);
		
		glColor3f(1.0f, 0.0f, 0.0f);

		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(0.0f, 1.0f, 0.0f);

		glVertex3f(0.0f, 1.0f, 0.0f);
		glVertex3f(1.0f, 1.0f, 0.0f);

		glVertex3f(1.0f, 1.0f, 0.0f);
		glVertex3f(1.0f, 0.0f, 0.0f);

		glVertex3f(1.0f, 0.0f, 0.0f);
		glVertex3f(0.0f, 0.0f, 0.0f);

		glEnd();

		glTranslatef(-vSelectedArea.x, -vSelectedArea.y, 0.0f);
	}
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	lua_State* L = luaL_newstate();

	int rcode = luaL_dofile(L, "config.lua");

	if (rcode == LUA_OK)
	{
		lua_getglobal(L, "WorldWidth");
		int nWorldWidth = lua_tonumber(L, -1);

		lua_getglobal(L, "WorldHeight");
		int nWorldHeight = lua_tonumber(L, -1);

		nWorldWidth = (nWorldWidth < 30) ? 30 : nWorldWidth;
		nWorldHeight = (nWorldHeight < 15) ? 15 : nWorldHeight;

		Example demo(L, nWorldWidth, nWorldHeight);

		lua_getglobal(L, "ScreenWidth");
		int nScreenWidth = lua_tonumber(L, -1);

		lua_getglobal(L, "ScreenHeight");
		int nScreenHeight = lua_tonumber(L, -1);

		if (nScreenWidth < 800)
			nScreenWidth = 800;

		if (nScreenHeight < 600)
			nScreenHeight = 600;

		if (!demo.Run(nScreenWidth, nScreenHeight, L"Car Crime City"))
			return 1;
	}
	else
	{
		const char* msg = lua_tostring(L, -1);
		std::cerr << msg << "\n";
		return 1;
	}

	return 0;
}