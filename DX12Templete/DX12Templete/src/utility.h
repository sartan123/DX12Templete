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

static std::wstring GetAssetFullPath(std::wstring assetsPath, std::wstring assetName)
{
	return assetsPath + assetName;
}

static std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

static void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}