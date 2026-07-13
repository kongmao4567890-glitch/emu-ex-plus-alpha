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
#include <imagine/gfx/Quads.hh>
#endif

namespace EmuEx
{

using namespace IG;

// 游戏列表视图：上方40%预览窗口，下方60%游戏列表
// 单击游戏→同步加载ROM预览（不改变视图栈）
// 双击游戏→正式进入游戏运行
// 所有视图栈操作均通过addOnFrame延迟到下一帧执行，避免输入处理器中的UAF
// 所有lambda仅捕获this（8字节），适配DelegateFunc的16字节限制
class RomPreviewView: public TableView, public EmuAppHelper
{
public:
	RomPreviewView(ViewAttachParams, const Input::Event &e);
	~RomPreviewView();

	void place() final;
	void draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams p = {}) const final;
	void onAddedToController(ViewController *vc, const Input::Event &e) final;

private:
	enum class PendingAction { NoAction, Preview, Launch };

	TextMenuItem playItem;
	TextMenuItem backItem;
	std::vector<TextMenuItem> romItems;
	std::vector<std::string> romPaths;
	std::vector<std::string> romNames;
	Gfx::IQuads bgQuads;
	OnFrameDelegate onFrameDel{};
	bool isRunning{};
	bool hasContent{};
	// 双击检测
	size_t lastClickedIdx{size_t(-1)};
	SteadyClockTimePoint lastClickTime{};
	// 帧回调延迟执行
	PendingAction pendingAction{PendingAction::NoAction};
	size_t pendingIdx{};
	bool pendingFrameCallback{};

	void startPreview();
	void stopPreview();
	void scanDirectory(ViewAttachParams attach);
	void loadRom(size_t idx, const Input::Event &e);
	void doLoadRomPreview(size_t idx);
	void doLaunchGame(size_t idx);
};

}