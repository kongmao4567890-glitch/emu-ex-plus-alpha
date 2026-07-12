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

RomPreviewView::RomPreviewView(ViewAttachParams attach, const Input::Event &e):
	TableView
	{
		"ROM Preview",
		attach,
		[this](ItemMessage msg) -> ItemReply
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply { return 3u; },
				[&](const GetItemMessage& m) -> ItemReply
				{
					switch(m.idx)
					{
						case 0: return &playItem;
						case 1: return &nextFrameItem;
						case 2: return &backItem;
						default: std::unreachable();
					}
				},
			});
		}
	},
	playItem
	{
		"\xe5\xbc\x80\xe5\xa7\x8b\xe6\xb8\xb8\xe6\x88\x8f", attachParams(),
		[this](const Input::Event& e)
		{
			app().launchSystem(e);
		}
	},
	nextFrameItem
	{
		"\xe8\xbf\x9b\xe5\xb8\xa7 60 \xe5\xb8\xa7", attachParams(),
		[this](const Input::Event&)
		{
			runNextFrames(60);
		}
	},
	backItem
	{
		"\xe8\xbf\x94\xe5\x9b\x9e", attachParams(),
		[this](const Input::Event&)
		{
			app().closeSystem();
			dismiss();
		}
	},
	launchEvent{e},
	bgQuads{attach.rendererTask, {.size = 1}}
{
	// Set up video format for the loaded ROM
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent())
	{
		app.setRenderPixelFormat(app.windowPixelFormat());
	}
}

void RomPreviewView::runNextFrames(int count)
{
	auto &app = this->app();
	auto &sys = system();
	if(!sys.hasContent())
		return;
	for([[maybe_unused]] auto i: iotaCount(count))
	{
		sys.runFrame({}, &app.video, nullptr);
	}
	postDraw();
}

void RomPreviewView::prepareDraw()
{
	TableView::prepareDraw();
}

void RomPreviewView::place()
{
	TableView::place();
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent())
	{
		app.videoLayer.place({}, displayRect(), nullptr, sys);
	}
	bgQuads.write(0, {.bounds = viewRect().as<int16_t>()});
}

void RomPreviewView::draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams) const
{
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent())
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
