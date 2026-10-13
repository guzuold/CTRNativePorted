// SPDX-License-Identifier: GPL-2.0-only
// Own bounded Windows debugger: observes first-chance exceptions and process exit.
#include <Windows.h>
#include <cstdio>
#include <string>
#include <vector>
int wmain(int argc,wchar_t** argv) {
    if(argc<2)return 2;
    std::wstring command;for(int i=1;i<argc;++i){if(i>1)command+=L" ";command+=L"\""+std::wstring(argv[i])+L"\"";}
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    HANDLE output=CreateFileW(L"runtime/user/logs/debug-startup.log",GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE)return 3;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=output;startup.hStdError=output;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION p{};if(!CreateProcessW(argv[1],command.data(),nullptr,nullptr,TRUE,DEBUG_ONLY_THIS_PROCESS|CREATE_NO_WINDOW,nullptr,nullptr,&startup,&p)){std::printf("create_error=%lu\n",GetLastError());CloseHandle(output);return 3;}
    auto local=GetModuleHandleW(L"ntdll.dll");const auto offset=reinterpret_cast<ULONG_PTR>(GetProcAddress(local,"NtTerminateProcess"))-reinterpret_cast<ULONG_PTR>(local);
    ULONG_PTR trap=0;unsigned char original=0;bool initial=true,exited=false,installed=false;
    const auto deadline=GetTickCount64()+25000;
    while(GetTickCount64()<deadline){DEBUG_EVENT e{};if(!WaitForDebugEvent(&e,100))continue;DWORD disposition=DBG_CONTINUE;
        if(e.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){std::printf("image_base=0x%llx\n",reinterpret_cast<unsigned long long>(e.u.CreateProcessInfo.lpBaseOfImage));if(e.u.CreateProcessInfo.hFile)CloseHandle(e.u.CreateProcessInfo.hFile);}
        if(e.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT){wchar_t path[1024]{};auto f=e.u.LoadDll.hFile;
            if(f && GetFinalPathNameByHandleW(f,path,1024,0) && std::wstring(path).find(L"ntdll.dll")!=std::wstring::npos)trap=reinterpret_cast<ULONG_PTR>(e.u.LoadDll.lpBaseOfDll)+offset;
            if(f)CloseHandle(f);
        }
        if(e.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){const auto code=e.u.Exception.ExceptionRecord.ExceptionCode;const auto address=reinterpret_cast<ULONG_PTR>(e.u.Exception.ExceptionRecord.ExceptionAddress);
            HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,e.dwThreadId);CONTEXT c{};c.ContextFlags=CONTEXT_FULL;
            if(thread)GetThreadContext(thread,&c);
            if(code==EXCEPTION_BREAKPOINT && initial){initial=false;SIZE_T read=0;DWORD old=0;unsigned char breakpoint=0xcc;
                if(trap && ReadProcessMemory(p.hProcess,reinterpret_cast<void*>(trap),&original,1,&read) && read==1 && VirtualProtectEx(p.hProcess,reinterpret_cast<void*>(trap),1,PAGE_EXECUTE_READWRITE,&old)){
                    SIZE_T written=0;installed=WriteProcessMemory(p.hProcess,reinterpret_cast<void*>(trap),&breakpoint,1,&written)&&written==1;DWORD ignored=0;VirtualProtectEx(p.hProcess,reinterpret_cast<void*>(trap),1,old,&ignored);FlushInstructionCache(p.hProcess,reinterpret_cast<void*>(trap),1);
                }std::printf("termination_breakpoint_installed=%d address=0x%llx\n",installed,static_cast<unsigned long long>(trap));
            }else if(code==EXCEPTION_BREAKPOINT && installed && address==trap){
                std::printf("NtTerminateProcess thread=%lu handle=0x%llx status=0x%llx rsp=0x%llx\n",e.dwThreadId,c.Rcx,c.Rdx,c.Rsp);
                unsigned long long stack[20]{};SIZE_T read=0;ReadProcessMemory(p.hProcess,reinterpret_cast<void*>(c.Rsp),stack,sizeof(stack),&read);
                for(SIZE_T i=0;i<read/8;++i)std::printf("stack%llu=0x%llx\n",static_cast<unsigned long long>(i),stack[i]);
                SIZE_T written=0;WriteProcessMemory(p.hProcess,reinterpret_cast<void*>(trap),&original,1,&written);FlushInstructionCache(p.hProcess,reinterpret_cast<void*>(trap),1);c.Rip=trap;if(thread)SetThreadContext(thread,&c);installed=false;
            }else{std::printf("exception code=0x%lx address=0x%llx thread=%lu first=%lu\n",code,static_cast<unsigned long long>(address),e.dwThreadId,e.u.Exception.dwFirstChance);disposition=DBG_EXCEPTION_NOT_HANDLED;
                MEMORY_BASIC_INFORMATION memory{};VirtualQueryEx(p.hProcess,reinterpret_cast<void*>(address),&memory,sizeof(memory));
                std::printf("fault allocation=0x%llx offset=0x%llx rax=0x%llx rdi=0x%llx rsi=0x%llx rcx=0x%llx rdx=0x%llx rbp=0x%llx rsp=0x%llx access=%llu target=0x%llx\n",reinterpret_cast<unsigned long long>(memory.AllocationBase),static_cast<unsigned long long>(address-reinterpret_cast<ULONG_PTR>(memory.AllocationBase)),c.Rax,c.Rdi,c.Rsi,c.Rcx,c.Rdx,c.Rbp,c.Rsp,static_cast<unsigned long long>(e.u.Exception.ExceptionRecord.ExceptionInformation[0]),static_cast<unsigned long long>(e.u.Exception.ExceptionRecord.ExceptionInformation[1]));
                unsigned char bytes[24]{};SIZE_T read=0;ReadProcessMemory(p.hProcess,reinterpret_cast<void*>(address),bytes,sizeof(bytes),&read);std::printf("instruction_bytes=");for(SIZE_T i=0;i<read;++i)std::printf("%02x",bytes[i]);std::printf("\n");
            }
            if(thread)CloseHandle(thread);
        }
        if(e.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){std::printf("process_exit=0x%lx\n",e.u.ExitProcess.dwExitCode);exited=true;}
        ContinueDebugEvent(e.dwProcessId,e.dwThreadId,disposition);std::fflush(stdout);if(exited)break;
    }
    if(!exited){std::printf("debugger_deadline_kill=1\n");TerminateProcess(p.hProcess,87);WaitForSingleObject(p.hProcess,2000);}
    CloseHandle(p.hThread);CloseHandle(p.hProcess);CloseHandle(output);return exited?0:87;
}
