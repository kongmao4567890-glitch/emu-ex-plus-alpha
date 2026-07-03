/*  This file is part of NES.emu.

	NES.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	NES.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with NES.emu.  If not, see <http://www.gnu.org/licenses/> */

module;
#include <fceu/driver.h>
#include <fceu/cheat.h>

module system;

extern "C++"
{
void EncodeGG(char* str, int a, int v, int c);
void RebuildSubCheats();
}

namespace EmuEx
{

unsigned parseHex(const char* str) { return strtoul(str, nullptr, 16); }

std::string toGGString(const CheatCode& c)
{
	std::string code;
	code.resize(9);
	EncodeGG(code.data(), c.addr, c.val, c.compare);
	code.resize(8);
	return code;
}

void saveCheats()
{
	savecheats = 1;
	FCEU_FlushGameCheats(nullptr, 0, false);
}

void syncCheats()
{
	saveCheats();
	RebuildSubCheats();
}

constexpr bool isValidGGCodeLen(const char* str)
{
	return std::string_view{str}.size() == 6 || std::string_view{str}.size() == 8;
}

Cheat* NesSystem::newCheat(EmuApp& app, const char* name, CheatCodeDesc desc)
{
	auto cPtr = &static_cast<Cheat&>(cheats.emplace_back(name));
	if(!addCheatCode(app, cPtr, desc))
	{
		cheats.pop_back();
		return {};
	}
	log.info("added new cheat, {} total", cheats.size());
	return cPtr;
}

bool NesSystem::setCheatName(Cheat& c, const char* name)
{
	c.name = name;
	saveCheats();
	return true;
}

std::string_view NesSystem::cheatName(const Cheat& c) const { return c.name; }

void NesSystem::setCheatEnabled(Cheat& c, bool on)
{
	c.status = on;
	syncCheats();
}

bool NesSystem::isCheatEnabled(const Cheat& c) const { return c.status; }

bool NesSystem::addCheatCode(EmuApp& app, Cheat*& cheatPtr, CheatCodeDesc desc)
{
	if(desc.flags)
	{
		if(!isValidGGCodeLen(desc.str))
		{
			app.postMessage(true, "Invalid, must be 6 or 8 digits");
			return false;
		}
		uint16 a; uint8 v; int c;
		if(!FCEUI_DecodeGG(desc.str, &a, &v, &c))
		{
			app.postMessage(true, "Error decoding code");
			return false;
		}
		cheatPtr->codes.emplace_back(a, v, c, 1);
	}
	else
	{
		auto a = parseHex(desc.str);
		if(a > 0xFFFF)
		{
			app.postMessage(true, "Invalid address");
			return false;
		}
		cheatPtr->codes.emplace_back(a, 0, -1, 0);
	}
	syncCheats();
	return true;
}

bool NesSystem::modifyCheatCode(EmuApp& app, Cheat&, CheatCode& c, CheatCodeDesc desc)
{
	assume(desc.flags);
	if(!isValidGGCodeLen(desc.str))
	{
		app.postMessage(true, "Invalid, must be 6 or 8 digits");
		return false;
	}
	if(!FCEUI_DecodeGG(desc.str, &c.addr, &c.val, &c.compare))
	{
		app.postMessage(true, "Error decoding code");
		return false;
	}
	syncCheats();
	return true;
}

Cheat* NesSystem::removeCheatCode(Cheat& c, CheatCode& code)
{
	c.codes.erase(toIterator(c.codes, static_cast<CHEATCODE&>(code)));
	bool removedAllCodes = c.codes.empty();
	if(removedAllCodes)
		cheats.erase(toIterator(cheats, static_cast<CHEATF&>(c)));
	syncCheats();
	return removedAllCodes ? nullptr : &c;
}

bool NesSystem::removeCheat(Cheat& c)
{
	cheats.erase(toIterator(cheats, static_cast<CHEATF&>(c)));
	syncCheats();
	return true;
}

void NesSystem::forEachCheat(DelegateFunc<bool(Cheat&, std::string_view)> del)
{
	for(auto& c: cheats)
	{
		if(!del(static_cast<Cheat&>(c), std::string_view{c.name}))
			break;
	}
}

void NesSystem::forEachCheatCode(Cheat& cheat, DelegateFunc<bool(CheatCode&, std::string_view)> del)
{
	for(auto& c_: cheat.codes)
	{
		auto& c = static_cast<CheatCode&>(c_);
		std::string code;
		if(c.type)
		{
			code = toGGString(c);
		}
		else
		{
			code = std::format("{:x}:{:x}", c.addr, c.val);
			if(c.compare != -1)
				code += std::format(":{:x}", c.compare);
		}
		del(c, std::string_view{code});
	}
}

int NesSystem::batchAddCheats(EmuApp& app, const char* text)
{
	int count = 0;
	std::string_view input{text};
	std::string pendingName;
	Cheat* currentCheat = nullptr;
	auto lines = std::views::split(input, '\n');
	for(auto line: lines)
	{
		std::string_view lineStr{line};
		while(!lineStr.empty() && ::isspace(static_cast<unsigned char>(lineStr.front())))
			lineStr.remove_prefix(1);
		while(!lineStr.empty() && ::isspace(static_cast<unsigned char>(lineStr.back())))
			lineStr.remove_suffix(1);
		if(lineStr.empty()) continue;
		if(lineStr.back() == '|')
		{
			pendingName = std::string{lineStr.substr(0, lineStr.size() - 1)};
			if(pendingName.empty())
				pendingName = "金手指 " + std::to_string(count + 1);
			currentCheat = nullptr;
			continue;
		}
		auto pipePos = lineStr.find('|');
		std::string name, code;
		if(pipePos != std::string_view::npos)
		{
			name = std::string{lineStr.substr(0, pipePos)};
			code = std::string{lineStr.substr(pipePos + 1)};
			if(name.empty())
				name = "金手指 " + std::to_string(count + 1);
			pendingName.clear();
			currentCheat = nullptr;
		}
		else
		{
			code = std::string{lineStr};
		}
		while(!code.empty() && ::isspace(static_cast<unsigned char>(code.front())))
			code.erase(code.begin());
		while(!code.empty() && ::isspace(static_cast<unsigned char>(code.back())))
			code.pop_back();
		if(code.empty()) continue;
		unsigned flags = isValidGGCodeLen(code.c_str()) ? 1 : 0;
		if(!name.empty())
		{
			currentCheat = newCheat(app, name.c_str(), {code.c_str(), flags});
			if(currentCheat) count++;
		}
		else if(!pendingName.empty())
		{
			currentCheat = newCheat(app, pendingName.c_str(), {code.c_str(), flags});
			if(currentCheat) count++;
			pendingName.clear();
		}
		else if(currentCheat)
		{
			auto* cPtr = currentCheat;
			addCheatCode(app, cPtr, {code.c_str(), flags});
		}
		else
		{
			std::string defName = "金手指 " + std::to_string(count + 1);
			currentCheat = newCheat(app, defName.c_str(), {code.c_str(), flags});
			if(currentCheat) count++;
		}
	}
	for(auto& c: cheats)
		static_cast<Cheat&>(c).status = 1;
	syncCheats();
	return count;
}

}
