#pragma once

/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework. If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/EmuAppHelper.hh>
#ifndef IG_USE_MODULE_IMAGINE
#include <imagine/gui/TableView.hh>
#include <imagine/gui/MenuItem.hh>
#include <imagine/gfx/Quads.hh>
#include <imagine/fs/FSDefs.hh>
#endif

namespace EmuEx
{

using namespace IG;

class GameBrowserView : public TableView, public EmuAppHelper
{
public:
	GameBrowserView(ViewAttachParams attach);
	~GameBrowserView();
	void place() final;
	void draw(Gfx::RendererCommands&, ViewDrawParams) const final;
	void prepareDraw() final;
	void onShow() final;
	void onHide() final;
	void onAddedToController(ViewController*, const Input::Event&) final;
	std::u16string_view name() const final { return u"游戏列表"; }

private:
	struct GameEntry
	{
		std::string path;
		TextMenuItem text;
		GameEntry(ViewAttachParams attach, std::string p, std::string_view n, GameBrowserView *browser)
			: path{p},
			  text{n, attach, [browser, path = p](const Input::Event &e) { browser->onGameClicked(path, e); }} {}
		MenuItem &menuItem() { return text; }
	};

	std::vector<GameEntry> gameList{};
	TextMenuItem selectFolderBtn;
	FS::PathString lastLoadedPath{};
	Gfx::IColQuads bgQuads;
	WRect previewRect{};
	WRect listRect{};

	void loadGameList();
	void onGameClicked(const std::string &path, const Input::Event &e);
};

}
