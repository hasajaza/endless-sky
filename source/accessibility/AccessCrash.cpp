/* AccessCrash.cpp
Copyright (c) 2026 by the Endless Sky Accessibility contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "AccessCrash.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {
	wchar_t reportPath[MAX_PATH * 2] = {};
	char screenName[128] = "none";
	LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;

	void ModuleOf(DWORD64 address, char *name, size_t size, DWORD64 &base)
	{
		HMODULE module = nullptr;
		name[0] = '\0';
		base = 0;
		if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(address), &module) && module)
		{
			char path[MAX_PATH] = {};
			GetModuleFileNameA(module, path, MAX_PATH);
			const char *slash = strrchr(path, '\\');
			snprintf(name, size, "%s", slash ? slash + 1 : path);
			base = reinterpret_cast<DWORD64>(module);
		}
		else
			snprintf(name, size, "unknown");
	}

	LONG WINAPI Filter(EXCEPTION_POINTERS *info)
	{
		FILE *file = _wfopen(reportPath, L"w");
		if(file)
		{
			fprintf(file, "Endless Sky Accessible crash report\n");
			fprintf(file, "Screen: %s\n", screenName);
			fprintf(file, "Exception code: 0x%08lx\n",
				static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode));
			if(info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
					&& info->ExceptionRecord->NumberParameters >= 2)
				fprintf(file, "Access: %s address 0x%llx\n",
					info->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
					static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[1]));

			// Walk the stack with the unwind data every x64 module carries.
			CONTEXT context = *info->ContextRecord;
			fprintf(file, "Stack:\n");
			for(int frame = 0; frame < 64 && context.Rip; ++frame)
			{
				char name[MAX_PATH];
				DWORD64 base = 0;
				ModuleOf(context.Rip, name, sizeof(name), base);
				fprintf(file, "  %2d  %s+0x%llx\n", frame, name,
					static_cast<unsigned long long>(context.Rip - base));

				DWORD64 imageBase = 0;
				PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
				if(!function)
				{
					// Leaf function: the return address is on top of the stack.
					if(IsBadReadPtr(reinterpret_cast<void *>(context.Rsp), sizeof(DWORD64)))
						break;
					context.Rip = *reinterpret_cast<DWORD64 *>(context.Rsp);
					context.Rsp += 8;
					continue;
				}
				PVOID handlerData = nullptr;
				DWORD64 establisher = 0;
				RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context,
					&handlerData, &establisher, nullptr);
			}
			fclose(file);
		}
		return previousFilter ? previousFilter(info) : EXCEPTION_CONTINUE_SEARCH;
	}
}



void AccessCrash::Install(const std::filesystem::path &reportFile)
{
	std::wstring path = reportFile.wstring();
	wcsncpy(reportPath, path.c_str(), sizeof(reportPath) / sizeof(reportPath[0]) - 1);
	previousFilter = SetUnhandledExceptionFilter(Filter);
}



void AccessCrash::SetContext(const char *screen)
{
	snprintf(screenName, sizeof(screenName), "%s", screen && *screen ? screen : "unnamed panel");
}

#else

void AccessCrash::Install(const std::filesystem::path &)
{
}



void AccessCrash::SetContext(const char *)
{
}

#endif
