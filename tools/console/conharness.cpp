// Runs a program in a real (hidden) Windows console, types into it with console key events, and prints
// what is on its screen. The point is to test what a person would do at a keyboard, which a pipe cannot:
// a console hands a program lines, and only the console shows whether a key reached it as it was pressed.
//
//   conharness <timeout-ms> <step>... -- <command> [<argument>...]
//   steps:  wait:<ms>        sleep
//           waitfor:<text>   wait (up to a minute) until the text is on the screen
//           text:<chars>     type these characters
//           key:<name>       UP DOWN LEFT RIGHT ENTER ESC TAB BACK HOME END PGUP PGDN DEL F1..F12
//           dump             print the screen now
//
// The screen is printed at the end, then the console's input mode as the program left it (it must be what it
// was when the program started) and how the program exited. tools/consolecheck.ps1 builds and uses it.
// Windows only: it needs g++ (MinGW) to build.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static HANDLE gIn, gOut;

static void sendKey(WORD vk, wchar_t ch, bool down)
{
	INPUT_RECORD r{};
	r.EventType = KEY_EVENT;
	r.Event.KeyEvent.bKeyDown = down;
	r.Event.KeyEvent.wRepeatCount = 1;
	r.Event.KeyEvent.wVirtualKeyCode = vk;
	r.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
	r.Event.KeyEvent.uChar.UnicodeChar = ch;
	DWORD written = 0;
	WriteConsoleInputW(gIn, &r, 1, &written);
}

static void typeChar(wchar_t ch)
{
	SHORT scan = VkKeyScanW(ch);
	WORD vk = scan == -1 ? 0 : LOBYTE(scan);
	sendKey(vk, ch, true);
	sendKey(vk, ch, false);
}

static bool screenHas(const std::string& needle)
{
	CONSOLE_SCREEN_BUFFER_INFO info{};
	GetConsoleScreenBufferInfo(gOut, &info);
	for (SHORT y = 0; y <= info.dwCursorPosition.Y; ++y)
	{
		wchar_t line[512]; DWORD n = 0;
		ReadConsoleOutputCharacterW(gOut, line, info.dwSize.X < 512 ? info.dwSize.X : 511, COORD{0, y}, &n);
		std::string s; for (DWORD i = 0; i < n; ++i) s += line[i] < 128 ? static_cast<char>(line[i]) : '?';
		if (s.find(needle) != std::string::npos) return true;
	}
	return false;
}

static void dump()
{
	CONSOLE_SCREEN_BUFFER_INFO info{};
	GetConsoleScreenBufferInfo(gOut, &info);
	for (SHORT y = 0; y <= info.dwCursorPosition.Y; ++y)
	{
		wchar_t line[512];
		DWORD n = 0;
		ReadConsoleOutputCharacterW(gOut, line, info.dwSize.X < 512 ? info.dwSize.X : 511, COORD{0, y}, &n);
		int end = static_cast<int>(n);
		while (end > 0 && line[end - 1] == L' ') --end;
		std::string s;
		for (int i = 0; i < end; ++i) s += line[i] < 128 ? static_cast<char>(line[i]) : '?';
		printf("| %s\n", s.c_str());
	}
	fflush(stdout);
}

int main(int argc, char** argv)
{
	if (argc < 4) { fprintf(stderr, "usage\n"); return 2; }
	const DWORD timeout = static_cast<DWORD>(atoi(argv[1]));
	std::vector<std::string> steps;
	std::string command;
	int i = 2;
	for (; i < argc && std::string(argv[i]) != "--"; ++i) steps.push_back(argv[i]);
	for (++i; i < argc; ++i)
	{
		if (!command.empty()) command += ' ';
		const std::string arg = argv[i];
		command += arg.find(' ') == std::string::npos ? arg : "\"" + arg + "\"";   // a path with a space stays one argument
	}

	FreeConsole();
	STARTUPINFOA si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi{};
	std::vector<char> cmd(command.begin(), command.end()); cmd.push_back(0);
	if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi))
	{ fprintf(stderr, "CreateProcess failed %lu\n", GetLastError()); return 2; }
	Sleep(700);
	if (!AttachConsole(pi.dwProcessId)) { fprintf(stderr, "AttachConsole failed %lu\n", GetLastError()); return 2; }
	gIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	gOut = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

	for (const std::string& step : steps)
	{
		if (step.rfind("wait:", 0) == 0) Sleep(static_cast<DWORD>(atoi(step.c_str() + 5)));
		else if (step == "dump") dump();
		else if (step.rfind("waitfor:", 0) == 0) { const std::string w = step.substr(8); for (int k = 0; k < 600 && !screenHas(w); ++k) Sleep(100); if (!screenHas(w)) printf("(gave up waiting for '%s')\n", w.c_str()); }
		else if (step.rfind("text:", 0) == 0) { for (char c : step.substr(5)) { typeChar(static_cast<wchar_t>(c)); Sleep(15); } }
		else if (step.rfind("key:", 0) == 0)
		{
			const std::string k = step.substr(4);
			WORD vk = 0; wchar_t ch = 0;
			if (k == "UP") vk = VK_UP; else if (k == "DOWN") vk = VK_DOWN; else if (k == "LEFT") vk = VK_LEFT; else if (k == "RIGHT") vk = VK_RIGHT;
			else if (k == "ENTER") { vk = VK_RETURN; ch = L'\r'; } else if (k == "ESC") { vk = VK_ESCAPE; ch = 27; }
			else if (k == "TAB") { vk = VK_TAB; ch = L'\t'; } else if (k == "BACK") { vk = VK_BACK; ch = 8; }
			else if (k == "HOME") vk = VK_HOME; else if (k == "END") vk = VK_END; else if (k == "PGUP") vk = VK_PRIOR; else if (k == "PGDN") vk = VK_NEXT;
			else if (k == "DEL") vk = VK_DELETE;
			else if (k[0] == 'F') vk = static_cast<WORD>(VK_F1 + atoi(k.c_str() + 1) - 1);
			sendKey(vk, ch, true); sendKey(vk, ch, false); Sleep(40);
		}
	}
	DWORD wait = WaitForSingleObject(pi.hProcess, timeout);
	dump();
	if (wait == WAIT_TIMEOUT) { printf("RESULT: still running after %lu ms (killed)\n", timeout); TerminateProcess(pi.hProcess, 99); return 1; }
	DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code); { DWORD m = 0; GetConsoleMode(gIn, &m); printf("console input mode after exit: %08lx\n", m); }
	printf("RESULT: exited with %lu\n", code);
	return 0;
}
