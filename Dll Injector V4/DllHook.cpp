#include "DllHook.h"
void WINAPI DllHook::SetHook()
{
	std::wstring szPath;
	LPSETHOOK lproc;
	HINSTANCE hDll;
	BOOL bRet;
	PROCESS_INFORMATION info;
	STARTUPINFO start;
	memset(&start, 0, sizeof(start));
	// 取得要载入的DLL文件名
	szPath = _T("C://Users//lisongqian//source//repos//Ldll//x64//Debug//Ldll.dll");
	// 载入DLL
	hDll = LoadLibraryW(szPath.c_str());
	if (hDll != NULL)
	{
		lproc = (LPSETHOOK)GetProcAddress(hDll, "SetHook");
		if (lproc != NULL)
		{
			// if ((*lproc)(0) == false) //全部进程
			// {
			// std::wstring message = L"Sethook";
			// ShowError(GetLastError(), const_cast<LPTSTR>(message.c_str()));
			// }
			// 因为没有适当的工具可以取得线程ＩＤ，也为了简单起见，所以这里新创建了一个记事本进程，以便取得它的线程ＩＤ，对其安装钩子，把我们的DLL注入到记事本进程中。
			std::wstring str = _T("C://Windows//System32//notepad.exe");
			bRet = CreateProcess(NULL,
				const_cast<wchar_t*>(str.c_str()),
				NULL,
				NULL,
				TRUE,
				0,
				NULL,
				NULL,
				&start,
				&info);
			if (bRet != 0)
			{
				if (((*lproc)(info.dwThreadId)) == false)
				{
					std::wstring message = L"Sethook";
					DllHook::ShowError(GetLastError(), const_cast<LPTSTR>(message.c_str()));
				}
			}
			else
			{
				std::wstring message = L"CreateProcess";
				DllHook::ShowError(GetLastError(), const_cast<LPTSTR>(message.c_str()));
			}
		}
	}
}
void WINAPI DllHook::UnSetHook()
{
	 std::wstring szPath;
	 LPSETHOOK lproc;
	 HINSTANCE hDll;

	 szPath = _T("C://Users//lisongqian//source//repos//Ldll//x64//Debug//Ldll.dll");
	 hDll = LoadLibrary(szPath.c_str());
	 if (hDll != NULL)
	 {
		 lproc = (LPSETHOOK)GetProcAddress(hDll, "SetHook");
		 if (lproc != NULL)
			 (*lproc)(0); //暂用进程号0作为结束
	 }
}
// DLL注入函数
BOOL WINAPI DllHook::LoadLib(DWORD dwProcessId, LPWSTR lpszLibName)
{
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;
	LPWSTR lpszRemoteFile = NULL;
	// 打开远程进程
	hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, dwProcessId);
	if (hProcess == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"OpenProcess"));
		return FALSE;
	}

	// 在远程进程中分配存贮DLL文件名的空间
	lpszRemoteFile = (LPWSTR)VirtualAllocEx(hProcess, NULL, sizeof(WCHAR) * lstrlenW(lpszLibName) + 1, MEM_COMMIT, PAGE_READWRITE);
	if (lpszRemoteFile == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"VirtualAllocEx"));
		return FALSE;
	}
	// 复制DLL文件名到远程刚分配的进程空间
	if (!WriteProcessMemory(hProcess, lpszRemoteFile, (PVOID)lpszLibName, sizeof(WCHAR) * lstrlenW(lpszLibName) + 1, NULL))
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"WriteProcessMemory"));
		return FALSE;
	}
	// 取得LoadLibrary函数在Kennel32.dll中的地址
	PTHREAD_START_ROUTINE pfnThreadRtn = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"Kernel32.dll"), "LoadLibraryW");
	if (pfnThreadRtn == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"GetProcAddress"));
		return FALSE;
	}
	// 创建远程线程 
	hThread = CreateRemoteThread(hProcess,
		NULL,
		0,
		pfnThreadRtn, // LoadLibrary地址
		lpszRemoteFile, // 要加载的DLL名
		0,
		NULL);
	if (hThread == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"CreateRemoteThread"));
		return FALSE;
	}
	// 等待线程返回 
	WaitForSingleObject(hThread, INFINITE);
	// 释放进程空间中的内存
	VirtualFreeEx(hProcess, lpszRemoteFile, 0, MEM_RELEASE);
	// 关闭句柄 
	CloseHandle(hThread);
	CloseHandle(hProcess);
	return TRUE;
}
// 在进程空间释放注入的DLL
BOOL WINAPI DllHook::FreeLib(DWORD dwProcessId, LPTSTR lpszLibName)
{
	HANDLE hProcess = NULL,
	       hThread = NULL,
	       hthSnapshot = NULL;
	MODULEENTRY32 hMod = {sizeof(hMod)};
	BOOL bFound = FALSE;
	// 取得指定进程的所有模块映象
	hthSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
	                                       dwProcessId);
	if (hthSnapshot == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"CreateToolhelp32Snapshot"));
		return FALSE;
	}
	// 取得所有模块列表中的指定的模块
	BOOL bMoreMods = Module32First(hthSnapshot, &hMod);
	if (bMoreMods == FALSE)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"Module32First"));
		return FALSE;
	}
	// 循环取得想要的模块
	for (; bMoreMods; bMoreMods = Module32Next(hthSnapshot, &hMod))
	{
		//ShowMessage(String(hMod.szExePath) + " | " + String(lpszLibName));
		if ((wcscmp(hMod.szExePath, lpszLibName) == 0) ||
			(wcscmp(hMod.szModule, lpszLibName) == 0))
		{
			bFound = TRUE;
			break;
		}
	}
	if (!bFound)
	{
		MessageBox(nullptr, L"模块不存在", L"", MB_OK);
		CloseHandle(hthSnapshot);
		return FALSE;
	}
	// 打开进程
	hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
	                       FALSE, dwProcessId);
	if (hProcess == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"OpenProcess"));
		return FALSE;
	}
	// 取得FreeLibrary函数在Kernel32.dll中的地址
	PTHREAD_START_ROUTINE pfnThreadRtn =
		(PTHREAD_START_ROUTINE)GetProcAddress(
			GetModuleHandle(L"Kernel32.dll"), "FreeLibrary");
	if (pfnThreadRtn == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"GetProcAddress"));
		return FALSE;
	}
	// 创建远程线程来执行FreeLibrary函数
	hThread = CreateRemoteThread(hProcess,
	                             NULL,
	                             0,
	                             pfnThreadRtn,
	                             hMod.modBaseAddr,
	                             0,
	                             NULL);
	if (hThread == NULL)
	{
		ShowError(GetLastError(), const_cast<LPTSTR>(L"CreateRemoteThread"));
		return FALSE;
	}
	// 等待线程返回
	WaitForSingleObject(hThread, INFINITE);
	// 关闭句柄
	CloseHandle(hThread);
	CloseHandle(hthSnapshot);
	CloseHandle(hProcess);
	return TRUE;
}
// 获取进程ID
DWORD DllHook::GetProcessId(LPCWSTR lpName, std::wstring& errMsg)
{
	DWORD dwPid = 0;
	HANDLE hProcess = NULL;
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	// Take a snapshot of all processes in the system.
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE)
	{
		errMsg = L"Error: CreateToolhelp32Snapshot (of processes)";
		return 0;
	}

	// Set the size of the structure before using it.
	pe32.dwSize = sizeof(PROCESSENTRY32);
	// Retrieve information about the first process,
	// and exit if unsuccessful
	if (!Process32First(hProcessSnap, &pe32))
	{
		errMsg = L"Error: Process32First";
		//printf("Error: Process32First\r\n"); // show cause of failure
		CloseHandle(hProcessSnap); // clean the snapshot object
		return 0;
	}

	// Now walk the snapshot of processes, and
	// display information about each process in turn
	int namelen = 200;
	char name[201] = { 0 };
	do
	{
		if (!wcscmp(pe32.szExeFile, lpName))
		{
			dwPid = pe32.th32ProcessID;
			break;
		}
	} while (Process32Next(hProcessSnap, &pe32));
	CloseHandle(hProcessSnap);
	return dwPid;
}
// 显示错误信息
void DllHook::ShowError(DWORD dwErrNo, LPTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwErrNo,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(
			TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dwErrNo, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	//    ExitProcess(dwErrNo);
}