/*  This file is part of Snes9x EX.

	Please see COPYING file in root directory for license information. */

module;
#include <snes9x.h>
#include <cheats.h>

module system;

extern "C++"
{
void S9xEnableCheat(SCheat&);
void S9xDisableCheat(SCheat&);
SCheat S9xTextToCheat(const std::string&);
std::string S9xCheatToText(const SCheat&);
}

namespace EmuEx
{

unsigned parseHex(const char* str) { return strtoul(str, nullptr, 16); }

int numCheats()
{
	#ifndef SNES9X_VERSION_1_4
	return ::Cheat.group.size();
	#else
	return ::Cheat.num_cheats;
	#endif
}

static FS::PathString cheatsFilename(Snes9xSystem& sys)
{
	return sys.userFilePath(sys.cheatsDir, ".cht");
}

void Snes9xSystem::writeCheatFile()
{
	if(!numCheats())
		Snes9xSystem::log.info("no cheats present, removing .cht file if present");
	else
		Snes9xSystem::log.info("saving {} cheat(s)", numCheats());
	S9xSaveCheatFile(cheatsFilename(*this).data());
}

static void setCheatCodeEnabled(CheatCode& c, bool on)
{
	if(on)
		S9xEnableCheat(c);
	else
		S9xDisableCheat(c);
}

static bool tryDisableCheatCode(CheatCode& c)
{
	bool isEnabled = c.enabled;
	S9xDisableCheat(c);
	return isEnabled;
}

void setCheatAddress(CheatCode& cheat, uint32_t a)
{
	auto isEnabled = tryDisableCheatCode(cheat);
	cheat.address = a;
	setCheatCodeEnabled(cheat, isEnabled);
	static_cast<Snes9xSystem&>(EmuEx::gSystem()).writeCheatFile();
}

void setCheatValue(CheatCode& cheat, uint8 v)
{
	auto isEnabled = tryDisableCheatCode(cheat);
	cheat.byte = v;
	setCheatCodeEnabled(cheat, isEnabled);
	static_cast<Snes9xSystem&>(EmuEx::gSystem()).writeCheatFile();
}

void setCheatConditionalValue(CheatCode& cheat, bool conditional, uint8 v)
{
	auto isEnabled = tryDisableCheatCode(cheat);
	#ifndef SNES9X_VERSION_1_4
	cheat.conditional = conditional;
	cheat.cond_byte = v;
	#else
	cheat.saved = conditional;
	cheat.saved_byte = v;
	#endif
	setCheatCodeEnabled(cheat, isEnabled);
	static_cast<Snes9xSystem&>(EmuEx::gSystem()).writeCheatFile();
}

void setCheatConditionalValue(CheatCode& cheat, int v)
{
	if(v >= 0 && v <= 0xFF)
	{
		setCheatConditionalValue(cheat, true, uint8(v));
	}
	else
	{
		setCheatConditionalValue(cheat, false, 0u);
	}
}

static std::pair<bool, uint8> cheatConditionalValue(CheatCode& c)
{
	#ifndef SNES9X_VERSION_1_4
	return {c.conditional, c.cond_byte};
	#else
	return {c.saved, c.saved_byte};
	#endif
}

std::string codeConditionalToString(CheatCode& c)
{
	auto [cond, byte] = cheatConditionalValue(c);
	return cond ? std::format("{:x}", byte) : std::string{};
}

Cheat* Snes9xSystem::newCheat(EmuApp& app, const char* name, CheatCodeDesc desc)
{
	#ifndef SNES9X_VERSION_1_4
	if(S9xAddCheatGroup(name, desc.str) == -1)
	{
		app.postMessage(true, "Invalid code");
		return {};
	}
	Snes9xSystem::log.info("added new cheat, {} total", ::Cheat.group.size());
	writeCheatFile();
	return static_cast<Cheat*>(&::Cheat.group.back());
	#else
	uint8 byte;
	uint32 address;
	uint8 bytes[3];
	bool8 sram;
	uint8 numBytes;
	if(!S9xGameGenieToRaw(desc.str, address, byte))
	{
		S9xAddCheat(false, true, address, byte);
		return static_cast<Cheat*>(&::Cheat.c[numCheats() - 1]);
	}
	else if(!S9xProActionReplayToRaw (desc.str, address, byte))
	{
		S9xAddCheat(false, true, address, byte);
		return static_cast<Cheat*>(&::Cheat.c[numCheats() - 1]);
	}
	else if(!S9xGoldFingerToRaw(desc.str, address, sram, numBytes, bytes))
	{
		for(auto i: iotaCount(numBytes))
			S9xAddCheat(false, true, address + i, bytes[i]);
		// TODO: handle cheat names for multiple codes added at once
		return static_cast<Cheat*>(&::Cheat.c[numCheats() - 1]);
	}
	return {};
	#endif
}

bool Snes9xSystem::setCheatName(Cheat& c, const char* name)
{
	c.name = name;
	writeCheatFile();
	return true;
}

std::string_view Snes9xSystem::cheatName(const Cheat& c) const { return c.name; }

void Snes9xSystem::setCheatEnabled(Cheat& c, bool on)
{
	#ifndef SNES9X_VERSION_1_4
	c.enabled = on;
  for(auto& c :c.cheat)
  {
  	setCheatCodeEnabled(static_cast<CheatCode&>(c), on);
  }
	#else
  auto idx = std::distance(::Cheat.c, static_cast<SCheat*>(&c));
	if(on)
		S9xEnableCheat(idx);
	else
		S9xDisableCheat(idx);
	#endif
	writeCheatFile();
}

bool Snes9xSystem::isCheatEnabled(const Cheat& c) const { return c.enabled; }

bool Snes9xSystem::addCheatCode(EmuApp& app, Cheat*& cheatPtr, CheatCodeDesc desc)
{
	#ifndef SNES9X_VERSION_1_4
	SCheat newCheat = S9xTextToCheat(desc.str);
	if(!newCheat.address)
	{
		app.postMessage(true, "Invalid code");
		return {};
	}
	newCheat.enabled = cheatPtr->enabled;
	setCheatCodeEnabled(static_cast<CheatCode&>(newCheat), cheatPtr->enabled);
	cheatPtr->cheat.emplace_back(newCheat);
	writeCheatFile();
	return true;
	#else
	return false;
	#endif
}

Cheat* Snes9xSystem::removeCheatCode(Cheat& c, CheatCode& code)
{
	#ifndef SNES9X_VERSION_1_4
	S9xDisableCheat(code);
	c.cheat.erase(toIterator(c.cheat, static_cast<SCheat&>(code)));
	bool removedAllCodes = c.cheat.empty();
	if(removedAllCodes)
		::Cheat.group.erase(toIterator(::Cheat.group, static_cast<SCheatGroup&>(c)));
	writeCheatFile();
	return removedAllCodes ? nullptr : &c;
	#else
	return nullptr;
	#endif
}

bool Snes9xSystem::removeCheat(Cheat& c)
{
	#ifndef SNES9X_VERSION_1_4
	S9xDeleteCheatGroup(std::distance(::Cheat.group.data(), static_cast<SCheatGroup*>(&c)));
	#else
	S9xDeleteCheat(std::distance(::Cheat.c, static_cast<SCheat*>(&c)));
	#endif
	writeCheatFile();
	return true;
}

void Snes9xSystem::forEachCheat(DelegateFunc<bool(Cheat&, std::string_view)> del)
{
	#ifndef SNES9X_VERSION_1_4
	for(auto& c: ::Cheat.group)
	#else
	for(auto& c: ::Cheat.c | std::views::take(::Cheat.num_cheats))
	#endif
	{
		if(!del(static_cast<Cheat&>(c), c.name))
			break;
	}
}

void Snes9xSystem::forEachCheatCode(Cheat& cheat, DelegateFunc<bool(CheatCode&, std::string_view)> del)
{
	#ifndef SNES9X_VERSION_1_4
	for(auto& c: cheat.cheat)
	{
		if(!del(static_cast<CheatCode&>(c), S9xCheatToText(c)))
			break;
	}
	#endif
}

int Snes9xSystem::batchAddCheats(EmuApp& app, const char* text)
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
		if(!name.empty())
		{
			currentCheat = newCheat(app, name.c_str(), {code.c_str(), 0});
			if(currentCheat) count++;
		}
		else if(!pendingName.empty())
		{
			currentCheat = newCheat(app, pendingName.c_str(), {code.c_str(), 0});
			if(currentCheat) count++;
			pendingName.clear();
		}
		else if(currentCheat)
		{
			auto* cPtr = currentCheat;
			addCheatCode(app, cPtr, {code.c_str(), 0});
		}
		else
		{
			std::string defName = "金手指 " + std::to_string(count + 1);
			currentCheat = newCheat(app, defName.c_str(), {code.c_str(), 0});
			if(currentCheat) count++;
		}
	}
	#ifndef SNES9X_VERSION_1_4
	for(auto& c: ::Cheat.group)
	{
		c.enabled = true;
		for(auto& cc: c.cheat)
			setCheatCodeEnabled(static_cast<CheatCode&>(cc), true);
	}
	#endif
	writeCheatFile();
	return count;
}

}
