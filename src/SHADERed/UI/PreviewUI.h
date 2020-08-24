#pragma once
#include <SHADERed/Objects/GizmoObject.h>
#include <SHADERed/UI/Tools/Magnifier.h>
#include <SHADERed/UI/UIView.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace ed {
	class PreviewUI : public UIView {
	public:
		PreviewUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_pickMode(0)
				, m_fpsDelta(0.0f)
				, m_fpsUpdateTime(0.0f)
				, m_pos1(0, 0, 0)
				, m_pos2(0, 0, 0)
				, m_overlayFBO(0)
				, m_overlayColor(0)
				, m_overlayDepth(0)
				, m_lastSize(-1, -1)
		{
			m_setupShortcuts();
			m_setupBoundingBox();
			m_fpsLimit = m_elapsedTime = 0;
			m_hasFocus = false;
			m_startWrap = false;
			m_mouseHovers = false;
			m_lastButtonUpdate = false;
			m_mouseVisible = true;
			m_mouseLock = false;
			m_fullWindowFocus = true;
			m_pauseTime = false;
		}
		~PreviewUI()
		{
			glDeleteBuffers(1, &m_boxVBO);
			glDeleteVertexArrays(1, &m_boxVAO);
			glDeleteShader(m_boxShader);
		}

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Duplicate();
		void Pick(PipelineItem* item, bool add = false);
		inline bool IsPicked(PipelineItem* item) { return std::count(m_picks.begin(), m_picks.end(), item) > 0; }

		inline glm::vec2 GetUIRectSize() { return glm::vec2(m_imgSize.x, m_imgSize.y); }
		inline glm::vec2 GetUIRectPosition() { return glm::vec2(m_imgPosition.x, m_imgPosition.y); }

		inline void Reset()
		{
			m_zoom.Reset();
			Pick(nullptr);
		}

	private:
		void m_setupShortcuts();

		void m_renderStatusbar(float width, float height);

		void m_setupBoundingBox();
		void m_buildBoundingBox();
		void m_renderBoundingBox();

		// zoom info
		Magnifier m_zoom;

		// mouse position
		ImVec2 m_mouseContact;
		bool m_startWrap;
		glm::vec2 m_mousePos, m_lastButton;
		bool m_lastButtonUpdate;

		ImVec2 m_imgSize, m_imgPosition;

		glm::vec3 m_tempTrans, m_tempScale, m_tempRota,
			m_prevTrans, m_prevScale, m_prevRota;
		GizmoObject m_gizmo;

		glm::vec3 m_pos1, m_pos2;

		eng::Timer m_fpsTimer;
		float m_fpsDelta;
		float m_fpsUpdateTime; // check if 0.5s passed then update the fps widget

		GLuint m_overlayFBO, m_overlayColor, m_overlayDepth;
		glm::ivec2 m_lastSize, m_zoomLastSize;
		bool m_hasFocus;
		bool m_mouseHovers;

		float m_elapsedTime;
		float m_fpsLimit;

		std::vector<PipelineItem*> m_picks;
		int m_pickMode; // 0 = position, 1 = scale, 2 = rotation

		// bounding box
		GLuint m_boxShader, m_boxVAO, m_boxVBO;
		glm::vec4 m_boxColor;

		// uniform locations
		GLuint m_uMatWVPLoc, m_uColorLoc;

		bool m_pauseTime;
		bool m_mouseVisible;
		bool m_mouseLock;
		bool m_fullWindowFocus;
	};
}