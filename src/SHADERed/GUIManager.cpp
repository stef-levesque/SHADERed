#include <SDL2/SDL_messagebox.h>
#include <SHADERed/GUIManager.h>
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/FS.h>
#include <SHADERed/Objects/CameraSnapshots.h>
#include <SHADERed/Objects/Export/ExportCPP.h>
#include <SHADERed/Objects/FunctionVariableManager.h>
#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/SPIRVParser.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/CreateItemUI.h>
#include <SHADERed/UI/Debug/BreakpointListUI.h>
#include <SHADERed/UI/Debug/FunctionStackUI.h>
#include <SHADERed/UI/Debug/ImmediateUI.h>
#include <SHADERed/UI/Debug/ValuesUI.h>
#include <SHADERed/UI/Debug/WatchUI.h>
#include <SHADERed/UI/Icons.h>
#include <SHADERed/UI/MessageOutputUI.h>
#include <SHADERed/UI/ObjectListUI.h>
#include <SHADERed/UI/ObjectPreviewUI.h>
#include <SHADERed/UI/OptionsUI.h>
#include <SHADERed/UI/PinnedUI.h>
#include <SHADERed/UI/PipelineUI.h>
#include <SHADERed/UI/PixelInspectUI.h>
#include <SHADERed/UI/PreviewUI.h>
#include <SHADERed/UI/PropertyUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <imgui/imgui.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

#include <fstream>

#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_CATMULLROM
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize.h>

#if defined(__APPLE__)
// no includes on mac os
#elif defined(__linux__) || defined(__unix__)
// no includes on linux
#elif defined(_WIN32)
#include <windows.h>
#endif
#include <queue>

#define HARRAYSIZE(a) (sizeof(a) / sizeof(*a))
#define TOOLBAR_HEIGHT 48

#define getByte(value, n) (value >> (n * 8) & 0xFF)

namespace ed {
	GUIManager::GUIManager(ed::InterfaceManager* objects, SDL_Window* wnd, SDL_GLContext* gl)
	{
		m_data = objects;
		m_wnd = wnd;
		m_gl = gl;
		m_settingsBkp = new Settings();
		m_previewSaveSize = glm::ivec2(1920, 1080);
		m_savePreviewPopupOpened = false;
		m_optGroup = 0;
		m_optionsOpened = false;
		m_cachedFont = "null";
		m_cachedFontSize = 15;
		m_performanceMode = false;
		m_perfModeFake = false;
		m_fontNeedsUpdate = false;
		m_isCreateItemPopupOpened = false;
		m_isCreateCubemapOpened = false;
		m_isCreateRTOpened = false;
		m_isCreateKBTxtOpened = false;
		m_isCreateBufferOpened = false;
		m_isNewProjectPopupOpened = false;
		m_isRecordCameraSnapshotOpened = false;
		m_exportAsCPPOpened = false;
		m_isCreateImgOpened = false;
		m_isAboutOpen = false;
		m_wasPausedPrior = true;
		m_savePreviewSeq = false;
		m_cacheProjectModified = false;
		m_isCreateImg3DOpened = false;
		m_isInfoOpened = false;
		m_isChangelogOpened = false;
		m_savePreviewSeqDuration = 5.5f;
		m_savePreviewSeqFPS = 30;
		m_savePreviewSupersample = 0;
		m_iconFontLarge = nullptr;
		m_expcppBackend = 0;
		m_expcppCmakeFiles = true;
		m_expcppCmakeModules = true;
		m_expcppImage = true;
		m_expcppMemoryShaders = true;
		m_expcppCopyImages = true;
		memset(&m_expcppProjectName[0], 0, 64 * sizeof(char));
		strcpy(m_expcppProjectName, "ShaderProject");
		m_expcppSavePath = "./export.cpp";
		m_expcppError = false;
		m_tipOpened = false;
		m_splashScreen = true;
		m_splashScreenLoaded = false;
		m_recompiledAll = false;
		m_isIncompatPluginsOpened = false;
		m_minimalMode = false;
		m_cubemapPathPtr = nullptr;

		m_isBrowseOnlineOpened = false;
		memset(&m_onlineQuery, 0, sizeof(m_onlineQuery));
		memset(&m_onlineUsername, 0, sizeof(m_onlineUsername));
		m_onlinePage = 0;
		m_onlineShaderPage = 0;
		m_onlinePluginPage = 0;
		m_onlineThemePage = 0;
		m_onlineIsShader = true;
		m_onlineIsPlugin = false;
		m_onlineExcludeGodot = false;

		m_uiIniFile = "data/workspace.dat";
		if (!ed::Settings::Instance().LinuxHomeDirectory.empty())
			m_uiIniFile = ed::Settings::Instance().LinuxHomeDirectory + m_uiIniFile;

		Settings::Instance().Load();
		m_loadTemplateList();

		SDL_GetWindowSize(m_wnd, &m_width, &m_height);

		Logger::Get().Log("Initializing Dear ImGUI");

		// set vsync on startup
		SDL_GL_SetSwapInterval(Settings::Instance().General.VSync);

		// Initialize imgui
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		io.IniFilename = m_uiIniFile.c_str();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_DockingEnable /*| ImGuiConfigFlags_ViewportsEnable TODO: allow this on windows? test on linux?*/;
		io.ConfigDockingWithShift = false;

		if (!ed::Settings::Instance().LinuxHomeDirectory.empty()) {
			if (!fs::exists(m_uiIniFile) && fs::exists("data/workspace.dat"))
				ImGui::LoadIniSettingsFromDisk("data/workspace.dat");
		}

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowMenuButtonPosition = ImGuiDir_Right;

		ImGui_ImplOpenGL3_Init(SDL_GLSL_VERSION);
		ImGui_ImplSDL2_InitForOpenGL(m_wnd, *m_gl);

		ImGui::StyleColorsDark();

		Logger::Get().Log("Creating various UI view objects");

		m_views.push_back(new PreviewUI(this, objects, "Preview"));
		m_views.push_back(new PinnedUI(this, objects, "Pinned"));
		m_views.push_back(new CodeEditorUI(this, objects, "Code"));
		m_views.push_back(new MessageOutputUI(this, objects, "Output"));
		m_views.push_back(new ObjectListUI(this, objects, "Objects"));
		m_views.push_back(new PipelineUI(this, objects, "Pipeline"));
		m_views.push_back(new PropertyUI(this, objects, "Properties"));
		m_views.push_back(new PixelInspectUI(this, objects, "Pixel Inspect"));

		m_debugViews.push_back(new DebugWatchUI(this, objects, "Watches"));
		m_debugViews.push_back(new DebugValuesUI(this, objects, "Variables"));
		m_debugViews.push_back(new DebugFunctionStackUI(this, objects, "Function stack"));
		m_debugViews.push_back(new DebugBreakpointListUI(this, objects, "Breakpoints"));
		m_debugViews.push_back(new DebugImmediateUI(this, objects, "Immediate"));

		KeyboardShortcuts::Instance().Load();
		m_setupShortcuts();

		m_options = new OptionsUI(this, objects, "Options");
		m_createUI = new CreateItemUI(this, objects);
		m_objectPrev = new ObjectPreviewUI(this, objects, "Object Preview");

		// turn on the tracker on startup
		((CodeEditorUI*)Get(ViewID::Code))->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
		
		((OptionsUI*)m_options)->SetGroup(OptionsUI::Page::General);

		// enable dpi awareness
		if (Settings::Instance().General.AutoScale) {
			float dpi = 0.0f;
			int wndDisplayIndex = SDL_GetWindowDisplayIndex(wnd);
			SDL_GetDisplayDPI(wndDisplayIndex, &dpi, NULL, NULL);
			dpi /= 96.0f;
			Settings::Instance().DPIScale = dpi;
			Logger::Get().Log("Setting DPI to " + std::to_string(dpi));
		}
		Settings::Instance().TempScale = Settings::Instance().DPIScale;

		ImGui::GetStyle().ScaleAllSizes(Settings::Instance().DPIScale);

		((OptionsUI*)m_options)->ApplyTheme();

		FunctionVariableManager::Instance().Initialize(&objects->Pipeline);
		m_data->Renderer.Pause(Settings::Instance().Preview.PausedOnStartup);

		m_kbInfo.SetText(std::string(KEYBOARD_KEYCODES_TEXT));
		m_kbInfo.SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
		m_kbInfo.SetHighlightLine(true);
		m_kbInfo.SetShowLineNumbers(true);
		m_kbInfo.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
		m_kbInfo.SetReadOnly(true);

		// load snippets
		((CodeEditorUI*)Get(ViewID::Code))->LoadSnippets();

		// load file dialog bookmarks
		std::string bookmarksFileLoc = "data/bookmarks.dat";
		if (!ed::Settings::Instance().LinuxHomeDirectory.empty())
			bookmarksFileLoc = ed::Settings::Instance().LinuxHomeDirectory + "data/bookmarks.dat";
		std::ifstream bookmarksFile(bookmarksFileLoc);
		std::string bookmarksString((std::istreambuf_iterator<char>(bookmarksFile)), std::istreambuf_iterator<char>());
		igfd::ImGuiFileDialog::Instance()->DeserializeBookmarks(bookmarksString);

		// setup splash screen
		m_splashScreenLoad();

		// load recents
		std::string currentInfoPath = "info.dat";
		if (!ed::Settings().Instance().LinuxHomeDirectory.empty())
			currentInfoPath = ed::Settings().Instance().LinuxHomeDirectory + currentInfoPath;

		int recentsSize = 0;
		std::ifstream infoReader(currentInfoPath);
		infoReader.ignore(128, '\n');
		infoReader >> recentsSize;
		infoReader.ignore(128, '\n');
		m_recentProjects = std::vector<std::string>(recentsSize);
		for (int i = 0; i < recentsSize; i++) {
			std::getline(infoReader, m_recentProjects[i]);
			if (infoReader.eof())
				break;
		}
		infoReader.close();
	}
	GUIManager::~GUIManager()
	{
		glDeleteShader(Magnifier::Shader);

		for (int i = 0; i < m_onlineShaderThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlineShaderThumbnail[i]);
		m_onlineShaderThumbnail.clear();

		for (int i = 0; i < m_onlinePluginThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlinePluginThumbnail[i]);
		m_onlinePluginThumbnail.clear();

		std::string currentInfoPath = "info.dat";
		if (!ed::Settings().Instance().LinuxHomeDirectory.empty())
			currentInfoPath = ed::Settings().Instance().LinuxHomeDirectory + currentInfoPath;
		
		std::ofstream verWriter(currentInfoPath);
		verWriter << WebAPI::InternalVersion << std::endl;
		verWriter << m_recentProjects.size() << std::endl;
		for (int i = 0; i < m_recentProjects.size(); i++)
			verWriter << m_recentProjects[i] << std::endl;
		verWriter.close();

		Logger::Get().Log("Shutting down UI");

		for (auto& view : m_views)
			delete view;
		for (auto& dview : m_debugViews)
			delete dview;

		delete m_options;
		delete m_objectPrev;
		delete m_createUI;
		delete m_settingsBkp;

		ImGui_ImplSDL2_Shutdown();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();
	}

	void GUIManager::OnEvent(const SDL_Event& e)
	{
		m_imguiHandleEvent(e);

		if (m_splashScreen) {

			return;
		}

		// check for shortcut presses
		if (e.type == SDL_KEYDOWN) {
			if (!(m_optionsOpened && ((OptionsUI*)m_options)->IsListening())) {
				bool codeHasFocus = ((CodeEditorUI*)Get(ViewID::Code))->HasFocus();

				if (!(ImGui::GetIO().WantTextInput && !codeHasFocus)) {
					KeyboardShortcuts::Instance().Check(e, codeHasFocus);
					((CodeEditorUI*)Get(ViewID::Code))->RequestedProjectSave = false;
				}
			}
		} else if (e.type == SDL_MOUSEMOTION)
			m_perfModeClock.Restart();
		else if (e.type == SDL_DROPFILE) {

			char* droppedFile = e.drop.file;

			std::string file = m_data->Parser.GetRelativePath(droppedFile);
			size_t dotPos = file.find_last_of('.');

			if (!file.empty() && dotPos != std::string::npos) {
				std::string ext = file.substr(dotPos + 1);

				const std::vector<std::string> imgExt = { "png", "jpeg", "jpg", "bmp", "gif", "psd", "pic", "pnm", "hdr", "tga" };
				const std::vector<std::string> sndExt = { "ogg", "wav", "flac", "aiff", "raw" }; // TODO: more file ext
				const std::vector<std::string> projExt = { "sprj" };

				if (std::count(projExt.begin(), projExt.end(), ext) > 0) {
					bool cont = true;
					if (m_data->Parser.IsProjectModified()) {
						int btnID = this->AreYouSure();
						if (btnID == 2)
							cont = false;
					}

					if (cont)
						Open(m_data->Parser.GetProjectPath(file));
				} else if (std::count(imgExt.begin(), imgExt.end(), ext) > 0)
					m_data->Objects.CreateTexture(file);
				else if (std::count(sndExt.begin(), sndExt.end(), ext) > 0)
					m_data->Objects.CreateAudio(file);
				else
					m_data->Plugins.HandleDropFile(file.c_str());
			}

			SDL_free(droppedFile);
		} else if (e.type == SDL_WINDOWEVENT) {
			if (e.window.event == SDL_WINDOWEVENT_MOVED || e.window.event == SDL_WINDOWEVENT_MAXIMIZED || e.window.event == SDL_WINDOWEVENT_RESIZED) {
				SDL_GetWindowSize(m_wnd, &m_width, &m_height);
			}
		}

		if (m_optionsOpened)
			m_options->OnEvent(e);

		if (((ed::ObjectPreviewUI*)m_objectPrev)->ShouldRun())
			m_objectPrev->OnEvent(e);

		for (auto& view : m_views)
			view->OnEvent(e);
		for (auto& dview : m_debugViews)
			dview->OnEvent(e);

		m_data->Plugins.OnEvent(e);
	}
	void GUIManager::Update(float delta)
	{
		// add star to the titlebar if project was modified
		if (m_cacheProjectModified != m_data->Parser.IsProjectModified()) {
			std::string projName = m_data->Parser.GetOpenedFile();

			if (projName.size() > 0) {
				projName = projName.substr(projName.find_last_of("/\\") + 1);

				if (m_data->Parser.IsProjectModified())
					projName = "*" + projName;

				SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());
			} else {
				if (m_data->Parser.IsProjectModified())
					SDL_SetWindowTitle(m_wnd, "SHADERed (*)");
				else
					SDL_SetWindowTitle(m_wnd, "SHADERed");
			}

			m_cacheProjectModified = m_data->Parser.IsProjectModified();
		}

