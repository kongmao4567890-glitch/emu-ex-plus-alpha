/*  This file is part of Saturn.emu.

	Saturn.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Saturn.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Saturn.emu.  If not, see <http://www.gnu.org/licenses/> */

module;
#include <mednafen/mednafen.h>
#include <mednafen/mempatcher-driver.h>

module system;

namespace EmuEx
{

using namespace Mednafen;

unsigned parseHex(const char* str) { return strtoul(str, nullptr, 16); }

static unsigned hexDigitCount(const char* str)
{
	unsigned count = 0;
	while(std::isxdigit(static_cast<unsigned char>(*str))) { count++; str++; }
	return count;
}

static unsigned byteLengthFromHexDigits(unsigned digits)
{
	if(digits <= 2) return 1;
	if(digits <= 4) return 2;
	return 4;
}

void SaturnSystem::syncCheats()
{
	// Remove all existing mednafen patches
	int count = 0;
	MDFNI_ListCheats([](const Mednafen::MemoryPatch&, void* data) -> int
	{
		(*static_cast<int*>(data))++;
		return 1;
	}, &count);
	for(int i = count - 1; i >= 0; i--)
		MDFNI_DelCheat(i);
	// Re-add all enabled cheats
	for(auto& cheat: cheats)
	{
		if(!cheat.enabled) continue;
		for(auto& code: cheat.codes)
		{
			Mednafen::MemoryPatch patch;
			patch.name = cheat.name;
			patch.addr = code.addr;
			patch.val = code.val;
			patch.length = code.length;
			patch.bigendian = true;
			patch.status = true;
			if(code.compare >= 0)
			{
				patch.compare = code.compare;
				patch.type = 'C';
			}
			else
			{
				patch.type = 'R';
			}
			MDFNI_AddCheat(patch);
		}
	}
}

Cheat* SaturnSystem::newCheat(EmuApp& app, const char* name, CheatCodeDesc desc)
{
	auto* cPtr = &cheats.emplace_back(name[0] ? name : "未命名");
	if(!addCheatCode(app, cPtr, desc))
	{
		cheats.pop_back();
		return {};
	}
	log.info("added new cheat, {} total", cheats.size());
	return cPtr;
}

bool SaturnSystem::setCheatName(Cheat& c, const char* name)
{
	c.name = name;
	return true;
}

std::string_view SaturnSystem::cheatName(const Cheat& c) const { return c.name; }

void SaturnSystem::setCheatEnabled(Cheat& c, bool on)
{
	c.enabled = on;
	syncCheats();
}

bool SaturnSystem::isCheatEnabled(const Cheat& c) const { return c.enabled; }

bool SaturnSystem::addCheatCode(EmuApp& app, Cheat*& cheatPtr, CheatCodeDesc desc)
{
	// Parse format: address:value[:compare]
	const char* str = desc.str;
	if(!str || !*str)
	{
		app.postMessage(true, "无效输入");
		return false;
	}
	// Find colon separator
	const char* colon = std::strchr(str, ':');
	if(!colon)
	{
		app.postMessage(true, "格式错误，请使用 地址:数值");
		return false;
	}
	std::string addrStr(str, colon - str);
	auto addr = parseHex(addrStr.c_str());
	if(addr > 0xFFFFFFFF)
	{
		app.postMessage(true, "地址超出范围");
		return false;
	}
	const char* valStart = colon + 1;
	const char* secondColon = std::strchr(valStart, ':');
	std::string valStr;
	if(secondColon)
		valStr.assign(valStart, secondColon - valStart);
	else
		valStr.assign(valStart);
	auto valDigits = hexDigitCount(valStr.c_str());
	auto val = strtoull(valStr.c_str(), nullptr, 16);
	auto length = byteLengthFromHexDigits(valDigits);
	int64_t compare = -1;
	if(secondColon)
	{
		std::string compStr(secondColon + 1);
		compare = strtoull(compStr.c_str(), nullptr, 16);
	}
	cheatPtr->codes.emplace_back(addr, val, compare, length);
	syncCheats();
	return true;
}

bool SaturnSystem::modifyCheatCode(EmuApp& app, Cheat& cheat, CheatCode& c, CheatCodeDesc desc)
{
	const char* str = desc.str;
	if(!str || !*str)
		return false;
	const char* colon = std::strchr(str, ':');
	if(!colon)
	{
		app.postMessage(true, "格式错误，请使用 地址:数值");
		return false;
	}
	std::string addrStr(str, colon - str);
	c.addr = parseHex(addrStr.c_str());
	const char* valStart = colon + 1;
	const char* secondColon = std::strchr(valStart, ':');
	std::string valStr;
	if(secondColon)
		valStr.assign(valStart, secondColon - valStart);
	else
		valStr.assign(valStart);
	auto valDigits = hexDigitCount(valStr.c_str());
	c.val = strtoull(valStr.c_str(), nullptr, 16);
	c.length = byteLengthFromHexDigits(valDigits);
	if(secondColon)
	{
		std::string compStr(secondColon + 1);
		c.compare = strtoull(compStr.c_str(), nullptr, 16);
	}
	else
	{
		c.compare = -1;
	}
	syncCheats();
	return true;
}

Cheat* SaturnSystem::removeCheatCode(Cheat& c, CheatCode& code)
{
	c.codes.erase(toIterator(c.codes, code));
	bool removedAllCodes = c.codes.empty();
	if(removedAllCodes)
		cheats.erase(toIterator(cheats, c));
	syncCheats();
	return removedAllCodes ? nullptr : &c;
}

bool SaturnSystem::removeCheat(Cheat& c)
{
	cheats.erase(toIterator(cheats, c));
	syncCheats();
	return true;
}

void SaturnSystem::forEachCheat(DelegateFunc<bool(Cheat&, std::string_view)> del)
{
	for(auto& c: cheats)
	{
		if(!del(c, std::string_view{c.name}))
			break;
	}
}

void SaturnSystem::forEachCheatCode(Cheat& cheat, DelegateFunc<bool(CheatCode&, std::string_view)> del)
{
	for(auto& c: cheat.codes)
	{
		std::string code = std::format("{:X}:{:X}", c.addr, c.val);
		if(c.compare >= 0)
			code += std::format(":{:X}", c.compare);
		del(c, std::string_view{code});
	}
}

// Batch add: parse multiple cheat lines in format "name|address:value[:compare]"
int SaturnSystem::batchAddCheats(EmuApp& app, const char* text)
{
	int count = 0;
	std::string_view input{text};
	auto lines = std::views::split(input, '\n');
	for(auto line: lines)
	{
		std::string_view lineStr{line};
		// Trim whitespace
		while(!lineStr.empty() && std::isspace(static_cast<unsigned char>(lineStr.front())))
			lineStr.remove_prefix(1);
		while(!lineStr.empty() && std::isspace(static_cast<unsigned char>(lineStr.back())))
			lineStr.remove_suffix(1);
		if(lineStr.empty()) continue;
		// Find pipe separator
		auto pipePos = lineStr.find('|');
		std::string name, code;
		if(pipePos != std::string_view::npos)
		{
			name = std::string{lineStr.substr(0, pipePos)};
			code = std::string{lineStr.substr(pipePos + 1)};
		}
		else
		{
			code = std::string{lineStr};
			name = "金手指 " + std::to_string(cheats.size() + 1);
		}
		// Trim code
		while(!code.empty() && std::isspace(static_cast<unsigned char>(code.front())))
			code.erase(code.begin());
		while(!code.empty() && std::isspace(static_cast<unsigned char>(code.back())))
			code.pop_back();
		if(code.empty()) continue;
		auto* cheatPtr = newCheat(app, name.c_str(), {code.c_str(), 0});
		if(cheatPtr)
		{
			cheatPtr->enabled = true;
			count++;
		}
	}
	syncCheats();
	return count;
}

}
