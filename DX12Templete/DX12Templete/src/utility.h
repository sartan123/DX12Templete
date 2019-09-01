#pragma once
#include "stddef.h"

static std::wstring GetExecutionDirectory()
{
	wchar_t filePath[MAX_PATH];
	GetModuleFileNameW(NULL, filePath, sizeof(filePath));
	wchar_t* p = wcsrchr(filePath, L'\\');
	std::wstring strPath(filePath, p);
	return strPath;
}

static inline void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
{
	if (path == nullptr)
	{
		throw std::exception();
	}

	DWORD size = GetModuleFileNameW(nullptr, path, pathSize);
	if (size == 0 || size == pathSize)
	{
		// Method failed or path was truncated.
		throw std::exception();
	}

	WCHAR* lastSlash = wcsrchr(path, L'\\');
	if (lastSlash)
	{
		*(lastSlash + 1) = L'\0';
	}
}

static std::wstring GetAssetFullPath(std::wstring assetsPath, LPCWSTR assetName)
{
	return assetsPath + assetName;
}
