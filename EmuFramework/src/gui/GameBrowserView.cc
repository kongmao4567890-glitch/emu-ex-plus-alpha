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

#include <emuframework/GameBrowserView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/AppMeta.hh>
#include <emuframework/FilePicker.hh>
#include <imagine/gui/MenuItem.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/gfx/Mat4.hh>
#include <imagine/base/Window.hh>
#include <imagine/logger/SystemLogger.hh>
import imagine;

namespace EmuEx
{

using namespace IG;

static SystemLogger log{"GameBrowserView"};

GameBrowserView::GameBrowserView(ViewAttachParams attach):
	TableView
	{
		u"游戏列表", attach,
		[this](TableView::ItemMessage msg)
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply
				{
					return gameList.empty() ? 1 : gameList.size();
				},
				[&](const GetItemMessage& m) -> ItemReply
				{
					if(gameList.empty())
						return static_cast<MenuItem*>(&selectFolderBtn);
					return static_cast<MenuItem*>(&gameList[m.idx].text);
				},
			});
		}
	},
	selectFolderBtn
	{
		"选择游戏文件夹", attach,
		[this](const Input::Event &e)
		{
			auto picker = makeView<FilePicker>(FSPicker::Mode::DIR, NameFilterFunc{}, e);
			picker->setOnSelectPath(
				[this](FSPicker &picker, CStringView path, std::string_view, const Input::Event &)
				{
					app().contentSearchPath = path;
					loadGameList();
					picker.dismiss();
				});
			pushAndShowModal(std::move(picker), e);
		}
	},
	titleItem{"游戏列表", attach},
	bgQuads{attach.rendererTask, {.size = 1}}
{
	setOnSelectElement(
		[this](const Input::Event &e, int i, MenuItem &item)
		{
			if(gameList.empty())
			{
				item.inputEvent(e, {.parentPtr = this});
				return;
			}
			// Keep the selection highlight on the tapped item
			selected = i;
			lastSelectedIdx = i;
			postDraw();
			onGameClicked(i, e);
		});
}

GameBrowserView::~GameBrowserView()
{
	app().stopPreviewEmulation();
}

void GameBrowserView::loadGameList()
{
	auto &searchPath = app().contentSearchPath;
	if(searchPath.empty())
	{
		gameList.clear();
		postDraw();
		return;
	}
	// Use cached list if path matches
	if(!app().cachedGameList.empty() && app().cachedGameListPath == searchPath)
	{
		gameList.clear();
		auto ap = attachParams();
		for(auto &[p, n] : app().cachedGameList)
			gameList.emplace_back(ap, p, n);
		resetItemSource(
			[this](TableView::ItemMessage msg)
			{
				return msg.visit(overloaded
				{
					[&](const ItemsMessage&) -> ItemReply
					{
						return gameList.empty() ? 1 : gameList.size();
					},
					[&](const GetItemMessage& m) -> ItemReply
					{
						if(gameList.empty())
							return static_cast<MenuItem*>(&selectFolderBtn);
						return static_cast<MenuItem*>(&gameList[m.idx].text);
					},
				});
			});
		postDraw();
		return;
	}
	gameList.clear();
	try
	{
		appContext().forEachInDirectoryUri(searchPath,
			[this](auto &entry)
			{
				if(entry.type() == FS::file_type::directory)
					return true;
				if(entry.name().starts_with('.'))
					return true;
				if(!AppMeta::defaultFsFilter(entry.name()) &&
				!EmuApp::hasArchiveExtension(entry.name()))
				return true;
				gameList.emplace_back(attachParams(), std::string{entry.path()}, entry.name());
				return true;
			});
	}
	catch(std::system_error &err)
	{
		log.error("can't open directory:{}", searchPath);
	}
	std::ranges::sort(gameList,
		[](const GameEntry &a, const GameEntry &b)
		{
			return caselessLexCompare(a.path, b.path);
		});
	// Cache the list for next time
	app().cachedGameList.clear();
	app().cachedGameList.reserve(gameList.size());
	for(auto &e : gameList)
		app().cachedGameList.emplace_back(e.path, e.name);
	app().cachedGameListPath = searchPath;
	resetItemSource(
		[this](TableView::ItemMessage msg)
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply
				{
					return gameList.empty() ? 1 : gameList.size();
				},
				[&](const GetItemMessage& m) -> ItemReply
				{
					if(gameList.empty())
						return static_cast<MenuItem*>(&selectFolderBtn);
					return static_cast<MenuItem*>(&gameList[m.idx].text);
				},
			});
		});
	postDraw();
}

void GameBrowserView::onGameClicked(int idx, const Input::Event &e)
{
	if(idx < 0 || idx >= (int)gameList.size())
		return;
	auto &entry = gameList[idx];
	auto path = std::string{entry.path};
	if(app().system().hasContent() &&
		path == std::string_view{lastLoadedPath})
	{
		app().enterGameFromPreview(e);
		return;
	}
	auto name = std::string{FS::basename(path)};
	lastLoadedPath = path;
	app().stopPreviewEmulation();
	app().createSystemWithMedia({}, path, name, e, {}, attachParams(),
		[this](const Input::Event &)
		{
			app().startPreviewEmulation();
		});
}

void GameBrowserView::place()
{
	auto fullRect = viewRect();
	int totalH = fullRect.ySize();
	int previewH = totalH * 40 / 100;
	int listY = fullRect.y + previewH;
	previewRect = WRect{{fullRect.x, fullRect.y}, {fullRect.x2, listY}};

	titleItem.place();
	auto titleH = titleItem.ySize();

	auto tableRect = WRect{{fullRect.x, listY + titleH}, {fullRect.x2, fullRect.y2}};
	listRect = WRect{{fullRect.x, listY}, {fullRect.x2, fullRect.y2}};

	setViewRect(tableRect, tableRect);
	app().viewController().setPreviewDisplayRect(previewRect);

	auto bgColor = Gfx::PackedColor::format.build(0.08, 0.08, 0.08, 1.0);
	bgQuads.write(0, {.bounds = listRect.as<int16_t>(), .color = bgColor});

	TableView::place();
}

void GameBrowserView::prepareDraw()
{
	TableView::prepareDraw();
	titleItem.prepareDraw();
}

void GameBrowserView::draw(Gfx::RendererCommands &cmds, ViewDrawParams params) const
{
	using namespace Gfx;
	if(bgQuads)
	{
		cmds.set(BlendMode::OFF);
		cmds.basicEffect().disableTexture(cmds);
		cmds.basicEffect().setModelView(cmds, Mat4::ident());
		cmds.setColor(ColorName::WHITE);
		cmds.setVertexArray(bgQuads);
		cmds.setVertexBuffer(bgQuads);
		cmds.drawQuads(0, 1);
	}
	auto titleH = titleItem.ySize();
	auto titleRect = listRect;
	titleRect.y2 = titleRect.y + titleH;
	titleItem.draw(cmds, {.rect = titleRect, .align = {Origin::center, Origin::center}});
	TableView::draw(cmds, params);
}

void GameBrowserView::onShow()
{
	TableView::onShow();
	if(gameList.empty())
		loadGameList();
	else if(lastSelectedIdx >= 0)
	{
		selected = lastSelectedIdx;
		postDraw();
	}
}

void GameBrowserView::onHide()
{
	TableView::onHide();
}

void GameBrowserView::onAddedToController(ViewController *, const Input::Event &e)
{
	TableView::onAddedToController(nullptr, e);
}

}
