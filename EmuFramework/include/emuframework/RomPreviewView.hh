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

// 游戏列表视图：上方40%预览区，下方60%游戏列表
// 单击游戏 → 在预览区运行游戏，列表保留
// 双击游戏 → 显示"运行"按钮
// 点"运行" → 正常全屏打开游戏
class RomPreviewView: public TableView, public EmuAppHelper
{
public:
	RomPreviewView(ViewAttachParams, const Input::Event &e);
	~RomPreviewView();

	void place() final;
	void draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams p = {}) const final;
	void onAddedToController(ViewController *vc, const Input::Event &e) final;

private:
	enum class PendingAction { None, Preview };

	TextMenuItem runItem;
	TextMenuItem backItem;
	std::vector<TextMenuItem> romItems;
	std::vector<std::string> romPaths;
	std::vector<std::string> romNames;
	Gfx::IQuads bgQuads;
	OnFrameDelegate onFrameDel{};
	bool isRunning{};
	bool hasContent{};
	bool showRunButton{};
	// 双击检测
	size_t lastClickedIdx{size_t(-1)};
	SteadyClockTimePoint lastClickTime{};
	// 帧回调延迟执行
	PendingAction pendingAction{PendingAction::None};
	size_t pendingIdx{};
	bool pendingFrameCallback{};

	void startPreview();
	void stopPreview();
	void scanDirectory(ViewAttachParams attach);
	void loadRom(size_t idx, const Input::Event &e);
	void doLoadRomForPreview(size_t idx);
	void doLaunchGame();
};

}
