#include <vector>
#include <string>
#include <map>
#include <thread>
#include <atomic>
#include <math.h>
#include <random>
#include <algorithm>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <stdio.h>
#include <SDL3/SDL.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

class sdl_c
{
public:
	SDL_Window *window;
	SDL_GLContext gl_context;
	const char *glsl_version;

	sdl_c() : window(nullptr)
	{
		printf("sdl_c constructor\n");

		// Setup SDL
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMEPAD) != 0)
		{
			printf("Error: SDL_Init(): %s\n", SDL_GetError());
			return;
		}

		// Decide GL+GLSL versions
	#if defined(IMGUI_IMPL_OPENGL_ES2)
		// GL ES 2.0 + GLSL 100
		glsl_version = "#version 100";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	#elif defined(__APPLE__)
		// GL 3.2 Core + GLSL 150
		glsl_version = "#version 150";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	#else
		// GL 3.0 + GLSL 130
		glsl_version = "#version 130";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	#endif

		// Enable native IME.
		SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

		// Create window with graphics context
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
		window = SDL_CreateWindow("SRB2Randomizer Logic Generator", 1280, 720, window_flags);

		if (window == nullptr)
		{
			printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
			return;
		}

		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		gl_context = SDL_GL_CreateContext(window);
		SDL_GL_MakeCurrent(window, gl_context);
		SDL_GL_SetSwapInterval(1); // Enable vsync

		SDL_ShowWindow(window);
	}

	~sdl_c()
	{
		printf("sdl_c destructor\n");

		if (window == nullptr)
		{
			return;
		}

		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}
};

class imgui_c
{
public:
	bool context_valid;
	ImGuiIO &io;

	ImGuiIO &InitContext()
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();

		ImGui::CreateContext();