		Settings& settings = Settings::Instance();
		m_performanceMode = m_perfModeFake;

		// update audio textures
		FunctionVariableManager::Instance().ClearVariableList();

		// update editor & workspace font
		if (((CodeEditorUI*)Get(ViewID::Code))->NeedsFontUpdate() || ((m_cachedFont != settings.General.Font || m_cachedFontSize != settings.General.FontSize) && strcmp(settings.General.Font, "null") != 0) || m_fontNeedsUpdate) {
			Logger::Get().Log("Updating fonts...");

			std::pair<std::string, int> edFont = ((CodeEditorUI*)Get(ViewID::Code))->GetFont();

			m_cachedFont = settings.General.Font;
			m_cachedFontSize = settings.General.FontSize;
			m_fontNeedsUpdate = false;

			ImFontAtlas* fonts = ImGui::GetIO().Fonts;
			fonts->Clear();

			ImFont* font = nullptr;
			if (fs::exists(m_cachedFont))
				font = fonts->AddFontFromFileTTF(m_cachedFont.c_str(), m_cachedFontSize * Settings::Instance().DPIScale);

			// icon font
			static const ImWchar icon_ranges[] = { 0xea5b, 0xf026, 0 };
			if (font && fs::exists("data/icofont.ttf")) {
				ImFontConfig config;
				config.MergeMode = true;
				fonts->AddFontFromFileTTF("data/icofont.ttf", m_cachedFontSize * Settings::Instance().DPIScale, &config, icon_ranges);
			}

			ImFont* edFontPtr = nullptr;
			if (fs::exists(edFont.first))
				edFontPtr = fonts->AddFontFromFileTTF(edFont.first.c_str(), edFont.second * Settings::Instance().DPIScale);

			if (font == nullptr || edFontPtr == nullptr) {
				fonts->Clear();
				font = fonts->AddFontDefault();
				edFontPtr = fonts->AddFontDefault();

				Logger::Get().Log("Failed to load fonts", true);
			}

			// icon font large
			if (fs::exists("data/icofont.ttf")) {
				ImFontConfig configIconsLarge;
				m_iconFontLarge = ImGui::GetIO().Fonts->AddFontFromFileTTF("data/icofont.ttf", Settings::Instance().CalculateSize(TOOLBAR_HEIGHT / 2), &configIconsLarge, icon_ranges);
			}

			ImGui::GetIO().FontDefault = font;
			fonts->Build();

			ImGui_ImplOpenGL3_DestroyFontsTexture();
			ImGui_ImplOpenGL3_CreateFontsTexture();

			((CodeEditorUI*)Get(ViewID::Code))->UpdateFont();
			((OptionsUI*)Get(ViewID::Options))->ApplyTheme(); // reset paddings, etc...
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_wnd);
		ImGui::NewFrame();

		// splash screen
		if (m_splashScreen) {
			m_splashScreenRender();
			return;
		}

		// toolbar
		static bool initializedToolbar = false;
		bool actuallyToolbar = settings.General.Toolbar && !m_performanceMode && !m_minimalMode;
		if (!initializedToolbar) { // some hacks ew
			m_renderToolbar();
			initializedToolbar = true;
		} else if (actuallyToolbar)
			m_renderToolbar();

		// create a fullscreen imgui panel that will host a dockspace
		bool showMenu = !m_minimalMode && !(m_performanceMode && settings.Preview.HideMenuInPerformanceMode && m_perfModeClock.GetElapsedTime() > 2.5f);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | (ImGuiWindowFlags_MenuBar * showMenu) | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + actuallyToolbar * Settings::Instance().CalculateSize(TOOLBAR_HEIGHT)));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - actuallyToolbar * Settings::Instance().CalculateSize(TOOLBAR_HEIGHT)));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpaceWnd", 0, window_flags);
		ImGui::PopStyleVar(3);

		// DockSpace
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable && !m_performanceMode && !m_minimalMode) {
			ImGuiID dockspace_id = ImGui::GetID("DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		}

		// rebuild
		if (((CodeEditorUI*)Get(ViewID::Code))->TrackedFilesNeedUpdate()) {
			if (!m_recompiledAll) {
				std::vector<bool> needsUpdate = ((CodeEditorUI*)Get(ViewID::Code))->TrackedNeedsUpdate();
				std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
				int ind = 0;
				if (needsUpdate.size() >= passes.size()) {
					for (PipelineItem*& pass : passes) {
						if (needsUpdate[ind])
							m_data->Renderer.Recompile(pass->Name);
						ind++;
					}
				}
			}

			((CodeEditorUI*)Get(ViewID::Code))->EmptyTrackedFiles();
		}
		((CodeEditorUI*)Get(ViewID::Code))->UpdateAutoRecompileItems();

		// parse
		if (!m_data->Renderer.SPIRVQueue.empty()) {
			auto& spvQueue = m_data->Renderer.SPIRVQueue;
			CodeEditorUI* codeEditor = ((CodeEditorUI*)Get(ViewID::Code));
			for (int i = 0; i < spvQueue.size(); i++) {
				bool hasDups = false;

				PipelineItem* spvItem = spvQueue[i];

				if (i + 1 < spvQueue.size())
					if (std::count(spvQueue.begin() + i + 1, spvQueue.end(), spvItem) > 0)
						hasDups = true;
			
				if (!hasDups) {
					SPIRVParser spvParser;
					if (spvItem->Type == PipelineItem::ItemType::ShaderPass) {
						pipe::ShaderPass* pass = (pipe::ShaderPass*)spvItem->Data;
						std::vector<std::string> allUniforms;
						
						bool deleteUnusedVariables = true;

						if (pass->PSSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->PSPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->PSSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Pixel);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}
						if (pass->VSSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->VSPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->VSSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Vertex);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}
						if (pass->GSSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->GSPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->GSSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Geometry);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}

						if (settings.General.AutoUniforms && deleteUnusedVariables && settings.General.AutoUniformsDelete && pass->VSSPV.size() > 0 && pass->PSSPV.size() > 0 && ((pass->GSUsed && pass->GSSPV.size()>0) || !pass->GSUsed))
							m_deleteUnusedUniforms(pass->Variables, allUniforms);
					} else if (spvItem->Type == PipelineItem::ItemType::ComputePass) {
						pipe::ComputePass* pass = (pipe::ComputePass*)spvItem->Data;
						std::vector<std::string> allUniforms;

						if (pass->SPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->Path, m_data->Plugins.Plugins());

							spvParser.Parse(pass->SPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Compute);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)))) {
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
								if (settings.General.AutoUniformsDelete)
									m_deleteUnusedUniforms(pass->Variables, allUniforms);
							}
						}
					} else if (spvItem->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* pass = (pipe::PluginItemData*)spvItem->Data;
						std::vector<std::string> allUniforms;

						for (int k = 0; k < (int)ShaderStage::Count; k++) {
							TextEditor* tEdit = codeEditor->Get(spvItem, (ed::ShaderStage)k);
							if (tEdit == nullptr) continue;

							unsigned int spvSize = pass->Owner->PipelineItem_GetSPIRVSize(pass->Type, pass->PluginData, (plugin::ShaderStage)k);
							if (spvSize > 0) {
								unsigned int* spv = pass->Owner->PipelineItem_GetSPIRV(pass->Type, pass->PluginData, (plugin::ShaderStage)k);
								std::vector<unsigned int> spvVec(spv, spv + spvSize);

								spvParser.Parse(spvVec);
								codeEditor->FillAutocomplete(tEdit, spvParser);
							}
						}
					}
				}
			}
			spvQueue.clear();
		}

		// menu
		if (showMenu && ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::BeginMenu("New")) {
					if (ImGui::MenuItem("Empty")) {
						bool cont = true;
						if (m_data->Parser.IsProjectModified()) {
							int btnID = this->AreYouSure();
							if (btnID == 2)
								cont = false;
						}

						if (cont) {
							m_selectedTemplate = "?empty";
							m_isNewProjectPopupOpened = true;
						}
					}
					ImGui::Separator();

					for (int i = 0; i < m_templates.size(); i++)
						if (ImGui::MenuItem(m_templates[i].c_str())) {
							bool cont = true;
							if (m_data->Parser.IsProjectModified()) {
								int btnID = this->AreYouSure();
								if (btnID == 2)
									cont = false;
							}

							if (cont) {
								m_selectedTemplate = m_templates[i];
								m_isNewProjectPopupOpened = true;
							}
						}

					m_data->Plugins.ShowMenuItems("newproject");

					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Open", KeyboardShortcuts::Instance().GetString("Project.Open").c_str())) {
					
					bool cont = true;
					if (m_data->Parser.IsProjectModified()) {
						int btnID = this->AreYouSure();
						if (btnID == 2)
							cont = false;
					}

					if (cont)
						igfd::ImGuiFileDialog::Instance()->OpenModal("OpenProjectDlg", "Open SHADERed project", "SHADERed project (*.sprj){.sprj},.*", ".");
				}
				if (ImGui::BeginMenu("Open Recent")) {
					int recentCount = 0;
					for (int i = 0; i < m_recentProjects.size(); i++) {
						fs::path path(m_recentProjects[i]);
						if (!fs::exists(path))
							continue;

						recentCount++;
						if (ImGui::MenuItem(path.filename().string().c_str())) {
							bool cont = true;
							if (m_data->Parser.IsProjectModified()) {
								int btnID = this->AreYouSure();
								if (btnID == 2)
									cont = false;
							}

							if (cont)
								this->Open(m_recentProjects[i]);
						}
					}

					if (recentCount == 0)
						ImGui::Text("No projects opened recently");

					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Browse online")) {
					m_isBrowseOnlineOpened = true;
				}
				if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("Project.Save").c_str()))
					Save();
				if (ImGui::MenuItem("Save As", KeyboardShortcuts::Instance().GetString("Project.SaveAs").c_str()))
					SaveAsProject(true);
				if (ImGui::MenuItem("Save Preview as Image", KeyboardShortcuts::Instance().GetString("Preview.SaveImage").c_str()))
					m_savePreviewPopupOpened = true;
				if (ImGui::MenuItem("Open project directory")) {
					std::string prpath = m_data->Parser.GetProjectPath("");
#if defined(__APPLE__)
					system(("open " + prpath).c_str()); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
					system(("xdg-open " + prpath).c_str());
#elif defined(_WIN32)
					ShellExecuteA(NULL, "open", prpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
				}

				if (ImGui::BeginMenu("Export")) {
					if (ImGui::MenuItem("as C++ project"))
						m_exportAsCPPOpened = true;

					ImGui::EndMenu();
				}

				m_data->Plugins.ShowMenuItems("file");

				ImGui::Separator();
				if (ImGui::MenuItem("Exit", KeyboardShortcuts::Instance().GetString("Window.Exit").c_str())) {
					SDL_Event event;
					event.type = SDL_QUIT;
					SDL_PushEvent(&event);
					return;
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Project")) {
				if (ImGui::MenuItem("Rebuild project", KeyboardShortcuts::Instance().GetString("Project.Rebuild").c_str())) {
					((CodeEditorUI*)Get(ViewID::Code))->SaveAll();

					std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
					for (PipelineItem*& pass : passes)
						m_data->Renderer.Recompile(pass->Name);
				}
				if (ImGui::MenuItem("Render", KeyboardShortcuts::Instance().GetString("Preview.SaveImage").c_str()))
					m_savePreviewPopupOpened = true;
				if (ImGui::BeginMenu("Create")) {
					if (ImGui::MenuItem("Shader Pass", KeyboardShortcuts::Instance().GetString("Project.NewShaderPass").c_str()))
						this->CreateNewShaderPass();
					if (ImGui::MenuItem("Compute Pass", KeyboardShortcuts::Instance().GetString("Project.NewComputePass").c_str()))
						this->CreateNewComputePass();
					if (ImGui::MenuItem("Audio Pass", KeyboardShortcuts::Instance().GetString("Project.NewAudioPass").c_str()))
						this->CreateNewAudioPass();
					ImGui::Separator();
					if (ImGui::MenuItem("Texture", KeyboardShortcuts::Instance().GetString("Project.NewTexture").c_str()))
						this->CreateNewTexture();
					if (ImGui::MenuItem("Cubemap", KeyboardShortcuts::Instance().GetString("Project.NewCubeMap").c_str()))
						this->CreateNewCubemap();
					if (ImGui::MenuItem("Audio", KeyboardShortcuts::Instance().GetString("Project.NewAudio").c_str()))
						this->CreateNewAudio();
					if (ImGui::MenuItem("Render Texture", KeyboardShortcuts::Instance().GetString("Project.NewRenderTexture").c_str()))
						this->CreateNewRenderTexture();
					if (ImGui::MenuItem("Buffer", KeyboardShortcuts::Instance().GetString("Project.NewBuffer").c_str()))
						this->CreateNewBuffer();
					if (ImGui::MenuItem("Empty image", KeyboardShortcuts::Instance().GetString("Project.NewImage").c_str()))
						this->CreateNewImage();
					if (ImGui::MenuItem("Empty 3D image", KeyboardShortcuts::Instance().GetString("Project.NewImage3D").c_str()))
						this->CreateNewImage3D();

					
					bool hasKBTexture = m_data->Objects.HasKeyboardTexture();
					if (hasKBTexture) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}
					if (ImGui::MenuItem("KeyboardTexture", KeyboardShortcuts::Instance().GetString("Project.NewKeyboardTexture").c_str()))
						this->CreateKeyboardTexture();
					if (hasKBTexture) {
						ImGui::PopStyleVar();
						ImGui::PopItemFlag();
					}
					
					m_data->Plugins.ShowMenuItems("createitem");

					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Camera snapshots")) {
					if (ImGui::MenuItem("Add", KeyboardShortcuts::Instance().GetString("Project.CameraSnapshot").c_str())) CreateNewCameraSnapshot();
					if (ImGui::BeginMenu("Delete")) {
						auto& names = CameraSnapshots::GetList();
						for (const auto& name : names)
							if (ImGui::MenuItem(name.c_str()))
								CameraSnapshots::Remove(name);
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Reset time")) {
					SystemVariableManager::Instance().Reset();
					if (!m_data->Debugger.IsDebugging() && m_data->Debugger.GetPixelList().size() == 0)
						m_data->Renderer.Render();
				}
				if (ImGui::MenuItem("Reload textures")) {
					const std::vector<std::string>& objs = m_data->Objects.GetObjects();
					
					for (const auto& obj : objs) {
						ObjectManagerItem* item = m_data->Objects.GetObjectManagerItem(obj);
						if (item->IsTexture && !item->IsKeyboardTexture && !item->IsCube)
							m_data->Objects.ReloadTexture(item, obj);
					}
				}
				if (ImGui::MenuItem("Options")) {
					m_optionsOpened = true;
					((OptionsUI*)m_options)->SetGroup(ed::OptionsUI::Page::Project);
					m_optGroup = (int)OptionsUI::Page::Project;
					*m_settingsBkp = settings;
					m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
				}

				m_data->Plugins.ShowMenuItems("project");

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window")) {
				for (auto& view : m_views) {
					if (view->Name != "Code") // dont show the "Code" UI view in this menu
						ImGui::MenuItem(view->Name.c_str(), 0, &view->Visible);
				}

				if (ImGui::BeginMenu("Debug")) {
					if (!m_data->Debugger.IsDebugging()) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					for (auto& dview : m_debugViews) {
#ifndef BUILD_IMMEDIATE_MODE
						bool isTempDisabled = (dview->Name == "Immediate" || dview->Name == "Watches"); // remove this later

						if (isTempDisabled) {
							ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
							ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
							dview->Visible = false;
						}
#endif

						ImGui::MenuItem(dview->Name.c_str(), 0, &dview->Visible);

#ifndef BUILD_IMMEDIATE_MODE
						if (isTempDisabled) {
							ImGui::PopStyleVar();
							ImGui::PopItemFlag();
						}
#endif
					}

					if (!m_data->Debugger.IsDebugging()) {
						ImGui::PopStyleVar();
						ImGui::PopItemFlag();
					}

					ImGui::EndMenu();
				}

				m_data->Plugins.ShowMenuItems("window");

				ImGui::Separator();

				ImGui::MenuItem("Performance Mode", KeyboardShortcuts::Instance().GetString("Workspace.PerformanceMode").c_str(), &m_perfModeFake);
				if (ImGui::MenuItem("Options", KeyboardShortcuts::Instance().GetString("Workspace.Options").c_str())) {
					m_optionsOpened = true;
					*m_settingsBkp = settings;
					m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				if (ImGui::BeginMenu("Support")) {
					if (ImGui::MenuItem("Patreon"))
						UIHelper::ShellOpen("https://www.patreon.com/dfranx");
					if (ImGui::MenuItem("PayPal"))
						UIHelper::ShellOpen("https://www.paypal.me/dfranx");
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Tutorial"))
					UIHelper::ShellOpen("http://docs.shadered.org");

				if (ImGui::MenuItem("Send feedback"))
					UIHelper::ShellOpen("https://www.github.com/dfranx/SHADERed/issues");

				if (ImGui::MenuItem("Information")) { m_isInfoOpened = true; }
				if (ImGui::MenuItem("About SHADERed")) { m_isAboutOpen = true; }

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Supporters")) {
				static const std::vector<std::pair<std::string, std::string>> slist = {
					std::make_pair("Hugo Locurcio", "https://hugo.pro"),
					std::make_pair("Vladimir Alyamkin", "https://alyamkin.com"),
					std::make_pair("Wogos Media", "http://theWogos.com"),
				};

				for (auto& sitem : slist)
					if (ImGui::MenuItem(sitem.first.c_str()))
						UIHelper::ShellOpen(sitem.second);

				ImGui::EndMenu();
			}
			m_data->Plugins.ShowCustomMenu();
			ImGui::EndMainMenuBar();
		}

		if (m_performanceMode || m_minimalMode)
			((PreviewUI*)Get(ViewID::Preview))->Update(delta);

		ImGui::End();

		if (!m_performanceMode && !m_minimalMode) {
			m_data->Plugins.Update(delta);

			for (auto& view : m_views)
				if (view->Visible) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width * 2, m_height * 2));
					if (ImGui::Begin(view->Name.c_str(), &view->Visible)) view->Update(delta);
					ImGui::End();
				}
			if (m_data->Debugger.IsDebugging()) {
				for (auto& dview : m_debugViews) {
#ifdef BUILD_IMMEDIATE_MODE
					if (dview->Visible) {
						ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width * 2, m_height * 2));
						if (ImGui::Begin(dview->Name.c_str(), &dview->Visible)) dview->Update(delta);
						ImGui::End();
					}
