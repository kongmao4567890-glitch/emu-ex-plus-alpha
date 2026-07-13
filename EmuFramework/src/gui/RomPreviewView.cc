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
#include <emuframework/EmuVideo.hh>
#include <emuframework/EmuVideoLayer.hh>
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
				[&](const ItemsMessage&) -> ItemReply
				{
					return (showRunButton ? 1u : 0u) + romItems.size() + 1u;
				},
				[&](const GetItemMessage& m) -> ItemReply
				{
					auto offset = showRunButton ? 1u : 0u;
					auto romCount = romItems.size();
					if(showRunButton && m.idx == 0)
						return &runItem;
					if(m.idx == offset + romCount)
						return &backItem;
					return &romItems[m.idx - offset];
				},
			});
		}
	},
	runItem
	{
		"▶ 运行游戏", attach,
		[this](const Input::Event &)
		{
			if(!hasContent)
				return;
			app().emuWindow().addOnFrame(
				[this](FrameParams) -> bool
				{
					doLaunchGame();
					return false;
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
					stopPreview();
					if(hasContent)
					{
						app().closeSystem();
						hasContent = false;
					}
					showRunButton = false;
					dismiss();
					return false;
				});
		}
	},
	bgQuads{attach.rendererTask, {.size = 2}}
{
	scanDirectory(attach);
}

RomPreviewView::~RomPreviewView()
{
	stopPreview();
}

void RomPreviewView::onAddedToController(ViewController *vc, const Input::Event &e)
{
	TableView::onAddedToController(vc, e);
}

void RomPreviewView::startPreview()
{
	if(isRunning)
		return;
	auto &sys = system();
	if(!sys.hasContent())
		return;
	log.info("starting ROM preview");
	isRunning = true;

	// 初始化视频纹理 — 这是最关键的一步
	// applyRenderPixelFormat → videoLayer.setFormat → sys.onVideoRenderFormatChange
	// → updateVideoPixmap → video.setFormat → 创建 vidImg 纹理
	// 没有 this，runFrame → video.startFrame → vidImg.lock() 返回空 → endFrame 崩溃
	app().applyRenderPixelFormat();

	// 设置 state=ACTIVE + onStart()，让模拟核心准备运行
	// 然后立即停止音频和定时器（预览不需要声音和自动存档）
	sys.start(app());
	app().audio.stop();
	app().autosaveManager.pauseTimer();
	app().rewindManager.pauseTimer();

	// 用主线程 addOnFrame 运行预览帧
	// 不能用 systemTask.start()，因为它会从主线程移除帧事件、禁用 UI 绘制
	// runFrame({}, &video, nullptr) 与 EmuSystem::benchmark() 用法相同
	onFrameDel =
		[this](FrameParams) -> bool
		{
			auto &sys = system();
			if(!isRunning || !sys.hasContent())
				return false;
			sys.runFrame({}, &app().video, nullptr);
			return true;
		};
	app().emuWindow().addOnFrame(onFrameDel);
}

void RomPreviewView::stopPreview()
{
	if(!isRunning)
		return;
	isRunning = false;
	if(onFrameDel)
	{
		app().emuWindow().removeOnFrame(onFrameDel);
		onFrameDel = {};
	}
	// pause 会设置 state=PAUSED、停止音频、暂停定时器、调用 onStop
	system().pause(app());
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
		log.info("double-click: showing Run button for {}", romNames[idx]);
		lastClickedIdx = size_t(-1);
		lastClickTime = {};
		// 显示"运行"按钮
		if(hasContent && !showRunButton)
		{
			showRunButton = true;
			postDraw();
		}
		return;
	}

	// 单击：加载 ROM 并在预览区运行
	lastClickedIdx = idx;
	lastClickTime = now;
	pendingAction = PendingAction::Preview;
	pendingIdx = idx;

	if(!pendingFrameCallback)
	{
		pendingFrameCallback = true;
		app().emuWindow().addOnFrame(
			[this](FrameParams) -> bool
			{
				pendingFrameCallback = false;
				auto action = pendingAction;
				pendingAction = PendingAction::None;
				if(action == PendingAction::Preview)
					doLoadRomForPreview(pendingIdx);
				return false;
			});
	}
}

void RomPreviewView::doLoadRomForPreview(size_t idx)
{
	auto &app = this->app();
	auto path = romPaths[idx];
	auto name = romNames[idx];

	log.info("loading ROM for preview: {}", name);

	// 隐藏 Run 按钮（新游戏加载中）
	if(showRunButton)
	{
		showRunButton = false;
		postDraw();
	}

	// 使用框架的异步加载流程
	// createSystemWithMedia 内部：
	//   1. closeSystem() 关闭旧系统
	//   2. pushAndShowModalView(LoadProgressView) 显示加载进度
	//   3. makeDetachedThread 后台加载 ROM
	//   4. 加载成功后 popModalViews() + onSystemCreated() + onComplete
	app.createSystemWithMedia(IO{}, path, name, appContext().defaultInputEvent(), {}, app.attachParams(),
		[this](const Input::Event &)
		{
			// 加载完成（主线程）
			// LoadProgressView 已自动 pop，onSystemCreated 已调用
			hasContent = true;
			place();
			startPreview();
		});
}

void RomPreviewView::doLaunchGame()
{
	log.info("launching game full screen");
	stopPreview();

	auto &app = this->app();
	app.emuWindow().addOnFrame(
		[this](FrameParams) -> bool
		{
			auto &app = this->app();
			// showEmulation 切换到模拟视图并启动 systemTask
			// system().start(app) 会重新设置 state=ACTIVE + onStart + 启动音频
			// systemTask.start(emuWindow) 会启动模拟线程
			app.showEmulation();
			// 从视图栈移除 RomPreviewView（延迟到下一帧避免回调中 UAF）
			app.emuWindow().addOnFrame(
				[this](FrameParams) -> bool
				{
					dismiss();
					return false;
				});
			return false;
		});
}

void RomPreviewView::place()
{
	auto fullRect = viewRect();
	auto previewHeight = fullRect.ySize() * 40 / 100;
	auto previewRect = WindowRect{{fullRect.x, fullRect.y}, {fullRect.x2, fullRect.y + previewHeight}};
	auto bottomRect = WindowRect{{fullRect.x, fullRect.y + previewHeight}, {fullRect.x2, fullRect.y2}};

	// TableView 只使用下方区域
	setViewRect(bottomRect, bottomRect);
	TableView::place();

	// 定位视频到预览区
	auto &app = this->app();
	auto &sys = system();
	if(hasContent && sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.place({}, previewRect, nullptr, sys);
	}

	bgQuads.write(0, {.bounds = previewRect.as<int16_t>()});
	bgQuads.write(1, {.bounds = bottomRect.as<int16_t>()});
}

void RomPreviewView::draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams) const
{
	auto &app = this->app();
	auto &sys = system();
	using namespace Gfx;
	auto &basicEffect = cmds.basicEffect();

	// 预览区背景（黑色）
	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, 1.});
	cmds.drawQuad(bgQuads, 0);

	// 渲染游戏画面到预览区
	if(hasContent && sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.draw(cmds);
	}

	// 列表区背景（半透明黑色）
	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, .7});
	cmds.drawQuad(bgQuads, 1);

	// 绘制游戏列表
	TableView::draw(cmds, {});
}

}