		ImGuiIO &ioRef = ImGui::GetIO();
		ioRef.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		return ioRef;
	}

	imgui_c() : io(InitContext())
	{
		printf("imgui_c constructor\n");

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
	}

	~imgui_c()
	{
		printf("imgui_c destructor\n");

		if (context_valid == false)
		{
			return;
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
};

class wad_header_c
{
public:
	char type[4];
	int32_t lump_count;
	int32_t directory_offset;
};

class wad_lump_c
{
public:
	int32_t offset;
	int32_t size;
	char name[8];

	template<typename T>
		bool load_as_vector(FILE *file_ptr, const char *safety_name, std::vector<T> &elements)
	{
		assert(file_ptr != nullptr);

		if (strlen(safety_name) == 0 || strncmp(safety_name, name, 8) == 0)
		{
			auto count = size / sizeof(T);
			elements.resize(count);

			fseek(file_ptr, offset, SEEK_SET);
			fread(elements.data(), sizeof(T), count, file_ptr);

			printf("Loaded lump %s\n", safety_name);
			return true;
		}

		return false;
	}
};

class wad_c
{
public:
	bool valid;
	FILE *file_ptr;
	wad_header_c header;
	std::vector<wad_lump_c> directory;

	wad_c(const char *wad_path)
	{
		printf("Attempting to open file: %s\n", wad_path);
		file_ptr = fopen(wad_path, "rb");
		if (file_ptr == nullptr)
		{
			printf("Cannot open file: %s\n", wad_path);
			return;
		}

		fread(&header, sizeof(header), 1, file_ptr);
		if (strncmp(header.type, "PWAD", 4) != 0
			&& strncmp(header.type, "IWAD", 4) != 0)
		{
			printf("File is not a WAD\n");
			return;
		}

		if (header.lump_count <= 0)
		{
			printf("File has no lumps\n");
			return;
		}

		directory.resize(header.lump_count);
		fseek(file_ptr, header.directory_offset, SEEK_SET);
		fread(directory.data(), sizeof(wad_lump_c), header.lump_count, file_ptr);

		printf("Finished opening WAD\n");
		valid = true;
	}

	~wad_c()
	{
		if (file_ptr != nullptr)
		{
			fclose(file_ptr);
			file_ptr = nullptr;
		}
	}
};

struct map_thing_s
{
	int16_t x;
	int16_t y;
	int16_t angle;
	int16_t doomednum;
	int16_t flags;
};

struct map_linedef_s
{
	int16_t vertex_id_a;
	int16_t vertex_id_b;
	int16_t flags;
	int16_t action;
	int16_t tag;
	int16_t side_id_front;
	int16_t side_id_back;
};

struct map_sidedef_s
{
	int16_t x_offset;
	int16_t y_offset;
	char texture_upper[8];
	char texture_lower[8];
	char texture_middle[8];
	int16_t sector_id;
};

struct map_vertex_s
{
	int16_t x;
	int16_t y;
};

/*
struct map_seg_s
{
	uint16_t vertex_id_a;
	uint16_t vertex_id_b;
	int16_t angle;
	uint16_t linedef_id;
	int16_t side;
	int16_t offset;
};

struct map_subsector_s
{
	int16_t seg_count;
	int16_t seg_id;
};

struct map_node_s
{
	int16_t partition_x;
	int16_t partition_y;
	int16_t partition_x_delta;
	int16_t partition_y_delta;
	int16_t bbox_right[4];
	int16_t bbox_left[4];
	int16_t child_right;
	int16_t child_left;
};
*/

struct map_sector_s
{
	int16_t floor_height;
	int16_t ceiling_height;
	char floor_texture[8];
	char ceiling_texture[8];
	int16_t light;
	int16_t special;
	int16_t tag;
};

class map_c
{
public:
	std::atomic_bool loaded;
	const char *name;

	std::vector<map_thing_s> things;
	std::vector<map_linedef_s> linedefs;
	std::vector<map_sidedef_s> sidedefs;
	std::vector<map_vertex_s> vertices;
	std::vector<map_sector_s> sectors;

	map_c(wad_c &wad, const char *map_name) : loaded(false), name(map_name)
	{
		if (wad.valid == false)
		{
			printf("Tried to load map from invalid WAD\n");
			return;
		}

		for (int i = 0, len = (int)wad.directory.size(); i < len; ++i)
		{
			auto &map_lump = wad.directory[i];
			printf("Lump: %s\n", map_lump.name);

			if (strncmp(name, map_lump.name, 8) == 0)
			{
				for (int j = 1; j < 8; ++i)
				{
					if (i + j >= len)
					{
						break;
					}

					auto &resource_lump = wad.directory[i + j];

					resource_lump.load_as_vector(wad.file_ptr, "THINGS", things);
					resource_lump.load_as_vector(wad.file_ptr, "LINEDEFS", linedefs);
					resource_lump.load_as_vector(wad.file_ptr, "SIDEDEFS", sidedefs);
					resource_lump.load_as_vector(wad.file_ptr, "VERTEXES", vertices);
					resource_lump.load_as_vector(wad.file_ptr, "SECTORS", sectors);
				}

				printf("Map successfully loaded\n");
				loaded = true;
				return;
			}
		}

		printf("No map lump labelled %s\n", map_name);
	}
};

class region_c
{
public:
	std::string title;
	std::map<std::string, bool> rules;
	ImVec2 rect_vertex_a, rect_vertex_b;
	double rect_color[3];

	region_c()
	{
		title = "Untitled region";

		rect_vertex_a = ImVec2(-64.0f, -64.0f);
		rect_vertex_b = ImVec2(64.0f, 64.0f);

		double hue = (float)(rand()) / (float)(RAND_MAX);
		rect_color[0] = hue;
		rect_color[1] = 1.0f;
		rect_color[2] = 0.0f;

		for (int i = 2; i > 0; i--)
		{
			int j = rand() % (i + 1);

			double tmp = rect_color[i];
			rect_color[i] = rect_color[j];
			rect_color[j] = tmp;
		}
	}
};

class main_c
{
public:
	class sdl_c sdl;
	class imgui_c imgui;

	class map_c *cur_map;
	std::vector<region_c> regions;
	int selected_region_id;

	const float GRID_STEP = 64.0f;
	const float GRID_MIN = -32768.0f;
	const float GRID_MAX = 32767.0f;

	const float ZOOM_MIN = 0.01f;
	const float ZOOM_MAX = 1.0f;
	const float ZOOM_BASE = 2.0f;

	enum grab_handle_e
	{
		GRAB_NULL = 0,
		GRAB_TOP = 1 << 0,
		GRAB_LEFT = 1 << 1,
		GRAB_BOTTOM = 1 << 2,
		GRAB_RIGHT = 1 << 3,
		GRAB_ALL = GRAB_TOP|GRAB_LEFT|GRAB_BOTTOM|GRAB_RIGHT,
	};

	ImVec2 scroll;
	float zoom;
	ImVec2 work_pos, work_size;

	main_c() : scroll(ImVec2(0.0f, 0.0f)), zoom(0.5f)
	{
		printf("main_c constructor\n");

		if (sdl.window == nullptr)
		{
			return;
		}

		// Setup Platform/Renderer backends
		ImGui_ImplSDL3_InitForOpenGL(sdl.window, sdl.gl_context);
		ImGui_ImplOpenGL3_Init(sdl.glsl_version);

		imgui.context_valid = true;
	}

	~main_c()
	{
		printf("main_c destructor\n");
		delete cur_map;
		imgui.~imgui_c();
		sdl.~sdl_c();
	}

	bool Frame_Poll(void)
	{
		SDL_Event event;
		bool done = false;

		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);

			if (event.type == SDL_EVENT_QUIT)
			{
				done = true;
			}

			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(sdl.window))
			{
				done = true;
			}
		}

		return done;
	}

	void Frame_Init(void)
	{
		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		work_pos = viewport->WorkPos;
		work_size = viewport->WorkSize;
	}

	void Frame_Process(void)
	{
		// Rendering
		ImGui::Render();

		glViewport(0, 0, (int)imgui.io.DisplaySize.x, (int)imgui.io.DisplaySize.y);
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(sdl.window);
	}

	ImVec2 MapSpaceToScreenSpace(const ImVec2 &input)
	{
		return ImVec2(
			work_pos.x + (work_size.x * 0.5) + (scroll.x * zoom) + (input.x * zoom * ZOOM_BASE),
			work_pos.y + (work_size.y * 0.5) + (scroll.y * zoom) + (-input.y * zoom * ZOOM_BASE)
		);
	}

	void DrawMap(void)
	{
		if (imgui.io.WantCaptureMouse == false)
		{
			if (imgui.io.MouseWheel != 0.0f)
			{
				static const float zoomScrollFactor = (1.0f / 8.0f);
				zoom = std::clamp(
					zoom + (imgui.io.MouseWheel * zoomScrollFactor * zoom),
					ZOOM_MIN,
					ZOOM_MAX
				);
				imgui.io.WantCaptureMouse = true;
			}

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
			{
				scroll.x += imgui.io.MouseDelta.x / zoom;
				scroll.y += imgui.io.MouseDelta.y / zoom;
				imgui.io.WantCaptureMouse = true;
			}
		}

		ImDrawList *draw_list = ImGui::GetBackgroundDrawList();
		static const float thickness = 1.0f;

		for (float x = -32768.0f; x < 32767.0f; x += GRID_STEP)
		{
			draw_list->AddLine(
				MapSpaceToScreenSpace(ImVec2(x, GRID_MIN)),
				MapSpaceToScreenSpace(ImVec2(x, GRID_MAX)),
				IM_COL32(200, 200, 200, 40),
				thickness * 0.5f
			);
		}

		for (float y = -32768.0f; y < 32767.0f; y += GRID_STEP)
		{
			draw_list->AddLine(
				MapSpaceToScreenSpace(ImVec2(GRID_MIN, y)),
				MapSpaceToScreenSpace(ImVec2(GRID_MAX, y)),
				IM_COL32(200, 200, 200, 40),
				thickness * 0.5f
			);
		}

		if (cur_map != nullptr && cur_map->loaded == true)
		{
			for (int i = 0, len = (int)cur_map->linedefs.size(); i < len; ++i)
			{
				const auto &line = cur_map->linedefs[i];

				const auto &vertex_a = cur_map->vertices[line.vertex_id_a];
				const auto &vertex_b = cur_map->vertices[line.vertex_id_b];

				float trans = 1.0f;

				if ((line.flags & 1) == 0)
				{
					trans *= 0.5f;
				}

				draw_list->AddLine(
					MapSpaceToScreenSpace(ImVec2(vertex_a.x, vertex_a.y)),
					MapSpaceToScreenSpace(ImVec2(vertex_b.x, vertex_b.y)),
					IM_COL32(200, 200, 200, 200 * trans),
					thickness
				);
			}

			for (int i = 0, len = (int)cur_map->things.size(); i < len; ++i)
			{
				const auto &thing = cur_map->things[i];

				draw_list->AddCircleFilled(
					MapSpaceToScreenSpace(ImVec2(thing.x, thing.y)),
					8.0f * zoom * ZOOM_BASE,
					IM_COL32(200, 200, 200, 200)
				);
			}
		}
	}

	void RectMoveByHandles(ImVec2 *top_left, ImVec2 *bottom_right, const int handle, const ImVec2 &delta)
	{
		if (handle & GRAB_LEFT)
		{
			top_left->x += delta.x / zoom / ZOOM_BASE;
		}

		if (handle & GRAB_TOP)
		{
			top_left->y += delta.y / zoom / ZOOM_BASE;
		}

		if (handle & GRAB_RIGHT)
		{
			bottom_right->x += delta.x / zoom / ZOOM_BASE;
		}

		if (handle & GRAB_BOTTOM)
		{
			bottom_right->y += delta.y / zoom / ZOOM_BASE;
		}
	}

	void DrawRegions(void)
	{
		static ImGuiWindowFlags region_flags = ( ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoNav
		);
		static const float BORDER_SIZE = GRID_STEP * 0.25;
		static int handle = GRAB_NULL;

		ImDrawList *draw_list = ImGui::GetBackgroundDrawList();

		if (imgui.io.WantCaptureMouse == false
			&& (selected_region_id >= 0 && selected_region_id < (int)regions.size()))
		{
			auto &region = regions[selected_region_id];

			ImVec2 top_left = MapSpaceToScreenSpace(ImVec2(region.rect_vertex_a.x, -region.rect_vertex_a.y));
			ImVec2 bottom_right = MapSpaceToScreenSpace(ImVec2(region.rect_vertex_b.x, -region.rect_vertex_b.y));

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)
				&& handle != GRAB_NULL)
			{
				// Update
				RectMoveByHandles(
					&region.rect_vertex_a,
					&region.rect_vertex_b,
					handle,
					ImGui::GetMouseDragDelta(ImGuiMouseButton_Left)
				);
				ImGui::ResetMouseDragDelta();
				imgui.io.WantCaptureMouse = true;
			}
			else if (ImGui::IsMouseHoveringRect(
				ImVec2(top_left.x - BORDER_SIZE, top_left.y - BORDER_SIZE),
				ImVec2(bottom_right.x + BORDER_SIZE, bottom_right.y + BORDER_SIZE),
				false))
			{
				if (ImGui::IsMouseHoveringRect(
					ImVec2(top_left.x + BORDER_SIZE, top_left.y + BORDER_SIZE),
					ImVec2(bottom_right.x - BORDER_SIZE, bottom_right.y - BORDER_SIZE),
					false))
				{
					// Drag the entire thing
					handle = GRAB_ALL;
				}
				else
				{
					// Stretch specific handle
					ImVec2 cursor = ImGui::GetMousePos();

					ImVec2 center = ImVec2(
						(top_left.x + bottom_right.x) * 0.5f,
						(top_left.y + bottom_right.y) * 0.5f
					);

					ImVec2 dist = ImVec2(
						abs(top_left.x - center.x),
						abs(top_left.y - center.y)
					);
					ImVec2 detect_handle = ImVec2(
						(cursor.x - center.x) / dist.x,
						(cursor.y - center.y) / dist.y
					);

					static const float HANDLE_DIST = 0.85f;
					handle = GRAB_NULL;

					if (detect_handle.x < -HANDLE_DIST)
					{
						handle |= GRAB_LEFT;
					}
					else if (detect_handle.x > HANDLE_DIST)
					{
						handle |= GRAB_RIGHT;
					}

					if (detect_handle.y < -HANDLE_DIST)
					{
						handle |= GRAB_TOP;
					}
					else if (detect_handle.y > HANDLE_DIST)
					{
						handle |= GRAB_BOTTOM;
					}
				}

				imgui.io.WantCaptureMouse = true;
			}
			else
			{
				handle = GRAB_NULL;
			}
		}
		else
		{
			handle = GRAB_NULL;
		}

		switch (handle)
		{
			case GRAB_TOP:
			case GRAB_BOTTOM:
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				break;
			}
			case GRAB_LEFT:
			case GRAB_RIGHT:
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
				break;
			}
			case (GRAB_TOP|GRAB_LEFT):
			case (GRAB_BOTTOM|GRAB_RIGHT):
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
				break;
			}
			case (GRAB_TOP|GRAB_RIGHT):
			case (GRAB_BOTTOM|GRAB_LEFT):
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
				break;
			}
			case GRAB_ALL:
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
				break;
			}
		}

		for (int i = (int)regions.size()-1; i >= 0; --i)
		{
			auto &region = regions[i];

			std::string window_id("Region Window##");
			window_id += std::to_string(i + 1);

			float trans = 1.0f;
			bool highlight = false;

			ImVec2 top_left = MapSpaceToScreenSpace(ImVec2(region.rect_vertex_a.x, -region.rect_vertex_a.y));
			ImVec2 bottom_right = MapSpaceToScreenSpace(ImVec2(region.rect_vertex_b.x, -region.rect_vertex_b.y));

			if ((i == selected_region_id || imgui.io.WantCaptureMouse == false)
				&& ImGui::IsMouseHoveringRect(
					ImVec2(top_left.x - BORDER_SIZE, top_left.y - BORDER_SIZE),
					ImVec2(bottom_right.x + BORDER_SIZE, bottom_right.y + BORDER_SIZE),
					false
				))
			{
				highlight = true;

				if (imgui.io.WantCaptureMouse == false)
				{
					if (handle == GRAB_NULL)
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					}

					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						selected_region_id = i;
						imgui.io.WantCaptureMouse = true;
					}
				}
			}

			float rect_color[3];
			rect_color[0] = region.rect_color[0] * 0.8f;
			rect_color[1] = region.rect_color[1] * 0.8f;
			rect_color[2] = region.rect_color[2] * 0.8f;

			if (highlight == true)
			{
				rect_color[0] = std::min(rect_color[0] + 0.5f, 1.0f);
				rect_color[1] = std::min(rect_color[1] + 0.5f, 1.0f);
				rect_color[2] = std::min(rect_color[2] + 0.5f, 1.0f);
			}

			draw_list->AddRectFilled(
				top_left,
				bottom_right,
				IM_COL32(255 * rect_color[0], 255 * rect_color[1], 255 * rect_color[2], 100 * trans)
			);
			draw_list->AddRect(
				top_left,
				bottom_right,
				IM_COL32(255 * rect_color[0], 255 * rect_color[1], 255 * rect_color[2], 200 * trans)
			);
			draw_list->AddText(
				ImVec2(top_left.x + (BORDER_SIZE * 0.5), top_left.y + (BORDER_SIZE * 0.5)),
				IM_COL32_WHITE,
				region.title.c_str()
			);
		}
	}

	void DrawMenuBar(void)
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::MenuItem("(demo menu)", NULL, false, false);
				if (ImGui::MenuItem("New")) {}
				if (ImGui::MenuItem("Open", "Ctrl+O")) {}
				if (ImGui::BeginMenu("Open Recent"))
				{
					ImGui::MenuItem("fish_hat.c");
					ImGui::MenuItem("fish_hat.inl");
					ImGui::MenuItem("fish_hat.h");
					if (ImGui::BeginMenu("More.."))
					{
						ImGui::MenuItem("Hello");
						ImGui::MenuItem("Sailor");
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Save", "Ctrl+S")) {}
				if (ImGui::MenuItem("Save As..")) {}

				ImGui::Separator();
				if (ImGui::BeginMenu("Options"))
				{
					static bool enabled = true;
					ImGui::MenuItem("Enabled", "", &enabled);
					ImGui::BeginChild("child", ImVec2(0, 60), true);
					for (int i = 0; i < 10; i++)
						ImGui::Text("Scrolling Text %d", i);
					ImGui::EndChild();
					static float f = 0.5f;
					static int n = 0;
					ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
					ImGui::InputFloat("Input", &f, 0.1f);
					ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Colors"))
				{
					float sz = ImGui::GetTextLineHeight();
					for (int i = 0; i < ImGuiCol_COUNT; i++)
					{
						const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
						ImVec2 p = ImGui::GetCursorScreenPos();
						ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
						ImGui::Dummy(ImVec2(sz, sz));
						ImGui::SameLine();
						ImGui::MenuItem(name);
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Checked", NULL, true)) {}
				ImGui::Separator();
				if (ImGui::MenuItem("Quit", "Alt+F4")) {}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
				if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "CTRL+X")) {}
				if (ImGui::MenuItem("Copy", "CTRL+C")) {}
				if (ImGui::MenuItem("Paste", "CTRL+V")) {}
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}

	void ValidateRegionTitle(region_c &region, int region_id)
	{
		std::string new_title = region.title;
		int append_num = 1;

		int i = 0;
		int len = (int)regions.size();

		bool valid = false;
		while (valid == false)
		{
			valid = true;

			for (int i = 0; i < len; ++i)
			{
				if (i == region_id)
				{
					continue;
				}

				const auto &other = regions[i];
				if (new_title.compare(other.title) == 0)
				{
					append_num++;
					new_title = region.title + " (" + std::to_string(append_num) + ")";
					valid = false;
					break;
				}
			}
		}

		region.title = new_title;
	}

	void NewRegion(void)
	{
		int region_id = regions.size();
		auto &new_region = regions.emplace_back();
		ValidateRegionTitle(new_region, region_id);
	}

	bool DoFrame(void)
	{
		bool done = Frame_Poll();
		Frame_Init();

		static bool show_demo_window = true;
		if (show_demo_window)
		{
			ImGui::ShowDemoWindow(&show_demo_window);
		}

		DrawRegions();
		DrawMap();

		DrawMenuBar();

		if (ImGui::Begin("Regions"))
		{
			if (ImGui::Button("New Region"))
			{
				NewRegion();
			}

			if (ImGui::BeginChild("region_list", ImVec2(0, 160), true))
			{
				for (int i = 0, len = (int)regions.size(); i < len; ++i)
				{
					auto &region = regions[i];
					if (ImGui::Selectable(region.title.c_str(), selected_region_id == i))
					{
						selected_region_id = i;
					}

					if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
					{
						int i_next = i + (ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y < 0.f ? -1 : 1);
						if (i_next >= 0 && i_next < len)
						{
							std::iter_swap(regions.begin() + i, regions.begin() + i_next);
							selected_region_id = i_next;
							ImGui::ResetMouseDragDelta();
						}
					}
				}
			}
			ImGui::EndChild();

			if (selected_region_id >= 0 && selected_region_id < (int)regions.size())
			{
				auto &region = regions[selected_region_id];

				static std::string title_input = "";
				static int old_selection = -1;

				if (old_selection != selected_region_id)
				{
					title_input = region.title;
					old_selection = selected_region_id;
				}

				ImGui::InputText("Title", &title_input, ImGuiInputTextFlags_EnterReturnsTrue);
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					if (title_input.size() > 0)
					{
						region.title = title_input;
						ValidateRegionTitle(region, selected_region_id);
					}
					title_input = region.title;
				}

				if (ImGui::InputFloat4("Bounding Box", &region.rect_vertex_a.x, "%.0f"))
				{
					region.rect_vertex_a.x = std::clamp(region.rect_vertex_a.x, GRID_MIN, GRID_MAX);
					region.rect_vertex_a.y = std::clamp(region.rect_vertex_a.y, GRID_MIN, GRID_MAX);
					region.rect_vertex_b.x = std::clamp(region.rect_vertex_b.x, GRID_MIN, GRID_MAX);
					region.rect_vertex_b.y = std::clamp(region.rect_vertex_b.y, GRID_MIN, GRID_MAX);
				}
			}
		}
		ImGui::End();

		Frame_Process();
		return done;
	}

	void LoadGenericMap(void)
	{
		wad_c wad = wad_c("MAP01.wad");
		cur_map = new map_c(wad, "MAP01");
		printf("Created map\n");
	}

	void Loop(void)
	{
		LoadGenericMap();

		bool done = false;
		while (done == false)
		{
			done = DoFrame();
		}
	}
};

// Main code
int main(int argc, char **argv)
{
	class main_c main_state;

	if (main_state.sdl.window == nullptr)
	{
		// Could not initialize SDL window
		return EXIT_FAILURE;
	}

	main_state.Loop();
	return EXIT_SUCCESS;
}
