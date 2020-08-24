#pragma once
#include <SHADERed/UI/Tools/CubemapPreview.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class ObjectListUI : public UIView {
	public:
		ObjectListUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_cubePrev.Init(152, 114);
		}
		~ObjectListUI() { }

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

	private:
		std::string m_saveObject;
		CubemapPreview m_cubePrev;
	};
}