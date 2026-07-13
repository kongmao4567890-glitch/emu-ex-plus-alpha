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
				[&](const ItemsMessage&) -> ItemReply { return 2u + romItems.size(); },
				[&](const GetItemMessage& m) -> ItemReply
				{
					auto romCount = romItems.size();
					if(m.idx == 0)
						return &playItem;
					if(m.idx == 1 + romCount)
						return &backItem;
					return &romItems[m.idx - 1];
				},
			});
		}
	},
	playItem
	{
		"开始游戏", attach,
		[this](const Input::Event&)
		{
			if(!hasContent)
				return;
			app().emuWindow().addOnFrame(
				[this](FrameParams) -> bool
				{
					stopPreview();
					auto &app = this->app();
					dismiss();
					app.launchSystem(app.appContext().defaultInputEvent());
					return false;
				});
		}
	},
	backItem
	{
		"返回", attach,
		[this](const Input::Event&)
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

	// 预览初始化：仅设置 state=ACTIVE 和执行子类 onStart()
	// 不调用 system().start(app)，因为它会：
	//   1. 启动音频（预览不需要声音）
	//   2. 启动 autosave/rewind 定时器（预览不需要）
	//   3. 启动 audio（可能与主线程帧回调冲突）
	{
		sys.state = EmuSystem::State::ACTIVE;
		sys.clearInputBuffers();
		sys.onStart();
	}

	// 用主线程 addOnFrame 运行预览帧
	// 绝对不能用 systemTask.start()，因为它会从主线程移除帧事件、禁用 UI 绘制
	onFrameDel =
		[this](FrameParams) -> bool
		{
			auto &sys = system();
			if(!isRunning || !sys.hasContent())
				return false;
			// EmuSystem::benchmark() 也使用 {} 作为 EmuSystemTaskContext 和 nullptr 作为 audio
			// state=ACTIVE 时 runFrame({}, ...) 安全执行
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
	// 仅调用子类 onStop()，不调用 system().pause()（它会操作音频和定时器）
	system().onStop();
	// 重置 state 以便 closeSystem 知道系统需要关闭
	system().state = EmuSystem::State::OFF;
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

	if(idx == lastClickedIdx && lastClickTime != SteadyClockTimePoint{} &&
	   now - lastClickTime < doubleClickTime)
	{
		log.info("double-click detected, launching ROM: {}", romNames[idx]);
		lastClickedIdx = size_t(-1);
		lastClickTime = {};
		pendingAction = PendingAction::Launch;
		pendingIdx = idx;
		return;
	}

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
				if(pendingAction == PendingAction::Launch)
					doLaunchGame(pendingIdx);
				else if(pendingAction == PendingAction::Preview)
					doLoadRomPreview(pendingIdx);
				pendingAction = PendingAction::NoAction;
				return false;
			});
	}
}

void RomPreviewView::doLoadRomPreview(size_t idx)
{
	stopPreview();
	auto &app = this->app();
	auto &sys = system();

	auto path = romPaths[idx];
	auto name = romNames[idx];

	log.info("loading ROM for preview: {}", name);

	// 关闭旧系统
	if(sys.hasContent())
	{
		app.closeSystem();
		hasContent = false;
	}

	// 使用 app.createSystemWithMedia 异步加载 ROM（在后台线程）
	// 之前同步调用 createWithMedia 会阻塞主线程导致 UI 卡死
	app.createSystemWithMedia(IO{}, path, name, appContext().defaultInputEvent(), {}, app.attachParams(),
		[this](const Input::Event&)
		{
			// 加载完成回调（在主线程执行）
			// createSystemWithMedia 已调用 onSystemCreated()
			// 但它还会 push LoadProgressView 和可能的其他视图
			// 我们需要 pop 到 RomPreviewView
			auto &app = this->app();

			// 关闭 createSystemWithMedia 内部可能显示的 LoadProgressView
			// pop 回到 RomPreviewView
			while(app.viewController().viewStack.size() > 1)
			{
				app.viewController().pop();
			}

			hasContent = true;
			place();
			startPreview();
		});
}

void RomPreviewView::doLaunchGame(size_t idx)
{
	stopPreview();
	auto &app = this->app();

	auto path = romPaths[idx];
	auto name = romNames[idx];

	log.info("launching ROM: {}", name);

	app.createSystemWithMedia(IO{}, path, name, appContext().defaultInputEvent(), {}, app.attachParams(),
		[this](const Input::Event &e)
		{
			auto &app = this->app();
			app.viewController().popToRoot();
			app.launchSystem(e);
		});
}

void RomPreviewView::place()
{
	auto fullRect = viewRect();
	auto previewHeight = fullRect.ySize() * 40 / 100;
	auto previewRect = WindowRect{{fullRect.x, fullRect.y}, {fullRect.x2, fullRect.y + previewHeight}};
	auto bottomRect = WindowRect{{fullRect.x, fullRect.y + previewHeight}, {fullRect.x2, fullRect.y2}};

	setViewRect(bottomRect, bottomRect);
	TableView::place();

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

	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, 1.});
	cmds.drawQuad(bgQuads, 0);

	if(hasContent && sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.draw(cmds);
	}

	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, .7});
	cmds.drawQuad(bgQuads, 1);

	TableView::draw(cmds, {});
}

}
