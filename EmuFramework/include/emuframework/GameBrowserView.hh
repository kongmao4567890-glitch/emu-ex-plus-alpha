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
#include <memory>
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
		std::string name;
		TextMenuItem text;
		GameEntry(ViewAttachParams attach, std::string p, std::string_view n)
			: path{std::move(p)}, name{n}, text{n, attach} {}
		MenuItem &menuItem() { return text; }
	};

	std::vector<GameEntry> gameList{};
	TextMenuItem selectFolderBtn;
	TextMenuItem titleItem;
	FS::PathString lastLoadedPath{};
	Gfx::IColQuads bgQuads;
	WRect previewRect{};
	WRect listRect{};

	// Async game list loading: the directory iteration (which can be slow via
	// Android SAF) runs on a background thread so the list UI renders
	// immediately on the first frame. Entries are collected as raw strings
	// (no GL resources) and converted to GameEntry on the main thread.
	struct PendingGameList
	{
		std::vector<std::pair<std::string, std::string>> entries;
	};
	std::shared_ptr<PendingGameList> pendingGameList;
	int loadGeneration{};

	void loadGameList();
	void loadGameListAsync();
	void onGameClicked(int idx, const Input::Event &e);
};

}
