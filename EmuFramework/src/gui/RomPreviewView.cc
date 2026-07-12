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
import imagine;

namespace EmuEx
{

using namespace IG;

[[maybe_unused]] constexpr SystemLogger log{"RomPreviewView"};

// Run 3 emulated frames per screen frame (~3x speed at 60fps)
static constexpr int previewSpeed = 3;

RomPreviewView::RomPreviewView(ViewAttachParams attach, [[maybe_unused]] const Input::Event &e):
	TableView
	{
		"ROM Preview",
		attach,
		[this](ItemMessage msg) -> ItemReply
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply { return 2u; },
				[&](const GetItemMessage& m) -> ItemReply
				{
					switch(m.idx)
					{
						case 0: return &playItem;
						case 1: return &backItem;
						default: std::unreachable();
					}
				},
			});
		}
	},
	playItem
	{
		"\xe5\xbc\x80\xe5\xa7\x8b\xe6\xb8\xb8\xe6\x88\x8f", attach,
		[this](const Input::Event &e)
		{
			stopPreview();
			app().launchSystem(e);
		}
	},
	backItem
	{
		"\xe8\xbf\x94\xe5\x9b\x9e", attach,
		[this](const Input::Event&)
		{
			stopPreview();
			app().closeSystem();
			dismiss();
		}
	},
	bgQuads{attach.rendererTask, {.size = 1}}
{}

RomPreviewView::~RomPreviewView()
{
	stopPreview();
}

void RomPreviewView::onAddedToController(ViewController *vc, const Input::Event &e)
{
	TableView::onAddedToController(vc, e);
	startPreview();
}

void RomPreviewView::startPreview()
{
	if(isRunning)
		return;
	auto &app = this->app();
	auto &sys = system();
	if(!sys.hasContent())
		return;
	log.info("starting ROM preview at {}x speed", previewSpeed);
	// Follow benchmark pattern: run frames directly without start()
	// Run a few frames first to initialize the video buffer
	for([[maybe_unused]] auto i: iotaCount(30))
	{
		sys.runFrame({}, &app.video, nullptr);
	}
	isRunning = true;
	onFrameDel =
		[this](FrameParams)
		{
			auto &sys = system();
			auto &app = this->app();
			if(!sys.hasContent())
				return false;
			for([[maybe_unused]] auto i: iotaCount(previewSpeed))
			{
				sys.runFrame({}, &app.video, nullptr);
			}
			return true;
		};
	app.emuWindow().addOnFrame(onFrameDel);
}

void RomPreviewView::stopPreview()
{
	if(!isRunning)
		return;
	isRunning = false;
	if(onFrameDel)
	{
		auto &app = this->app();
		app.emuWindow().removeOnFrame(onFrameDel);
		onFrameDel = {};
	}
}

void RomPreviewView::place()
{
	TableView::place();
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.place({}, displayRect(), nullptr, sys);
	}
	bgQuads.write(0, {.bounds = viewRect().as<int16_t>()});
}

void RomPreviewView::draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams) const
{
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.draw(cmds);
	}
	using namespace Gfx;
	auto &basicEffect = cmds.basicEffect();
	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, .7});
	cmds.drawQuad(bgQuads, 0);
	TableView::draw(cmds, {});
}

}
