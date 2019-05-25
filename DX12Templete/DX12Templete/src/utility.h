#pragma once
#include "stddef.h"

std::wstring GetExecutionDirectory()
{
	wchar_t filePath[MAX_PATH];
	GetModuleFileNameW(NULL, filePath, sizeof(filePath));
	wchar_t* p = wcsrchr(filePath, L'\\');
	std::wstring strPath(filePath, p);
	return strPath;
}
