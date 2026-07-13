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
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/defs.hh>
#include <emuframework/EmuAppHelper.hh>
#ifndef IG_USE_MODULE_IMAGINE
#include <imagine/gui/TableView.hh>
#include <imagine/gui/MenuItem.hh>
#endif

namespace EmuEx
{

using namespace IG;

// 游戏列表视图
// 单击游戏 = 正常打开游戏，游戏列表保留在视图栈中（按返回键可回到列表）
// 双击游戏 = 正常打开游戏，游戏列表消失
// 完全复用框架的 createSystemWithMedia + launchSystem 流程
// 不自己实现 runFrame / systemTask / 预览渲染
class RomPreviewView: public TableView, public EmuAppHelper
{
public:
	RomPreviewView(ViewAttachParams, const Input::Event &e);
	~RomPreviewView();

	void onAddedToController(ViewController *vc, const Input::Event &e) final;

private:
	enum class PendingAction { None, SingleClick, DoubleClick };

	TextMenuItem backItem;
	std::vector<TextMenuItem> romItems;
	std::vector<std::string> romPaths;
	std::vector<std::string> romNames;
	// 双击检测
	size_t lastClickedIdx{size_t(-1)};
	SteadyClockTimePoint lastClickTime{};
	// 帧回调延迟执行
	PendingAction pendingAction{PendingAction::None};
	size_t pendingIdx{};
	bool pendingFrameCallback{};

	void scanDirectory(ViewAttachParams attach);
	void loadRom(size_t idx, const Input::Event &e);
	void doOpenGame(size_t idx, bool keepList);
};

}
