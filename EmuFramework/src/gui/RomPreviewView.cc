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

#include <emuframework/RomPreviewView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuSystem.hh>
#include <emuframework/FilePicker.hh>
import imagine;

namespace EmuEx
{

using namespace IG;

[[maybe_unused]] constexpr SystemLogger log{"RomPreviewView"};

static constexpr auto doubleClickTime = std::chrono::milliseconds(400);

RomPreviewView::RomPreviewView(ViewAttachParams attach, [[maybe_unused]] const Input::Event &e):
	TableView
	{
		"游戏列表",
		attach,
		[this](ItemMessage msg) -> ItemReply
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply { return 1u + romItems.size(); },
				[&](const GetItemMessage& m) -> ItemReply
				{
					auto romCount = romItems.size();
					if(m.idx == romCount)
						return &backItem;
					return &romItems[m.idx];
				},
			});
		}
	},
	backItem
	{
		"返回", attach,
		[this](const Input::Event &)
		{
			app().emuWindow().addOnFrame(
				[this](FrameParams) -> bool
				{
					// 如果有游戏在运行，先关闭
					if(system().hasContent())
						app().closeSystem();
					dismiss();
					return false;
				});
		}
	}
{
	scanDirectory(attach);
}

RomPreviewView::~RomPreviewView()
{
	// 如果视图销毁时游戏还在运行，关闭它
	if(system().hasContent())
		app().closeSystem();
}

void RomPreviewView::onAddedToController(ViewController *vc, const Input::Event &e)
{
	TableView::onAddedToController(vc, e);
}

void RomPreviewView::scanDirectory(ViewAttachParams attach)
{
	auto &dir = app().contentSearchPath;
	if(dir.empty())
	{
		log.info("contentSearchPath is empty, no games to scan");
		return;
	}

	log.info("scanning directory: {}", dir);

	struct RomEntry
	{
		std::string path;
		std::string name;
	};

	std::vector<RomEntry> roms;
	try
	{
		appContext().forEachInDirectoryUri(dir,
			[&roms](auto &entry)
			{
				if(entry.type() == FS::file_type::directory)
					return true;
				if(!AppMeta::defaultFsFilter(entry.name()))
					return true;
				roms.push_back({std::string{entry.path()}, std::string{entry.name()}});
				return true;
			});
	}
	catch(...)
	{
		log.error("error scanning directory");
		return;
	}

	std::ranges::sort(roms,
		[](const RomEntry &a, const RomEntry &b)
		{
			return caselessLexCompare(a.name, b.name);
		});

	romItems.reserve(roms.size());
	romPaths.reserve(roms.size());
	romNames.reserve(roms.size());
	for(auto &rom : roms)
	{
		romPaths.push_back(rom.path);
		romNames.push_back(rom.name);
		auto idx = romPaths.size() - 1;
		romItems.emplace_back(
			rom.name, attach,
			[this, idx](const Input::Event &e)
			{
				loadRom(idx, e);
			}
		);
	}
	log.info("found {} ROMs in {}", roms.size(), dir);
}

void RomPreviewView::loadRom(size_t idx, [[maybe_unused]] const Input::Event &e)
{
	auto now = SteadyClock::now();

	// 双击检测：同一项在 400ms 内再次点击
	if(idx == lastClickedIdx && lastClickTime != SteadyClockTimePoint{} &&
	   now - lastClickTime < doubleClickTime)
	{
		log.info("double-click: launching game (list will close): {}", romNames[idx]);
		lastClickedIdx = size_t(-1);
		lastClickTime = {};
		pendingAction = PendingAction::DoubleClick;
		pendingIdx = idx;
	}
	else
	{
		log.info("single-click: opening game (list will stay): {}", romNames[idx]);
		lastClickedIdx = idx;
		lastClickTime = now;
		pendingAction = PendingAction::SingleClick;
		pendingIdx = idx;
	}

	// 延迟到下一帧执行，避免在输入回调中操作视图栈
	if(!pendingFrameCallback)
	{
		pendingFrameCallback = true;
		app().emuWindow().addOnFrame(
			[this](FrameParams) -> bool
			{
				pendingFrameCallback = false;
				auto action = pendingAction;
				pendingAction = PendingAction::None;
				if(action == PendingAction::None)
					return false;
				doOpenGame(pendingIdx, action == PendingAction::SingleClick);
				return false;
			});
	}
}

void RomPreviewView::doOpenGame(size_t idx, bool keepList)
{
	auto &app = this->app();
	auto path = romPaths[idx];
	auto name = romNames[idx];

	log.info("opening game: {} (keepList={})", name, keepList);

	// 完全复用框架的正常启动流程：
	// createSystemWithMedia 内部会：
	//   1. closeSystem() 关闭旧系统（含 systemTask.stop()）
	//   2. pushAndShowModalView(LoadProgressView) 显示加载进度
	//   3. makeDetachedThread 后台加载 ROM
	//   4. 加载成功后 popModalViews() + onSystemCreated() + onComplete 回调
	//
	// onComplete 中调用 launchSystem：
	//   1. 加载 autosave
	//   2. showEmulation() → showEmulationView() → startEmulation()
	//   3. system().start(app) → 设置 state=ACTIVE、onStart()、启动音频
	//   4. systemTask.start(emuWindow()) → 启动模拟线程
	//
	// 单击(keepList=true)：不 dismiss，RomPreviewView 留在视图栈中
	//   → 游戏运行时按返回键暂停 → showUI() → showMenuView() → 显示 RomPreviewView
	//
	// 双击(keepList=false)：dismiss，RomPreviewView 从视图栈移除
	//   → 游戏运行时按返回键暂停 → 显示正常的菜单视图
	app.createSystemWithMedia(IO{}, path, name, appContext().defaultInputEvent(), {}, app.attachParams(),
		[this, keepList](const Input::Event &e)
		{
			auto &app = this->app();
			app.launchSystem(e);
			if(!keepList)
			{
				// 双击：延迟 dismiss 到下一帧，避免在 onComplete 回调中操作视图栈
				app.emuWindow().addOnFrame(
					[this](FrameParams) -> bool
					{
						dismiss();
						return false;
					});
			}
		});
}

}