#else
					if (dview->Visible && (dview->Name != "Immediate" && dview->Name != "Watches")) {
						ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width * 2, m_height * 2));
						if (ImGui::Begin(dview->Name.c_str(), &dview->Visible)) dview->Update(delta);
						ImGui::End();
					}
#endif
				}
			}

			Get(ViewID::Code)->Update(delta);

			// object preview
			if (((ed::ObjectPreviewUI*)m_objectPrev)->ShouldRun())
				m_objectPrev->Update(delta);
		}

		// handle the "build occured" event
		if (settings.General.AutoOpenErrorWindow && m_data->Messages.BuildOccured) {
			size_t errors = m_data->Messages.GetErrorAndWarningMsgCount();
			if (errors > 0 && !Get(ViewID::Output)->Visible)
				Get(ViewID::Output)->Visible = true;
			m_data->Messages.BuildOccured = false;
		}

		// render options window
		if (m_optionsOpened) {
			ImGui::Begin("Options", &m_optionsOpened, ImGuiWindowFlags_NoDocking);
			m_renderOptions();
			ImGui::End();
		}

		// open popup for creating items
		if (m_isCreateItemPopupOpened) {
			ImGui::OpenPopup("Create Item##main_create_item");
			m_isCreateItemPopupOpened = false;
		}

		// open popup for saving preview as image
		if (m_savePreviewPopupOpened) {
			ImGui::OpenPopup("Save Preview##main_save_preview");
			m_previewSavePath = "render.png";
			m_savePreviewPopupOpened = false;
			m_wasPausedPrior = m_data->Renderer.IsPaused();
			m_savePreviewCachedTime = m_savePreviewTime = SystemVariableManager::Instance().GetTime();
			m_savePreviewTimeDelta = SystemVariableManager::Instance().GetTimeDelta();
			m_savePreviewCachedFIndex = m_savePreviewFrameIndex = SystemVariableManager::Instance().GetFrameIndex();
			glm::ivec4 wasd = SystemVariableManager::Instance().GetKeysWASD();
			m_savePreviewWASD[0] = wasd.x;
			m_savePreviewWASD[1] = wasd.y;
			m_savePreviewWASD[2] = wasd.z;
			m_savePreviewWASD[3] = wasd.w;
			m_savePreviewMouse = SystemVariableManager::Instance().GetMouse();
			m_savePreviewSupersample = 0;

			m_savePreviewSeq = false;

			m_data->Renderer.Pause(true);
		}

		// open popup for creating cubemap
		if (m_isCreateCubemapOpened) {
			ImGui::OpenPopup("Create cubemap##main_create_cubemap");
			m_isCreateCubemapOpened = false;
		}

		// open popup for creating buffer
		if (m_isCreateBufferOpened) {
			ImGui::OpenPopup("Create buffer##main_create_buffer");
			m_isCreateBufferOpened = false;
		}

		// open popup for creating image
		if (m_isCreateImgOpened) {
			ImGui::OpenPopup("Create image##main_create_img");
			m_isCreateImgOpened = false;
		}

		// open popup for creating image3D
		if (m_isCreateImg3DOpened) {
			ImGui::OpenPopup("Create 3D image##main_create_img3D");
			m_isCreateImg3DOpened = false;
		}

		// open popup that shows the list of incompatible plugins
		if (m_isIncompatPluginsOpened) {
			ImGui::OpenPopup("Incompatible plugins##main_incompat_plugins");
			m_isIncompatPluginsOpened = false;
		}

		// open popup for creating camera snapshot
		if (m_isRecordCameraSnapshotOpened) {
			ImGui::OpenPopup("Camera snapshot name##main_camsnap_name");
			m_isRecordCameraSnapshotOpened = false;
		}

		// open popup for creating render texture
		if (m_isCreateRTOpened) {
			ImGui::OpenPopup("Create RT##main_create_rt");
			m_isCreateRTOpened = false;
		}

		// open popup for creating keyboard texture
		if (m_isCreateKBTxtOpened) {
			ImGui::OpenPopup("Create KeyboardTexture##main_create_kbtxt");
			m_isCreateKBTxtOpened = false;
		}

		// open popup for opening new project
		if (m_isNewProjectPopupOpened) {
			ImGui::OpenPopup("Are you sure?##main_new_proj");
			m_isNewProjectPopupOpened = false;
		}

		// open about popup
		if (m_isAboutOpen) {
			ImGui::OpenPopup("About##main_about");
			m_isAboutOpen = false;
		}

		// open tips window
		if (m_isInfoOpened) {
			ImGui::OpenPopup("Information##main_info");
			m_isInfoOpened = false;
		}

		// open export as c++ app
		if (m_exportAsCPPOpened) {
			ImGui::OpenPopup("Export as C++ project##main_export_as_cpp");
			m_expcppError = false;
			m_exportAsCPPOpened = false;
		}

		// open changelog popup
		if (m_isChangelogOpened) {
			ImGui::OpenPopup("Changelog##main_upd_changelog");
			m_isChangelogOpened = false;
		}

		// open tips popup
		if (m_tipOpened) {
			ImGui::OpenPopup("###main_tips");
			m_tipOpened = false;
		}

		// open browse online window
		if (m_isBrowseOnlineOpened) {
			ImGui::OpenPopup("Browse online##browse_online");

			memset(&m_onlineQuery, 0, sizeof(m_onlineQuery));
			memset(&m_onlineUsername, 0, sizeof(m_onlineUsername));

			if (m_onlineShaders.size() == 0)
				m_onlineSearchShaders();
			if (m_onlinePlugins.size() == 0)
				m_onlineSearchPlugins();
			if (m_onlineThemes.size() == 0)
				m_onlineSearchThemes();

			m_isBrowseOnlineOpened = false;
		}

		// File dialogs (open project, create texture, create audio, pick cubemap face texture)
		if (igfd::ImGuiFileDialog::Instance()->FileDialog("OpenProjectDlg")) {
			if (igfd::ImGuiFileDialog::Instance()->IsOk) {
				std::string filePathName = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
				Open(filePathName);
			}

			igfd::ImGuiFileDialog::Instance()->CloseDialog("OpenProjectDlg");
		}
		if (igfd::ImGuiFileDialog::Instance()->FileDialog("CreateTextureDlg")) {
			if (igfd::ImGuiFileDialog::Instance()->IsOk) {
				auto sel = igfd::ImGuiFileDialog::Instance()->GetSelection();

				for (auto pair : sel) {
					std::string file = m_data->Parser.GetRelativePath(pair.second);
					if (!file.empty())
						m_data->Objects.CreateTexture(file);
				}
			}

			igfd::ImGuiFileDialog::Instance()->CloseDialog("CreateTextureDlg");
		}
		if (igfd::ImGuiFileDialog::Instance()->FileDialog("CreateAudioDlg")) {
			if (igfd::ImGuiFileDialog::Instance()->IsOk) {
				std::string filepath = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
				std::string rfile = m_data->Parser.GetRelativePath(filepath);
				if (!rfile.empty())
					m_data->Objects.CreateAudio(rfile);
			}

			igfd::ImGuiFileDialog::Instance()->CloseDialog("CreateAudioDlg");
		}
		if (igfd::ImGuiFileDialog::Instance()->FileDialog("SaveProjectDlg")) {
			if (igfd::ImGuiFileDialog::Instance()->IsOk) {
				if (m_saveAsPreHandle)
					m_saveAsPreHandle();

				std::string file = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
				m_data->Parser.SaveAs(file, true);

				// cache opened code editors
				CodeEditorUI* editor = ((CodeEditorUI*)Get(ViewID::Code));
				std::vector<std::pair<std::string, ShaderStage>> files = editor->GetOpenedFiles();
				std::vector<std::string> filesData = editor->GetOpenedFilesData();

				// close all
				this->ResetWorkspace();

				m_data->Parser.Open(file);

				std::string projName = m_data->Parser.GetOpenedFile();
				projName = projName.substr(projName.find_last_of("/\\") + 1);

				SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());

				// return cached state
				if (m_saveAsRestoreCache) {
					for (auto& file : files) {
						PipelineItem* item = m_data->Pipeline.Get(file.first.c_str());
						editor->Open(item, file.second);
					}
					editor->SetOpenedFilesData(filesData);
					editor->SaveAll();
				}
			}

			if (m_saveAsHandle != nullptr)
				m_saveAsHandle(igfd::ImGuiFileDialog::Instance()->IsOk);

			igfd::ImGuiFileDialog::Instance()->CloseDialog("SaveProjectDlg");
		}

		// Create RT popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(830), Settings::Instance().CalculateSize(550)), ImGuiCond_FirstUseEver);
		if (ImGui::BeginPopupModal("Browse online##browse_online")) {
			int startY = ImGui::GetCursorPosY();
			ImGui::Text("Query:");
			ImGui::SameLine();
			ImGui::SetCursorPosX(Settings::Instance().CalculateSize(75));
			ImGui::PushItemWidth(-Settings::Instance().CalculateSize(110));
			ImGui::InputText("##online_query", m_onlineQuery, sizeof(m_onlineQuery));
			ImGui::PopItemWidth();
			int startX = ImGui::GetCursorPosX();

			ImGui::Text("By:");
			ImGui::SameLine();
			ImGui::SetCursorPosX(Settings::Instance().CalculateSize(75));
			ImGui::PushItemWidth(-Settings::Instance().CalculateSize(110));
			ImGui::InputText("##online_username", m_onlineUsername, sizeof(m_onlineUsername));
			ImGui::PopItemWidth();
			int endY = ImGui::GetCursorPosY();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(90), startY));
			if (ImGui::Button("SEARCH", ImVec2(Settings::Instance().CalculateSize(70), endY-startY))) {
				if (m_onlineIsShader)
					m_onlineShaderPage = 0;
				else if (m_onlineIsPlugin)
					m_onlinePluginPage = 0;
				else
					m_onlineThemePage = 0;
				m_onlineSearchShaders();
			}

			bool hasNext = true;

			if (ImGui::BeginTabBar("BrowseOnlineTabBar")) {
				if (ImGui::BeginTabItem("Shaders")) {
					m_onlineIsShader = true;
					m_onlineIsPlugin = false;
					m_onlinePage = m_onlineShaderPage;
					hasNext = m_onlineShaders.size() > 12;

					ImGui::BeginChild("##online_shader_container", ImVec2(0, Settings::Instance().CalculateSize(-60)));

					if (ImGui::BeginTable("##shaders_table", 2, ImGuiTableFlags_None)) {
						ImGui::TableSetupColumn("Thumbnail", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
						ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableAutoHeaders();

						for (int i = 0; i < std::min<int>(12, m_onlineShaders.size()); i++) {
							const ed::WebAPI::ShaderResult& shaderInfo = m_onlineShaders[i];

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::Image((ImTextureID)m_onlineShaderThumbnail[i], ImVec2(256, 144), ImVec2(0, 1), ImVec2(1, 0));

							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%s", shaderInfo.Title.c_str()); // just in case someone has a %s or sth in the title, so that the app doesn't crash :'D
							ImGui::Text("Language: %s", shaderInfo.Language.c_str());
							ImGui::Text("%d view(s)", shaderInfo.Views);
							ImGui::Text("by: %s", shaderInfo.Owner.c_str());

							ImGui::PushID(i);
							if (ImGui::Button("OPEN")) {
								CodeEditorUI* codeUI = ((CodeEditorUI*)Get(ViewID::Code));
								codeUI->SetTrackFileChanges(false);

								bool ret = m_data->API.DownloadShaderProject(shaderInfo.ID);
								if (ret) {
									std::string outputPath = "temp/";
									if (!ed::Settings::Instance().LinuxHomeDirectory.empty())
										outputPath = ed::Settings::Instance().LinuxHomeDirectory + "temp/";

									Open(outputPath + "project.sprj");
									m_data->Parser.SetOpenedFile("");
									ImGui::CloseCurrentPopup();
								}

								codeUI->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
							}
							ImGui::PopID();
						}

						ImGui::EndTable();
					}

					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Plugins")) {
					m_onlineIsShader = false;
					m_onlineIsPlugin = true;
					m_onlinePage = m_onlinePluginPage;
					hasNext = m_onlinePlugins.size() > 12;

					ImGui::BeginChild("##online_plugin_container", ImVec2(0, Settings::Instance().CalculateSize(-60)));

					if (ImGui::BeginTable("##plugins_table", 2, ImGuiTableFlags_None)) {
						ImGui::TableSetupColumn("Thumbnail", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
						ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableAutoHeaders();

						for (int i = 0; i < std::min<int>(12, m_onlinePlugins.size()); i++) {
							const ed::WebAPI::PluginResult& pluginInfo = m_onlinePlugins[i];

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::Image((ImTextureID)m_onlinePluginThumbnail[i], ImVec2(256, 144), ImVec2(0, 1), ImVec2(1, 0));

							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%s", pluginInfo.Title.c_str()); // just in case someone has a %s or sth in the title, so that the app doesn't crash :'D
							ImGui::TextWrapped("%s", pluginInfo.Description.c_str());
							ImGui::Text("%d download(s)", pluginInfo.Downloads);
							ImGui::Text("by: %s", pluginInfo.Owner.c_str());

							if (m_data->Plugins.GetPlugin(pluginInfo.ID) == nullptr && std::count(m_onlineInstalledPlugins.begin(), m_onlineInstalledPlugins.end(), pluginInfo.ID) == 0) {
								ImGui::PushID(i);
								if (ImGui::Button("DOWNLOAD")) {
									m_onlineInstalledPlugins.push_back(pluginInfo.ID); // since PluginManager's GetPlugin won't register it immediately
									m_data->API.DownloadPlugin(pluginInfo.ID);
								}
								ImGui::PopID();
							} else {
								ImGui::Text("Plugin already installed!");
							}
						}

						ImGui::EndTable();
					}

					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Themes")) {
					m_onlineIsShader = false;
					m_onlineIsPlugin = false;
					m_onlinePage = m_onlineThemePage;

					hasNext = m_onlineThemes.size() > 12;

					ImGui::BeginChild("##online_theme_container", ImVec2(0, Settings::Instance().CalculateSize(-60)));

					if (ImGui::BeginTable("##themes_table", 2, ImGuiTableFlags_None)) {
						ImGui::TableSetupColumn("Thumbnail", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
						ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableAutoHeaders();

						for (int i = 0; i < std::min<int>(12, m_onlineThemes.size()); i++) {
							const ed::WebAPI::ThemeResult& themeInfo = m_onlineThemes[i];

							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::Image((ImTextureID)m_onlineThemeThumbnail[i], ImVec2(256, 144), ImVec2(0, 1), ImVec2(1, 0));

							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%s", themeInfo.Title.c_str()); // just in case someone has a %s or sth in the title, so that the app doesn't crash :'D
							ImGui::TextWrapped("%s", themeInfo.Description.c_str());
							ImGui::Text("%d download(s)", themeInfo.Downloads);
							ImGui::Text("by: %s", themeInfo.Owner.c_str());

							const auto& tList = ((ed::OptionsUI*)m_options)->GetThemeList();

							if (std::count(tList.begin(), tList.end(), themeInfo.Title) == 0) {
								ImGui::PushID(i);
								if (ImGui::Button("DOWNLOAD")) {
									m_data->API.DownloadTheme(themeInfo.ID);
									((ed::OptionsUI*)m_options)->RefreshThemeList();
								}
								ImGui::PopID();
							} else {
								ImGui::PushID(i);
								if (ImGui::Button("APPLY")) {
									ed::Settings::Instance().Theme = themeInfo.Title;
									((ed::OptionsUI*)m_options)->ApplyTheme();
								}
								ImGui::PopID();
							}
						}

						ImGui::EndTable();
					}

					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}

			
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(180));
			if (ImGui::Button("<<", ImVec2(Settings::Instance().CalculateSize(70), 0))) {
				m_onlinePage = std::max<int>(0, m_onlinePage - 1);

				if (m_onlineIsShader) m_onlineShaderPage = m_onlinePage;
				else if (m_onlineIsPlugin) m_onlinePluginPage = m_onlinePage;
				else m_onlineThemePage = m_onlinePage;

				m_onlineSearchShaders();
			}
			ImGui::SameLine();
			ImGui::Text("%d", m_onlinePage + 1);
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(90));
			
			if (!hasNext) {
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::Button(">>", ImVec2(Settings::Instance().CalculateSize(70), 0))) {
				m_onlinePage++;
				
				if (m_onlineIsShader) m_onlineShaderPage = m_onlinePage;
				else if (m_onlineIsPlugin) m_onlinePluginPage = m_onlinePage;
				else m_onlineThemePage = m_onlinePage;

				m_onlineSearchShaders();
			}
			if (!hasNext) {
				ImGui::PopStyleVar();
				ImGui::PopItemFlag();
			}


			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(90));
			if (ImGui::Button("Close", ImVec2(Settings::Instance().CalculateSize(70), 0))) {
				ImGui::CloseCurrentPopup();
			}


			ImGui::EndPopup();
		}

		// Create Item popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(530), Settings::Instance().CalculateSize(300)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create Item##main_create_item", 0, ImGuiWindowFlags_NoResize)) {
			m_createUI->Update(delta);

			if (ImGui::Button("Ok")) {
				if (m_createUI->Create())
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				m_createUI->Reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// Create cubemap popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(275)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create cubemap##main_create_cubemap", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			static std::string left, top, front, bottom, right, back;
			float btnWidth = Settings::Instance().CalculateSize(65.0f);

			ImGui::Text("Left: %s", fs::path(left).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##left")) {
				m_cubemapPathPtr = &left;
				igfd::ImGuiFileDialog::Instance()->OpenModal("CubemapFaceDlg", "Select cubemap face - left", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
			}

			ImGui::Text("Top: %s", fs::path(top).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##top")) {
				m_cubemapPathPtr = &top;
				igfd::ImGuiFileDialog::Instance()->OpenModal("CubemapFaceDlg", "Select cubemap face - top", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
			}

			ImGui::Text("Front: %s", fs::path(front).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##front")) {
				m_cubemapPathPtr = &front;
				igfd::ImGuiFileDialog::Instance()->OpenModal("CubemapFaceDlg", "Select cubemap face - front", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
			}

			ImGui::Text("Bottom: %s", fs::path(bottom).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##bottom")) {
				m_cubemapPathPtr = &bottom;
				igfd::ImGuiFileDialog::Instance()->OpenModal("CubemapFaceDlg", "Select cubemap face - bottom", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
			}

			ImGui::Text("Right: %s", fs::path(right).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##right")) {
				m_cubemapPathPtr = &right;
				igfd::ImGuiFileDialog::Instance()->OpenModal("CubemapFaceDlg", "Select cubemap face - right", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
			}

			ImGui::Text("Back: %s", fs::path(back).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##back")) {
				m_cubemapPathPtr = &back;
				igfd::ImGuiFileDialog::Instance()->OpenModal("CubemapFaceDlg", "Select cubemap face - back", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
			}

			
			if (igfd::ImGuiFileDialog::Instance()->FileDialog("CubemapFaceDlg")) {
				if (igfd::ImGuiFileDialog::Instance()->IsOk && m_cubemapPathPtr != nullptr) {
					std::string filepath = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
					*m_cubemapPathPtr = m_data->Parser.GetRelativePath(filepath);
				}

				igfd::ImGuiFileDialog::Instance()->CloseDialog("CubemapFaceDlg");
			}

			if (ImGui::Button("Ok") && strlen(buf) > 0 && !m_data->Objects.Exists(buf)) {
				if (m_data->Objects.CreateCubemap(buf, left, top, front, bottom, right, back))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create RT popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create RT##main_create_rt", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 65);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateRenderTexture(buf)) {
					((PropertyUI*)Get(ViewID::Properties))->Open(buf, m_data->Objects.GetObjectManagerItem(buf));
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create KB texture
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create KeyboardTexture##main_create_kbtxt", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 65);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateKeyboardTexture(buf))
					ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Record cam snapshot
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Camera snapshot name##main_camsnap_name", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			if (ImGui::Button("Ok")) {
				bool exists = false;
				auto& names = CameraSnapshots::GetList();
				for (const auto& name : names) {
					if (strcmp(buf, name.c_str()) == 0) {
						exists = true;
						break;
					}
				}

				if (!exists) {
					CameraSnapshots::Add(buf, SystemVariableManager::Instance().GetViewMatrix());
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create buffer popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create buffer##main_create_buffer", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateBuffer(buf))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create empty image popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(175)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create image##main_create_img", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			static glm::ivec2 size(0, 0);

			ImGui::InputText("Name", buf, 64);
			ImGui::DragInt2("Size", glm::value_ptr(size));
			if (size.x <= 0) size.x = 1;
			if (size.y <= 0) size.y = 1;

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateImage(buf, size))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create empty 3D image
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(175)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create 3D image##main_create_img3D", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			static glm::ivec3 size(0, 0, 0);

			ImGui::InputText("Name", buf, 64);
			ImGui::DragInt3("Size", glm::value_ptr(size));
			if (size.x <= 0) size.x = 1;
			if (size.y <= 0) size.y = 1;
			if (size.z <= 0) size.z = 1;

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateImage3D(buf, size))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create about popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(270), Settings::Instance().CalculateSize(220)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("About##main_about", 0, ImGuiWindowFlags_NoResize)) {
			ImGui::TextWrapped("(C) 2020 dfranx");
			ImGui::TextWrapped("Version 1.4");
			ImGui::TextWrapped("Internal version: %d", WebAPI::InternalVersion);
			ImGui::NewLine();
			UIHelper::Markdown("This app is open sourced: [link](https://www.github.com/dfranx/SHADERed)");
			ImGui::NewLine();

			ImGui::Separator();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create "Tips" popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(550), Settings::Instance().CalculateSize(460)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Information##main_info")) {
			ImGui::TextWrapped("Here you can find random information about various features");

			ImGui::TextWrapped("System variables");
			ImGui::Separator();
			ImGui::TextWrapped("Time (float) - time elapsed since app start");
			ImGui::TextWrapped("TimeDelta (float) - render time");
			ImGui::TextWrapped("FrameIndex (uint) - current frame index");
			ImGui::TextWrapped("ViewportSize (vec2) - rendering window size");
			ImGui::TextWrapped("MousePosition (vec2) - normalized mouse position in the Preview window");
			ImGui::TextWrapped("View (mat4) - a built-in camera matrix");
			ImGui::TextWrapped("Projection (mat4) - a built-in projection matrix");
			ImGui::TextWrapped("ViewProjection (mat4) - View*Projection matrix");
			ImGui::TextWrapped("Orthographic (mat4) - an orthographic matrix");
			ImGui::TextWrapped("ViewOrthographic (mat4) - View*Orthographic");
			ImGui::TextWrapped("GeometryTransform (mat4) - applies Scale, Rotation and Position to geometry");
			ImGui::TextWrapped("IsPicked (bool) - check if current item is selected");
			ImGui::TextWrapped("CameraPosition (vec4) - current camera position");
			ImGui::TextWrapped("CameraPosition3 (vec3) - current camera position");
			ImGui::TextWrapped("CameraDirection3 (vec3) - camera view direction");
			ImGui::TextWrapped("KeysWASD (vec4) - W, A, S or D keys state");
			ImGui::TextWrapped("Mouse (vec4) - vec4(x,y,left,right) updated every frame");
			ImGui::TextWrapped("MouseButton (vec4) - vec4(viewX,viewY,clickX,clickY) updated only when left mouse button is down");

			ImGui::NewLine();
			ImGui::Separator();

			ImGui::TextWrapped("KeyboardTexture:");
			ImGui::PushFont(((CodeEditorUI*)Get(ViewID::Code))->GetImFont());
			m_kbInfo.Render("##kbtext_info", ImVec2(0, 600), true);
			ImGui::PopFont();

			ImGui::NewLine();
			ImGui::Separator();

			ImGui::TextWrapped("Do you have an idea for a feature? Suggest it ");
			ImGui::SameLine();
			if (ImGui::Button("here"))
				UIHelper::ShellOpen("https://github.com/dfranx/SHADERed/issues");
			ImGui::Separator();
			ImGui::NewLine();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create new project
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(450), Settings::Instance().CalculateSize(150)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Are you sure?##main_new_proj", 0, ImGuiWindowFlags_NoResize)) {
			ImGui::TextWrapped("You will lose your unsaved progress if you create a new project. Are you sure you want to continue?");
			if (ImGui::Button("Yes")) {
				m_saveAsOldFile = m_data->Parser.GetOpenedFile();

				SaveAsProject(false, nullptr, [&]() {
					if (m_selectedTemplate == "?empty") {
						Settings::Instance().Project.FPCamera = false;
						Settings::Instance().Project.ClearColor = glm::vec4(0, 0, 0, 0);

						ResetWorkspace();
						m_data->Pipeline.New(false);

						SDL_SetWindowTitle(m_wnd, "SHADERed");
					} else {
						m_data->Parser.SetTemplate(m_selectedTemplate);

						ResetWorkspace();
						m_data->Pipeline.New();
						m_data->Parser.SetTemplate(settings.General.StartUpTemplate);

						SDL_SetWindowTitle(m_wnd, ("SHADERed (" + m_selectedTemplate + ")").c_str());
					}
				});

				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Save preview
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(450), Settings::Instance().CalculateSize(250)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Save Preview##main_save_preview")) {
			ImGui::TextWrapped("Path: %s", m_previewSavePath.c_str());
			ImGui::SameLine();
			if (ImGui::Button("...##save_prev_path"))
				igfd::ImGuiFileDialog::Instance()->OpenModal("SavePreviewDlg", "Save", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".");
			if (igfd::ImGuiFileDialog::Instance()->FileDialog("SavePreviewDlg")) {
				if (igfd::ImGuiFileDialog::Instance()->IsOk)
					m_previewSavePath = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
				igfd::ImGuiFileDialog::Instance()->CloseDialog("SavePreviewDlg");
			}

			ImGui::Text("Width: ");
			ImGui::SameLine();
			ImGui::Indent(Settings::Instance().CalculateSize(110));
			ImGui::InputInt("##save_prev_sizew", &m_previewSaveSize.x);
			ImGui::Unindent(Settings::Instance().CalculateSize(110));

			ImGui::Text("Height: ");
			ImGui::SameLine();
			ImGui::Indent(Settings::Instance().CalculateSize(110));
			ImGui::InputInt("##save_prev_sizeh", &m_previewSaveSize.y);
			ImGui::Unindent(Settings::Instance().CalculateSize(110));

			ImGui::Text("Supersampling: ");
			ImGui::SameLine();
			ImGui::Indent(Settings::Instance().CalculateSize(110));
			ImGui::Combo("##save_prev_ssmp", &m_savePreviewSupersample, " 1x\0 2x\0 4x\0 8x\0");
			ImGui::Unindent(Settings::Instance().CalculateSize(110));

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Sequence")) {
				ImGui::TextWrapped("Export a sequence of images");

				/* RECORD */
				ImGui::Text("Record:");
				ImGui::SameLine();
				ImGui::Checkbox("##save_prev_keyw", &m_savePreviewSeq);

				if (!m_savePreviewSeq) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				/* DURATION */
				ImGui::Text("Duration:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##save_prev_seqdur", &m_savePreviewSeqDuration);
				ImGui::PopItemWidth();

				/* TIME DELTA */
				ImGui::Text("FPS:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragInt("##save_prev_seqfps", &m_savePreviewSeqFPS);
				ImGui::PopItemWidth();

				if (!m_savePreviewSeq) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}
			ImGui::Separator();

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Advanced")) {
				bool requiresPreviewUpdate = false;

				/* TIME */
				ImGui::Text("Time:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##save_prev_time", &m_savePreviewTime)) {
					float timeAdvance = m_savePreviewTime - SystemVariableManager::Instance().GetTime();
					SystemVariableManager::Instance().AdvanceTimer(timeAdvance);
					requiresPreviewUpdate = true;
				}
				ImGui::PopItemWidth();

				/* TIME DELTA */
				ImGui::Text("Time delta:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##save_prev_timed", &m_savePreviewTimeDelta))
					requiresPreviewUpdate = true;
				ImGui::PopItemWidth();

				/* FRAME INDEX */
				ImGui::Text("Frame index:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt("##save_prev_findex", &m_savePreviewFrameIndex))
					requiresPreviewUpdate = true;
				ImGui::PopItemWidth();

				/* WASD */
				ImGui::Text("WASD:");
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keyw", &m_savePreviewWASD[0]))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keya", &m_savePreviewWASD[1]))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keys", &m_savePreviewWASD[2]))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keyd", &m_savePreviewWASD[3]))
					requiresPreviewUpdate = true;

				/* MOUSE */
				ImGui::Text("Mouse:");
				ImGui::SameLine();
				if (ImGui::DragFloat2("##save_prev_mpos", glm::value_ptr(m_savePreviewMouse))) {
					SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
					requiresPreviewUpdate = true;
				}
				ImGui::SameLine();
				bool isLeftDown = m_savePreviewMouse.z >= 1.0f;
				if (ImGui::Checkbox("##save_prev_btnleft", &isLeftDown))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				m_savePreviewMouse.z = isLeftDown;
				bool isRightDown = m_savePreviewMouse.w >= 1.0f;
				if (ImGui::Checkbox("##save_prev_btnright", &isRightDown))
					requiresPreviewUpdate = true;
				m_savePreviewMouse.w = isRightDown;
				SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);
			
				if (requiresPreviewUpdate)
					m_data->Renderer.Render();
			}
			ImGui::Separator();

			bool rerenderPreview = false;
			glm::ivec2 rerenderSize = m_data->Renderer.GetLastRenderSize();

			if (ImGui::Button("Save")) {
				int sizeMulti = 1;
				switch (m_savePreviewSupersample) {
				case 1: sizeMulti = 2; break;
				case 2: sizeMulti = 4; break;
				case 3: sizeMulti = 8; break;
				}
				int actualSizeX = m_previewSaveSize.x * sizeMulti;
				int actualSizeY = m_previewSaveSize.y * sizeMulti;

				SystemVariableManager::Instance().SetSavingToFile(true);

				// normal render
				if (!m_savePreviewSeq) {
					if (actualSizeX > 0 && actualSizeY > 0) {
						SystemVariableManager::Instance().CopyState();

						SystemVariableManager::Instance().SetTimeDelta(m_savePreviewTimeDelta);
						SystemVariableManager::Instance().SetFrameIndex(m_savePreviewFrameIndex);
						SystemVariableManager::Instance().SetKeysWASD(m_savePreviewWASD[0], m_savePreviewWASD[1], m_savePreviewWASD[2], m_savePreviewWASD[3]);
						SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
						SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);

						m_data->Renderer.Render(actualSizeX, actualSizeY);

						SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTime);
					
						rerenderPreview = true;
					}

					unsigned char* pixels = (unsigned char*)malloc(actualSizeX * actualSizeY * 4);
					unsigned char* outPixels = nullptr;

					if (sizeMulti != 1)
						outPixels = (unsigned char*)malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4);
					else
						outPixels = pixels;

					GLuint tex = m_data->Renderer.GetTexture();
					glBindTexture(GL_TEXTURE_2D, tex);
					glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
					glBindTexture(GL_TEXTURE_2D, 0);

					// resize image
					if (sizeMulti != 1) {
						stbir_resize_uint8(pixels, actualSizeX, actualSizeY, actualSizeX * 4,
							outPixels, m_previewSaveSize.x, m_previewSaveSize.y, m_previewSaveSize.x * 4, 4);
					}

					std::string ext = m_previewSavePath.substr(m_previewSavePath.find_last_of('.') + 1);

					if (ext == "jpg" || ext == "jpeg")
						stbi_write_jpg(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels, 100);
					else if (ext == "bmp")
						stbi_write_bmp(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels);
					else if (ext == "tga")
						stbi_write_tga(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels);
					else
						stbi_write_png(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels, m_previewSaveSize.x * 4);

					if (sizeMulti != 1) free(outPixels);
					free(pixels);
				} else { // sequence render
					float seqDelta = 1.0f / m_savePreviewSeqFPS;

					if (actualSizeX > 0 && actualSizeY > 0) {
						SystemVariableManager::Instance().SetKeysWASD(m_savePreviewWASD[0], m_savePreviewWASD[1], m_savePreviewWASD[2], m_savePreviewWASD[3]);
						SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
						SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);

						float curTime = 0.0f;

						GLuint tex = m_data->Renderer.GetTexture();

						size_t lastDot = m_previewSavePath.find_last_of('.');
						std::string ext = lastDot == std::string::npos ? "png" : m_previewSavePath.substr(lastDot + 1);
						std::string filename = m_previewSavePath;

						// allow only one %??d
						bool inFormat = false;
						int lastFormatPos = -1;
						int formatCount = 0;
						for (int i = 0; i < filename.size(); i++) {
							if (filename[i] == '%') {
								inFormat = true;
								lastFormatPos = i;
								continue;
							}

							if (inFormat) {
								if (isdigit(filename[i])) {
								} else {
									if (filename[i] != '%' && ((filename[i] == 'd' && formatCount > 0) || (filename[i] != 'd'))) {
										filename.insert(lastFormatPos, 1, '%');
									}

									if (filename[i] == 'd')
										formatCount++;
									inFormat = false;
								}
							}
						}

						// no %d found? add one
						if (formatCount == 0)
							filename.insert(lastDot == std::string::npos ? filename.size() : lastDot, "%d"); // frame%d

						SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTimeDelta);
						SystemVariableManager::Instance().SetTimeDelta(seqDelta);

						stbi_write_png_compression_level = 5; // set to lowest compression level

						int tCount = std::thread::hardware_concurrency();
						tCount = tCount == 0 ? 2 : tCount;

						unsigned char** pixels = new unsigned char*[tCount];
						unsigned char** outPixels = new unsigned char*[tCount];
						int* curFrame = new int[tCount];
						bool* needsUpdate = new bool[tCount];
						std::thread** threadPool = new std::thread*[tCount];
						std::atomic<bool> isOver = false;

						for (int i = 0; i < tCount; i++) {
							curFrame[i] = 0;
							needsUpdate[i] = true;
							pixels[i] = (unsigned char*)malloc(actualSizeX * actualSizeY * 4);

							if (sizeMulti != 1)
								outPixels[i] = (unsigned char*)malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4);
							else
								outPixels[i] = nullptr;

							threadPool[i] = new std::thread([ext, filename, sizeMulti, actualSizeX, actualSizeY, &outPixels, &pixels, &needsUpdate, &curFrame, &isOver](int worker, int w, int h) {
								char prevSavePath[SHADERED_MAX_PATH];
								while (!isOver) {
									if (needsUpdate[worker])
										continue;

									// resize image
									if (sizeMulti != 1) {
										stbir_resize_uint8(pixels[worker], actualSizeX, actualSizeY, actualSizeX * 4,
											outPixels[worker], w, h, w * 4, 4);
									} else
										outPixels[worker] = pixels[worker];

									sprintf(prevSavePath, filename.c_str(), curFrame[worker]);

									if (ext == "jpg" || ext == "jpeg")
										stbi_write_jpg(prevSavePath, w, h, 4, outPixels[worker], 100);
									else if (ext == "bmp")
										stbi_write_bmp(prevSavePath, w, h, 4, outPixels[worker]);
									else if (ext == "tga")
										stbi_write_tga(prevSavePath, w, h, 4, outPixels[worker]);
									else
										stbi_write_png(prevSavePath, w, h, 4, outPixels[worker], w * 4);

									needsUpdate[worker] = true;
								}
							},
								i, m_previewSaveSize.x, m_previewSaveSize.y);
						}

						int globalFrame = 0;
						while (curTime < m_savePreviewSeqDuration) {
							int hasWork = -1;
							for (int i = 0; i < tCount; i++)
								if (needsUpdate[i]) {
									hasWork = i;
									break;
								}

							if (hasWork == -1)
								continue;

							SystemVariableManager::Instance().CopyState();
							SystemVariableManager::Instance().SetFrameIndex(m_savePreviewFrameIndex + globalFrame);

							m_data->Renderer.Render(actualSizeX, actualSizeY);

							glBindTexture(GL_TEXTURE_2D, tex);
							glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels[hasWork]);
							glBindTexture(GL_TEXTURE_2D, 0);

							SystemVariableManager::Instance().AdvanceTimer(seqDelta);

							curTime += seqDelta;
							curFrame[hasWork] = globalFrame;
							needsUpdate[hasWork] = false;
							globalFrame++;
						}
						isOver = true;

						for (int i = 0; i < tCount; i++) {
							if (threadPool[i]->joinable())
								threadPool[i]->join();
							free(pixels[i]);
							if (sizeMulti != 1)
								free(outPixels[i]);
							delete threadPool[i];
						}
						delete[] pixels;
						delete[] outPixels;
						delete[] curFrame;
						delete[] needsUpdate;
						delete[] threadPool;

						stbi_write_png_compression_level = 8; // set back to default compression level

						rerenderPreview = true;
					}
				}

				SystemVariableManager::Instance().SetSavingToFile(false);

				m_data->Renderer.Pause(m_wasPausedPrior);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
				SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTime);
				m_data->Renderer.Pause(m_wasPausedPrior);
				rerenderPreview = true;
			}
			ImGui::EndPopup();

			if (rerenderPreview)
				m_data->Renderer.Render(rerenderSize.x, rerenderSize.y);

			m_recompiledAll = false;
		}

		// Export as C++ app
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(450), Settings::Instance().CalculateSize(300)));
		if (ImGui::BeginPopupModal("Export as C++ project##main_export_as_cpp")) {
			// output file
			ImGui::TextWrapped("Output file: %s", m_expcppSavePath.c_str());
			ImGui::SameLine();
			if (ImGui::Button("...##expcpp_savepath"))
				igfd::ImGuiFileDialog::Instance()->OpenModal("ExportCPPDlg", "Save", "C++ source file (*.cpp;*.cxx){.cpp,.cxx},.*", ".");
			if (igfd::ImGuiFileDialog::Instance()->FileDialog("ExportCPPDlg")) {
				if (igfd::ImGuiFileDialog::Instance()->IsOk)
					m_expcppSavePath = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
				igfd::ImGuiFileDialog::Instance()->CloseDialog("ExportCPPDlg");
			}

			// store shaders in files
			ImGui::Text("Store shaders in memory: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_memory_shaders", &m_expcppMemoryShaders);

			// cmakelists
			ImGui::Text("Generate CMakeLists.txt: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_cmakelists", &m_expcppCmakeFiles);

			// cmake project name
			ImGui::Text("CMake project name: ");
			ImGui::SameLine();
			ImGui::InputText("##expcpp_cmake_name", &m_expcppProjectName[0], 64);

			// copy cmake modules
			ImGui::Text("Copy CMake modules: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_cmakemodules", &m_expcppCmakeModules);

			// copy stb_image
			ImGui::Text("Copy stb_image.h: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_stb_image", &m_expcppImage);

			// copy images
			ImGui::Text("Copy images: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_copy_images", &m_expcppCopyImages);

			// backend
			ImGui::Text("Backend: ");
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::RadioButton("OpenGL", &m_expcppBackend, 0);
			ImGui::SameLine();

			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::RadioButton("DirectX", &m_expcppBackend, 1);
			ImGui::PopItemFlag();
			ImGui::EndGroup();

			if (m_expcppError) ImGui::Text("An error occured. Possible cause: some files are missing.");

			// export || cancel
			if (ImGui::Button("Export")) {
				m_expcppError = !ExportCPP::Export(m_data, m_expcppSavePath, !m_expcppMemoryShaders, m_expcppCmakeFiles, m_expcppProjectName, m_expcppCmakeModules, m_expcppImage, m_expcppCopyImages);
				if (!m_expcppError)
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Changelog popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(370), Settings::Instance().CalculateSize(420)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Changelog##main_upd_changelog", 0, ImGuiWindowFlags_NoResize)) {
			UIHelper::Markdown(m_changelogText);

			ImGui::Separator();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Tips popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(470), Settings::Instance().CalculateSize(420)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal((m_tipTitle + "###main_tips").c_str(), 0)) {
			UIHelper::Markdown(m_tipText);


			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - Settings::Instance().CalculateSize(50), ImGui::GetWindowSize().y - Settings::Instance().CalculateSize(40)));
			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Incompatible plugins
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(470), 0), ImGuiCond_Appearing);
		if (ImGui::BeginPopupModal("Incompatible plugins##main_incompat_plugins")) {
			std::string text = "There's a mismatch between SHADERed's plugin API version and the following plugins' API version:\n";
			for (int i = 0; i < m_data->Plugins.GetIncompatiblePlugins().size(); i++)
				text += "  * " + m_data->Plugins.GetIncompatiblePlugins()[i] + "\n";
			text += "Either update your instance of SHADERed or update these plugins.";
			
			UIHelper::Markdown(text);

			ImGui::Separator();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// notifications
		if (m_notifs.Has())
			m_notifs.Render();

		// render ImGUI
		ImGui::Render();
	}
	void GUIManager::Render()
	{
		ImDrawData* drawData = ImGui::GetDrawData();
		if (drawData != NULL) {
			// actually render to back buffer
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			// Update and Render additional Platform Windows
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}
	}

	void GUIManager::m_splashScreenRender()
	{
		SDL_GetWindowSize(m_wnd, &m_width, &m_height);
		float wndRounding = ImGui::GetStyle().WindowRounding;
		float wndBorder = ImGui::GetStyle().WindowBorderSize;
		ImGui::GetStyle().WindowRounding = 0.0f;
		ImGui::GetStyle().WindowBorderSize = 0.0f;

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(m_width, m_height));
		if (ImGui::Begin("##splash_screen", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
			ImGui::SetCursorPos(ImVec2((m_width - 350) / 2, (m_height - 324) / 6));
			ImGui::Image((ImTextureID)m_splashScreenIcon, ImVec2(350, 324));

			ImGui::SetCursorPos(ImVec2((m_width - 350) / 2, m_height - 326));
			ImGui::Image((ImTextureID)m_splashScreenText, ImVec2(350, 324));
		}
		ImGui::End();

		ImGui::GetStyle().WindowRounding = wndRounding;
		ImGui::GetStyle().WindowBorderSize = wndBorder;

		// render ImGUI
		ImGui::Render();

		if (m_splashScreenTimer.GetElapsedTime() > 0.85f && m_splashScreenLoaded)
			m_splashScreen = false;

		// rather than moving everything on the other thread rn, load on this one but first render the splash screen (maybe TODO)
		if (m_splashScreenFrame < 5)
			m_splashScreenFrame++;
		else if (!m_splashScreenLoaded) {
			// check for updates
			if (Settings::Instance().General.CheckUpdates) {
				m_data->API.CheckForApplicationUpdates([&]() {
					m_notifs.Add(0, "A new version of SHADERed is available!", "UPDATE", [](int id, IPlugin1* pl) {
						UIHelper::ShellOpen("https://shadered.org/download.php");
					});
				});
			}

			// check for plugin updates
			if (Settings::Instance().General.CheckPluginUpdates) {
				const auto& pluginList = m_data->Plugins.GetPluginList();
				for (const auto& plugin : pluginList) {
					int version = m_data->API.GetPluginVersion(plugin);

					if (version > m_data->Plugins.GetPluginVersion(plugin)) {
						m_notifs.Add(1, "A new version of " + plugin + " plugin is available!", "UPDATE", [&](int id, IPlugin1* pl) {
							UIHelper::ShellOpen("https://shadered.org/plugin?id=" + plugin);
						});
					}
				}
			}

			// check for tips
			if (Settings::Instance().General.Tips) {
				m_data->API.FetchTips([&](int n, int i, const std::string& title, const std::string& text) {
					m_tipCount = n;
					m_tipIndex = i;
					m_tipTitle = title + " (tip " + std::to_string(m_tipIndex + 1) + "/" + std::to_string(m_tipCount) + ")";
					m_tipText = text;
					m_tipOpened = true;
				});
			}

			// check the changelog
			m_checkChangelog();

			// load plugins
			m_data->Plugins.Init(m_data, this);

			m_onlineExcludeGodot = m_data->Plugins.GetPlugin("GodotShaders") == nullptr;

			m_splashScreenLoaded = true;
			m_isIncompatPluginsOpened = !m_data->Plugins.GetIncompatiblePlugins().empty();

			if (m_isIncompatPluginsOpened) {
				const std::vector<std::string>& incompat = m_data->Plugins.GetIncompatiblePlugins();
				std::vector<std::string>& notLoaded = Settings::Instance().Plugins.NotLoaded;
				
				for (const auto& plg : incompat)
					if (std::count(notLoaded.begin(), notLoaded.end(), plg) == 0)
						notLoaded.push_back(plg);
			}
		}
	}
	void GUIManager::m_splashScreenLoad()
	{
		stbi_set_flip_vertically_on_load(0);

		// logo 
		int req_format = STBI_rgb_alpha;
		int width, height, orig_format;
		unsigned char* data = stbi_load("./data/splash_screen_logo.png", &width, &height, &orig_format, req_format);
		if (data == NULL) {
			ed::Logger::Get().Log("Failed to load splash screen icon", true);
			return;
		}
		glGenTextures(1, &m_splashScreenIcon);
		glBindTexture(GL_TEXTURE_2D, m_splashScreenIcon);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
		stbi_image_free(data);

		// check if we should use black or white text
		bool white = true;
		ImVec4 wndBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		float avgWndBg = (wndBg.x + wndBg.y + wndBg.z) / 3.0f;
		if (avgWndBg >= 0.5f)
			white = false;

		// text
		data = stbi_load(white ? "./data/splash_screen_text_white.png" : "./data/splash_screen_text_black.png", &width, &height, &orig_format, req_format);
		if (data == NULL) {
			ed::Logger::Get().Log("Failed to load splash screen icon", true);
			return;
		}
		glGenTextures(1, &m_splashScreenText);
		glBindTexture(GL_TEXTURE_2D, m_splashScreenText);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
		stbi_image_free(data);

		stbi_set_flip_vertically_on_load(1);

		m_splashScreenFrame = 0;
		m_splashScreenLoaded = false;
		m_splashScreenTimer.Restart();
	}

	void GUIManager::AddNotification(int id, const char* text, const char* btnText, std::function<void(int, IPlugin1*)> fn, IPlugin1* plugin)
	{
		m_notifs.Add(id, text, btnText, fn, plugin);
	}

	void GUIManager::StopDebugging()
	{
		CodeEditorUI* codeUI = (CodeEditorUI*)Get(ViewID::Code);
		codeUI->StopDebugging();
		m_data->Debugger.SetDebugging(false);
	}
	void GUIManager::Destroy()
	{
		std::string bookmarksFileLoc = "data/bookmarks.dat";
		if (!ed::Settings::Instance().LinuxHomeDirectory.empty())
			bookmarksFileLoc = ed::Settings::Instance().LinuxHomeDirectory + "data/bookmarks.dat";
		std::ofstream bookmarksFile(bookmarksFileLoc);
		bookmarksFile << igfd::ImGuiFileDialog::Instance()->SerializeBookmarks();
		bookmarksFile.close();

		CodeEditorUI* codeUI = ((CodeEditorUI*)Get(ViewID::Code));
		codeUI->SaveSnippets();
		codeUI->SetTrackFileChanges(false);
		codeUI->StopThreads();
	}

	int GUIManager::AreYouSure()
	{
		int buttonid = UIHelper::MessageBox_YesNoCancel(m_wnd, "Save changes to the project before quitting?");
		if (buttonid == 0) // save
			Save();
		return buttonid;
	}
	void GUIManager::m_tooltip(const std::string& text)
	{
		if (ImGui::IsItemHovered()) {
			ImGui::PopFont(); // TODO: remove this if its being used in non-toolbar places

			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();

			ImGui::PushFont(m_iconFontLarge);
		}
	}
	void GUIManager::m_renderToolbar()
	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight()));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, Settings::Instance().CalculateSize(TOOLBAR_HEIGHT)));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("##toolbar", 0, window_flags);
		ImGui::PopStyleVar(3);

		float bHeight = TOOLBAR_HEIGHT / 2 + ImGui::GetStyle().FramePadding.y * 2;

		ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - Settings::Instance().CalculateSize(bHeight) / 2);
		ImGui::Indent(Settings::Instance().CalculateSize(15));

		/*
			project (open, open directory, save, empty, new shader) ||
			objects (new texture, new cubemap, new audio, new render texture) ||
			preview (screenshot, pause, zoom in, zoom out, maximize) ||
			random (settings)
		*/
		ImGui::Columns(4);

		ImGui::PushFont(m_iconFontLarge);

		const ImVec2 labelSize = ImGui::CalcTextSize(UI_ICON_DOCUMENT_FOLDER, NULL, true);
		const float btnWidth = labelSize.x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float scaledBtnWidth = Settings::Instance().CalculateWidth(btnWidth) + ImGui::GetStyle().ItemSpacing.x * 2.0f;

		ImGui::SetColumnWidth(0, 5 * scaledBtnWidth);
		ImGui::SetColumnWidth(1, 8 * scaledBtnWidth);
		ImGui::SetColumnWidth(2, 4 * scaledBtnWidth);

		// TODO: maybe pack all these into functions such as m_open, m_createEmpty, etc... so that there are no code
		// repetitions
		if (ImGui::Button(UI_ICON_DOCUMENT_FOLDER)) { // OPEN PROJECT
			bool cont = true;
			if (m_data->Parser.IsProjectModified()) {
				int btnID = this->AreYouSure();
				if (btnID == 2)
					cont = false;
			}

			if (cont)
				igfd::ImGuiFileDialog::Instance()->OpenModal("OpenProjectDlg", "Open SHADERed project", "SHADERed project (*.sprj){.sprj},.*", ".");
		}
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		m_tooltip("Open a project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_SAVE)) // SAVE
			Save();
		m_tooltip("Save project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_FILE)) { // EMPTY PROJECT
			m_selectedTemplate = "?empty";
			m_isNewProjectPopupOpened = true;
		}
		m_tooltip("New empty project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FOLDER_OPEN)) { // OPEN DIRECTORY
			std::string prpath = m_data->Parser.GetProjectPath("");
#if defined(__APPLE__)
			system(("open " + prpath).c_str()); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
			system(("xdg-open " + prpath).c_str());
#elif defined(_WIN32)
			ShellExecuteA(NULL, "open", prpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
		}
		m_tooltip("Open project directory");
		ImGui::NextColumn();

		if (ImGui::Button(UI_ICON_PIXELS)) this->CreateNewShaderPass();
		m_tooltip("New shader pass");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FX)) this->CreateNewComputePass();
		m_tooltip("New compute pass");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_IMAGE)) this->CreateNewTexture();
		m_tooltip("New texture");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_IMAGE)) this->CreateNewImage();
		m_tooltip("New empty image");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_WAVE)) this->CreateNewAudio();
		m_tooltip("New audio");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_TRANSPARENT)) this->CreateNewRenderTexture();
		m_tooltip("New render texture");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_CUBE)) this->CreateNewCubemap();
		m_tooltip("New cubemap");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_TEXT)) this->CreateNewBuffer();
		m_tooltip("New buffer");
		ImGui::NextColumn();

		if (ImGui::Button(UI_ICON_REFRESH)) { // REBUILD PROJECT
			((CodeEditorUI*)Get(ViewID::Code))->SaveAll();
			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			for (PipelineItem*& pass : passes)
				m_data->Renderer.Recompile(pass->Name);
		}
		m_tooltip("Rebuild");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_CAMERA)) m_savePreviewPopupOpened = true; // TAKE A SCREENSHOT
		m_tooltip("Render");
		ImGui::SameLine();
		if (ImGui::Button(m_data->Renderer.IsPaused() ? UI_ICON_PLAY : UI_ICON_PAUSE))
			m_data->Renderer.Pause(!m_data->Renderer.IsPaused());
		m_tooltip("Pause preview");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_MAXIMIZE)) m_perfModeFake = !m_perfModeFake;
		m_tooltip("Performance mode");
		ImGui::NextColumn();

		if (ImGui::Button(UI_ICON_GEAR)) { // SETTINGS BUTTON
			m_optionsOpened = true;
			*m_settingsBkp = Settings::Instance();
			m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
		}
		m_tooltip("Settings");
		ImGui::NextColumn();

		ImGui::PopStyleColor();
		ImGui::PopFont();
		ImGui::Columns(1);

		ImGui::End();
	}
	void GUIManager::m_onlineSearchShaders()
	{
		m_onlineShaders.clear();
		for (int i = 0; i < m_onlineShaderThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlineShaderThumbnail[i]);
		m_onlineShaderThumbnail.clear();

		m_onlineShaders = m_data->API.SearchShaders(m_onlineQuery, m_onlinePage, "hot", "", m_onlineUsername, m_onlineExcludeGodot);



		int minSize = std::min<int>(12, m_onlineShaders.size());

		m_onlineShaderThumbnail.resize(minSize);

		size_t dataLen = 0;
		for (int i = 0; i < minSize; i++) {
			char* data = m_data->API.AllocateThumbnail(m_onlineShaders[i].ID, dataLen);
			if (data == nullptr) {
				Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlineShaders[i].ID, true);
				continue;
			}

			int width, height, nrChannels;
			unsigned char* pixelData = stbi_load_from_memory((stbi_uc*)data, dataLen, &width, &height, &nrChannels, STBI_rgb_alpha);

			if (pixelData == nullptr) {
				Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlineShaders[i].ID, true);
				free(data);
				continue;
			}

			// normal texture
			glGenTextures(1, &m_onlineShaderThumbnail[i]);
			glBindTexture(GL_TEXTURE_2D, m_onlineShaderThumbnail[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
			glBindTexture(GL_TEXTURE_2D, 0);

			free(data);
		}
	}
	void GUIManager::m_onlineSearchPlugins()
	{
		m_onlinePlugins.clear();
		for (int i = 0; i < m_onlinePluginThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlinePluginThumbnail[i]);
		m_onlinePluginThumbnail.clear();

		m_onlinePlugins = m_data->API.SearchPlugins(m_onlineQuery, m_onlinePage, "popular", m_onlineUsername);
		
		
		int minSize = std::min<int>(12, m_onlinePlugins.size());

		m_onlinePluginThumbnail.resize(minSize);

		size_t dataLen = 0;
		for (int i = 0; i < minSize; i++) {
			char* data = m_data->API.DecodeThumbnail(m_onlinePlugins[i].Thumbnail, dataLen);
			if (data == nullptr) {
				Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlinePlugins[i].ID, true);
				continue;
			}
			m_onlinePlugins[i].Thumbnail.clear(); // since they are pretty large, no need to keep them in memory

			int width, height, nrChannels;
			unsigned char* pixelData = stbi_load_from_memory((stbi_uc*)data, dataLen, &width, &height, &nrChannels, STBI_rgb_alpha);

			if (pixelData == nullptr) {
				Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlinePlugins[i].ID, true);
				free(data);
				continue;
			}

			// normal texture
			glGenTextures(1, &m_onlinePluginThumbnail[i]);
			glBindTexture(GL_TEXTURE_2D, m_onlinePluginThumbnail[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
			glBindTexture(GL_TEXTURE_2D, 0);

			free(data);
		}
	}
	void GUIManager::m_onlineSearchThemes()
	{
		m_onlineThemes.clear();
		for (int i = 0; i < m_onlineThemeThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlineThemeThumbnail[i]);
		m_onlineThemeThumbnail.clear();

		m_onlineThemes = m_data->API.SearchThemes(m_onlineQuery, m_onlinePage, "popular", m_onlineUsername);

		int minSize = std::min<int>(12, m_onlineThemes.size());

		m_onlineThemeThumbnail.resize(minSize);

		size_t dataLen = 0;
		for (int i = 0; i < minSize; i++) {
			char* data = m_data->API.DecodeThumbnail(m_onlineThemes[i].Thumbnail, dataLen);
			if (data == nullptr) {
				Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlineThemes[i].ID, true);
				continue;
			}
			m_onlineThemes[i].Thumbnail.clear(); // since they are pretty large, no need to keep them in memory

			int width, height, nrChannels;
			unsigned char* pixelData = stbi_load_from_memory((stbi_uc*)data, dataLen, &width, &height, &nrChannels, STBI_rgb_alpha);

			if (pixelData == nullptr) {
				Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlineThemes[i].ID, true);
				free(data);
				continue;
			}

			// normal texture
			glGenTextures(1, &m_onlineThemeThumbnail[i]);
			glBindTexture(GL_TEXTURE_2D, m_onlineThemeThumbnail[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
			glBindTexture(GL_TEXTURE_2D, 0);

			free(data);
		}
	}
	void GUIManager::m_renderOptions()
	{
		OptionsUI* options = (OptionsUI*)m_options;
		static const char* optGroups[8] = {
			"General",
			"Editor",
			"Code snippets",
			"Debugger",
			"Shortcuts",
			"Preview",
			"Plugins",
			"Project"
		};

		float height = abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - ImGui::GetStyle().WindowPadding.y * 2) / ImGui::GetTextLineHeightWithSpacing() - 1;

		ImGui::Columns(2);

		// TODO: this is only a temprorary fix for non-resizable columns
		static bool isColumnWidthSet = false;
		if (!isColumnWidthSet) {
			ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(100) + ImGui::GetStyle().WindowPadding.x * 2);
			isColumnWidthSet = true;
		}

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-1);
		if (ImGui::ListBox("##optiongroups", &m_optGroup, optGroups, HARRAYSIZE(optGroups), height))
			options->SetGroup((OptionsUI::Page)m_optGroup);
		ImGui::PopStyleColor();

		ImGui::NextColumn();

		options->Update(0.0f);

		ImGui::Columns();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(160));
		if (ImGui::Button("OK", ImVec2(Settings::Instance().CalculateSize(70), 0))) {
			Settings::Instance().Save();
			KeyboardShortcuts::Instance().Save();

			CodeEditorUI* code = ((CodeEditorUI*)Get(ViewID::Code));

			code->ApplySettings();

			if (Settings::Instance().TempScale != Settings::Instance().DPIScale) {
				((ed::OptionsUI*)m_options)->ApplyTheme();
				Settings::Instance().DPIScale = Settings::Instance().TempScale;
				ImGui::GetStyle().ScaleAllSizes(Settings::Instance().DPIScale);
				m_fontNeedsUpdate = true;
			}

			m_optionsOpened = false;
		}

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(80));
		if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
			Settings::Instance() = *m_settingsBkp;
			KeyboardShortcuts::Instance().SetMap(m_shortcutsBkp);
			((OptionsUI*)m_options)->ApplyTheme();
			m_optionsOpened = false;
		}
	}

	UIView* GUIManager::Get(ViewID view)
	{
		if (view == ViewID::Options)
			return m_options;
		else if (view == ViewID::ObjectPreview)
			return m_objectPrev;
		else if (view >= ViewID::DebugWatch && view <= ViewID::DebugImmediate)
			return m_debugViews[(int)view - (int)ViewID::DebugWatch];

		return m_views[(int)view];
	}
	void GUIManager::ResetWorkspace()
	{
		m_data->Renderer.FlushCache();
		((CodeEditorUI*)Get(ViewID::Code))->CloseAll();
		((PinnedUI*)Get(ViewID::Pinned))->CloseAll();
		((PreviewUI*)Get(ViewID::Preview))->Reset();
		((PropertyUI*)Get(ViewID::Properties))->Open(nullptr);
		((PipelineUI*)Get(ViewID::Pipeline))->Reset();
		((ObjectPreviewUI*)Get(ViewID::ObjectPreview))->CloseAll();
		CameraSnapshots::Clear();
	}
	void GUIManager::SaveSettings()
	{
		std::ofstream data("data/gui.dat");

		for (auto& view : m_views)
			data.put(view->Visible);
		for (auto& dview : m_debugViews)
			data.put(dview->Visible);

		data.close();
	}
	void GUIManager::LoadSettings()
	{
		std::ifstream data("data/gui.dat");

		if (data.is_open()) {
			for (auto& view : m_views)
				view->Visible = data.get();
			for (auto& dview : m_debugViews)
				dview->Visible = data.get();

			data.close();
		}

		Get(ViewID::Code)->Visible = false;

		((OptionsUI*)m_options)->ApplyTheme();
	}
	void GUIManager::m_imguiHandleEvent(const SDL_Event& e)
	{
		ImGui_ImplSDL2_ProcessEvent(&e);
	}
	ShaderVariable::ValueType getTypeFromSPV(SPIRVParser::ValueType valType)
	{
		switch (valType) {
		case SPIRVParser::ValueType::Bool:
			return ShaderVariable::ValueType::Boolean1;
		case SPIRVParser::ValueType::Int:
			return ShaderVariable::ValueType::Integer1;
		case SPIRVParser::ValueType::Float:
			return ShaderVariable::ValueType::Float1;
		default:
			return ShaderVariable::ValueType::Count;
		}
		return ShaderVariable::ValueType::Count;
	}
	ShaderVariable::ValueType formVectorType(ShaderVariable::ValueType valType, int compCount)
	{
		if (valType == ShaderVariable::ValueType::Boolean1) {
			if (compCount == 2) return ShaderVariable::ValueType::Boolean2;
			if (compCount == 3) return ShaderVariable::ValueType::Boolean3;
			if (compCount == 4) return ShaderVariable::ValueType::Boolean4;
		}
		else if (valType == ShaderVariable::ValueType::Integer1) {
			if (compCount == 2) return ShaderVariable::ValueType::Integer2;
			if (compCount == 3) return ShaderVariable::ValueType::Integer3;
			if (compCount == 4) return ShaderVariable::ValueType::Integer4;
		}
		else if (valType == ShaderVariable::ValueType::Float1) {
			if (compCount == 2) return ShaderVariable::ValueType::Float2;
			if (compCount == 3) return ShaderVariable::ValueType::Float3;
			if (compCount == 4) return ShaderVariable::ValueType::Float4;
		}

		return ShaderVariable::ValueType::Count;
	}
	ShaderVariable::ValueType formMatrixType(ShaderVariable::ValueType valType, int compCount)
	{
		if (compCount == 2) return ShaderVariable::ValueType::Float2x2;
		if (compCount == 3) return ShaderVariable::ValueType::Float3x3;
		if (compCount == 4) return ShaderVariable::ValueType::Float4x4;

		return ShaderVariable::ValueType::Count;
	}
	void GUIManager::m_autoUniforms(ShaderVariableContainer& varManager, SPIRVParser& spv, std::vector<std::string>& uniformList)
	{
		PinnedUI* pinUI = ((PinnedUI*)Get(ViewID::Pinned));
		std::vector<ShaderVariable*> vars = varManager.GetVariables();

		// add variables
		for (const auto& unif : spv.Uniforms) {
			bool exists = false;
			for (ShaderVariable* var : vars)
				if (strcmp(var->Name, unif.Name.c_str()) == 0) {
					exists = true;
					break;
				}

			uniformList.push_back(unif.Name);

			// add it 
			if (!exists) {
				// type
				ShaderVariable::ValueType valType = getTypeFromSPV(unif.Type);
				if (valType == ShaderVariable::ValueType::Count) {
					if (unif.Type == SPIRVParser::ValueType::Vector)
						valType = formVectorType(getTypeFromSPV(unif.BaseType), unif.TypeComponentCount);
					else if (unif.Type == SPIRVParser::ValueType::Matrix)
						valType = formMatrixType(getTypeFromSPV(unif.BaseType), unif.TypeComponentCount);
				}

				if (valType == ShaderVariable::ValueType::Count) {
					std::queue<std::string> curName;
					std::queue<SPIRVParser::Variable> curType;

					curType.push(unif);
					curName.push(unif.Name);

					while (!curType.empty()) {
						SPIRVParser::Variable type = curType.front();
						std::string name = curName.front();

						curType.pop();
						curName.pop();

						if (type.Type != SPIRVParser::ValueType::Struct) {
							for (ShaderVariable* var : vars)
								if (strcmp(var->Name, name.c_str()) == 0) {
									exists = true;
									break;
								}

							uniformList.push_back(name);

							if (!exists) {
								// add variable
								valType = getTypeFromSPV(type.Type);
								if (valType == ShaderVariable::ValueType::Count) {
									if (type.Type == SPIRVParser::ValueType::Vector)
										valType = formVectorType(getTypeFromSPV(type.BaseType), type.TypeComponentCount);
									else if (type.Type == SPIRVParser::ValueType::Matrix)
										valType = formMatrixType(getTypeFromSPV(type.BaseType), type.TypeComponentCount);
								}

								if (valType != ShaderVariable::ValueType::Count) {
									ShaderVariable newVariable = ShaderVariable(valType, name.c_str(), SystemShaderVariable::None);
									ShaderVariable* ptr = varManager.AddCopy(newVariable);
									if (Settings::Instance().General.AutoUniformsPin)
										pinUI->Add(ptr);
								}
							}
						} else {
							// branch
							if (spv.UserTypes.count(type.TypeName) > 0) {
								const std::vector<SPIRVParser::Variable>& mems = spv.UserTypes[type.TypeName];
								for (int m = 0; m < mems.size(); m++) {
									std::string memName = std::string(name.c_str()) + "." + mems[m].Name; // hack for \0
									curType.push(mems[m]);
									curName.push(memName);
								}
							}
						}
					}
				} else {
					// usage
					SystemShaderVariable usage = SystemShaderVariable::None;
					if (Settings::Instance().General.AutoUniformsFunction)
						usage = SystemVariableManager::GetTypeFromName(unif.Name);

					// add and pin
					if (valType != ShaderVariable::ValueType::Count) {
						ShaderVariable newVariable = ShaderVariable(valType, unif.Name.c_str(), usage);
						ShaderVariable* ptr = varManager.AddCopy(newVariable);
						if (Settings::Instance().General.AutoUniformsPin && usage == SystemShaderVariable::None)
							pinUI->Add(ptr);
					}
				}
			}
		}
	}
	void GUIManager::m_deleteUnusedUniforms(ShaderVariableContainer& varManager, const std::vector<std::string>& spv)
	{
		PinnedUI* pinUI = ((PinnedUI*)Get(ViewID::Pinned));
		std::vector<ShaderVariable*> vars = varManager.GetVariables();

		for (ShaderVariable* var : vars) {
			bool exists = false;
			for (const auto& unif : spv)
				if (strcmp(var->Name, unif.c_str()) == 0) {
					exists = true;
					break;
				}

			if (!exists) {
				pinUI->Remove(var->Name);
				varManager.Remove(var->Name);
			}
		}
	}
	bool GUIManager::Save()
	{
		if (m_data->Parser.GetOpenedFile() == "") {
			if (!((CodeEditorUI*)Get(ViewID::Code))->RequestedProjectSave) {
				SaveAsProject(true);
				return true;
			}
			return false;
		}

		m_data->Parser.Save();

		((CodeEditorUI*)Get(ViewID::Code))->SaveAll();

		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
		for (PipelineItem*& pass : passes)
			m_data->Renderer.Recompile(pass->Name);

		m_recompiledAll = true; 

		return true;
	}
	void GUIManager::SaveAsProject(bool restoreCached, std::function<void(bool)> handle, std::function<void()> preHandle)
	{
		m_saveAsRestoreCache = restoreCached;
		m_saveAsHandle = handle;
		m_saveAsPreHandle = preHandle;
		igfd::ImGuiFileDialog::Instance()->OpenModal("SaveProjectDlg", "Save project", "SHADERed project (*.sprj){.sprj},.*", ".");
	}
	void GUIManager::Open(const std::string& file)
	{
		StopDebugging();

		glm::ivec2 curSize = m_data->Renderer.GetLastRenderSize();

		this->ResetWorkspace();
		m_data->Renderer.Pause(false); // unpause

		m_data->Parser.Open(file);


		// add to recents
		std::string fileToBeOpened = file;
		for (int i = 0; i < m_recentProjects.size(); i++) {
			if (m_recentProjects[i] == fileToBeOpened) {
				m_recentProjects.erase(m_recentProjects.begin() + i);
				break;
			}
		}
		m_recentProjects.insert(m_recentProjects.begin(), fileToBeOpened);
		if (m_recentProjects.size() > 9)
			m_recentProjects.pop_back();


		std::string projName = m_data->Parser.GetOpenedFile();
		projName = projName.substr(projName.find_last_of("/\\") + 1);

		m_data->Renderer.Pause(Settings::Instance().Preview.PausedOnStartup);
		if (m_data->Renderer.IsPaused())
			m_data->Renderer.Render(curSize.x, curSize.y);

		SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());
	}

	void GUIManager::CreateNewShaderPass()
	{
		m_createUI->SetType(PipelineItem::ItemType::ShaderPass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewComputePass()
	{
		m_createUI->SetType(PipelineItem::ItemType::ComputePass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewAudioPass()
	{
		m_createUI->SetType(PipelineItem::ItemType::AudioPass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewTexture()
	{
		igfd::ImGuiFileDialog::Instance()->OpenModal("CreateTextureDlg", "Select texture(s)", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*", ".", 0);
	}
	void GUIManager::CreateNewAudio()
	{
		igfd::ImGuiFileDialog::Instance()->OpenModal("CreateAudioDlg", "Select audio file", "Audio file (*.wav;*.flac;*.ogg;*.midi){.wav,.flac,.ogg,.midi},.*", ".", 0);
	}

	void GUIManager::m_setupShortcuts()
	{
		// PROJECT
		KeyboardShortcuts::Instance().SetCallback("Project.Rebuild", [=]() {
			((CodeEditorUI*)Get(ViewID::Code))->SaveAll();

			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			for (PipelineItem*& pass : passes)
				m_data->Renderer.Recompile(pass->Name);
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Save", [=]() {
			Save();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.SaveAs", [=]() {
			SaveAsProject(true);
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ToggleToolbar", [=]() {
			Settings::Instance().General.Toolbar ^= 1;
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Open", [=]() {
			bool cont = true;
			if (m_data->Parser.IsProjectModified()) {
				int btnID = this->AreYouSure();
				if (btnID == 2)
					cont = false;
			}

			if (cont)
				igfd::ImGuiFileDialog::Instance()->OpenModal("OpenProjectDlg", "Open SHADERed project", "SHADERed project (*.sprj){.sprj},.*", ".");
		});
		KeyboardShortcuts::Instance().SetCallback("Project.New", [=]() {
			this->ResetWorkspace();
			m_data->Pipeline.New();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewRenderTexture", [=]() {
			CreateNewRenderTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewBuffer", [=]() {
			CreateNewBuffer();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewImage", [=]() {
			CreateNewImage();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewImage3D", [=]() {
			CreateNewImage3D();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewKeyboardTexture", [=]() {
			CreateKeyboardTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewCubeMap", [=]() {
			CreateNewCubemap();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewTexture", [=]() {
			CreateNewTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewAudio", [=]() {
			CreateNewAudio();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewShaderPass", [=]() {
			CreateNewShaderPass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewComputePass", [=]() {
			CreateNewComputePass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewAudioPass", [=]() {
			CreateNewAudioPass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.CameraSnapshot", [=]() {
			CreateNewCameraSnapshot();
		});

		// PREVIEW
		KeyboardShortcuts::Instance().SetCallback("Preview.ToggleStatusbar", [=]() {
			Settings::Instance().Preview.StatusBar = !Settings::Instance().Preview.StatusBar;
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.SaveImage", [=]() {
			m_savePreviewPopupOpened = true;
		});

		// WORKSPACE
		KeyboardShortcuts::Instance().SetCallback("Workspace.PerformanceMode", [=]() {
			m_performanceMode = !m_performanceMode;
			m_perfModeFake = m_performanceMode;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideOutput", [=]() {
			Get(ViewID::Output)->Visible = !Get(ViewID::Output)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideEditor", [=]() {
			Get(ViewID::Code)->Visible = !Get(ViewID::Code)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePreview", [=]() {
			Get(ViewID::Preview)->Visible = !Get(ViewID::Preview)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePipeline", [=]() {
			Get(ViewID::Pipeline)->Visible = !Get(ViewID::Pipeline)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePinned", [=]() {
			Get(ViewID::Pinned)->Visible = !Get(ViewID::Pinned)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideProperties", [=]() {
			Get(ViewID::Properties)->Visible = !Get(ViewID::Properties)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.Help", [=]() {
			UIHelper::ShellOpen("https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md");
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.Options", [=]() {
			m_optionsOpened = true;
			*m_settingsBkp = Settings::Instance();
			m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ChangeThemeUp", [=]() {
			std::vector<std::string> themes = ((OptionsUI*)m_options)->GetThemeList();

			std::string& theme = Settings::Instance().Theme;

			for (int i = 0; i < themes.size(); i++) {
				if (theme == themes[i]) {
					if (i != 0)
						theme = themes[i - 1];
					else
						theme = themes[themes.size() - 1];
					break;
				}
			}

			((OptionsUI*)m_options)->ApplyTheme();
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ChangeThemeDown", [=]() {
			std::vector<std::string> themes = ((OptionsUI*)m_options)->GetThemeList();

			std::string& theme = Settings::Instance().Theme;

			for (int i = 0; i < themes.size(); i++) {
				if (theme == themes[i]) {
					if (i != themes.size() - 1)
						theme = themes[i + 1];
					else
						theme = themes[0];
					break;
				}
			}

			((OptionsUI*)m_options)->ApplyTheme();
		});

		KeyboardShortcuts::Instance().SetCallback("Window.Fullscreen", [=]() {
			Uint32 wndFlags = SDL_GetWindowFlags(m_wnd);
			bool isFullscreen = wndFlags & SDL_WINDOW_FULLSCREEN_DESKTOP;
			SDL_SetWindowFullscreen(m_wnd, (!isFullscreen) * SDL_WINDOW_FULLSCREEN_DESKTOP);
		});
	}
	void GUIManager::m_checkChangelog()
	{
		std::string currentVersionPath = "info.dat";
		if (!ed::Settings().Instance().LinuxHomeDirectory.empty())
			currentVersionPath = ed::Settings().Instance().LinuxHomeDirectory + currentVersionPath;

		std::ifstream verReader(currentVersionPath);
		int curVer = 0;
		if (verReader.is_open()) {
			verReader >> curVer;
			verReader.close();
		}

		if (curVer < WebAPI::InternalVersion) {
			m_data->API.FetchChangelog([&](const std::string& str) -> void {
				m_isChangelogOpened = true;
				m_changelogText = str;
			});
		}
	}
	void GUIManager::m_loadTemplateList()
	{
		m_selectedTemplate = "";

		Logger::Get().Log("Loading template list");

		if (fs::exists("./templates/")) {
			for (const auto& entry : fs::directory_iterator("./templates/")) {
				std::string file = entry.path().filename().string();
				m_templates.push_back(file);

				if (file == Settings::Instance().General.StartUpTemplate)
					m_selectedTemplate = file;
			}
		}

		if (m_selectedTemplate.size() == 0) {
			if (m_templates.size() != 0) {
				m_selectedTemplate = m_templates[0];
				Logger::Get().Log("Default template set to " + m_selectedTemplate);
			} else {
				m_selectedTemplate = "?empty";
				Logger::Get().Log("No templates found", true);
			}
		}

		m_data->Parser.SetTemplate(m_selectedTemplate);
	}
}