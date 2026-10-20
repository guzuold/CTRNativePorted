; SPDX-License-Identifier: GPL-2.0-only
; SysV integer/pointer arguments to Win64. Preserve SysV nonvolatile registers.
EXTERN ctr_kernel_process_time:PROC
EXTERN ctr_kernel_gettimeofday:PROC
EXTERN ctr_kernel_mutex_lock:PROC
EXTERN ctr_kernel_mutex_unlock:PROC
EXTERN ctr_kernel_condition_broadcast:PROC
EXTERN ctr_orbis_condition_broadcast:PROC
EXTERN ctr_kernel_mutex_attributes_init:PROC
EXTERN ctr_kernel_mutex_attributes_settype:PROC
EXTERN ctr_kernel_mutex_init:PROC
EXTERN ctr_kernel_mutex_attributes_destroy:PROC
EXTERN ctr_json2_stub:PROC
.code
PUBLIC ctr_sysv_gettimeofday
ctr_sysv_gettimeofday PROC
    mov r8, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    mov rdx, rsi
    call ctr_kernel_gettimeofday
    add rsp, 40
    ret
ctr_sysv_gettimeofday ENDP
PUBLIC ctr_sysv_mutex_lock
ctr_sysv_mutex_lock PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_kernel_mutex_lock
    add rsp, 40
    ret
ctr_sysv_mutex_lock ENDP
PUBLIC ctr_sysv_mutex_unlock
ctr_sysv_mutex_unlock PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_kernel_mutex_unlock
    add rsp, 40
    ret
ctr_sysv_mutex_unlock ENDP
PUBLIC ctr_sysv_condition_broadcast
ctr_sysv_condition_broadcast PROC
    mov r8, rbp
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_kernel_condition_broadcast
    add rsp, 40
    ret
ctr_sysv_condition_broadcast ENDP
PUBLIC ctr_sysv_orbis_condition_broadcast
ctr_sysv_orbis_condition_broadcast PROC
    mov r8, rbp
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_orbis_condition_broadcast
    add rsp, 40
    ret
ctr_sysv_orbis_condition_broadcast ENDP
PUBLIC ctr_sysv_mutex_attributes_init
ctr_sysv_mutex_attributes_init PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_kernel_mutex_attributes_init
    add rsp, 40
    ret
ctr_sysv_mutex_attributes_init ENDP
EXTERN ctr_posix_mutexattr_init:PROC
PUBLIC ctr_sysv_posix_mutexattr_init
ctr_sysv_posix_mutexattr_init PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_posix_mutexattr_init
    add rsp, 40
    ret
ctr_sysv_posix_mutexattr_init ENDP
EXTERN ctr_posix_mutex_trylock:PROC
PUBLIC ctr_sysv_posix_mutex_trylock
ctr_sysv_posix_mutex_trylock PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_posix_mutex_trylock
    add rsp, 40
    ret
ctr_sysv_posix_mutex_trylock ENDP
EXTERN ctr_orbis_mutex_trylock:PROC
PUBLIC ctr_sysv_orbis_mutex_trylock
ctr_sysv_orbis_mutex_trylock PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_orbis_mutex_trylock
    add rsp, 40
    ret
ctr_sysv_orbis_mutex_trylock ENDP
EXTERN ctr_posix_mutexattr_destroy:PROC
PUBLIC ctr_sysv_posix_mutexattr_destroy
ctr_sysv_posix_mutexattr_destroy PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_posix_mutexattr_destroy
    add rsp, 40
    ret
ctr_sysv_posix_mutexattr_destroy ENDP
PUBLIC ctr_sysv_mutex_attributes_settype
ctr_sysv_mutex_attributes_settype PROC
    mov r8, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    mov edx, esi
    call ctr_kernel_mutex_attributes_settype
    add rsp, 40
    ret
ctr_sysv_mutex_attributes_settype ENDP
PUBLIC ctr_sysv_mutex_init
ctr_sysv_mutex_init PROC
    mov r9, qword ptr [rsp]
    sub rsp, 40
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    call ctr_kernel_mutex_init
    add rsp, 40
    ret
ctr_sysv_mutex_init ENDP
PUBLIC ctr_sysv_mutex_attributes_destroy
ctr_sysv_mutex_attributes_destroy PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_kernel_mutex_attributes_destroy
    add rsp, 40
    ret
ctr_sysv_mutex_attributes_destroy ENDP
EXTERN ctr_random_bytes:PROC
PUBLIC ctr_sysv_random_bytes
ctr_sysv_random_bytes PROC
    mov r8, qword ptr [rsp]
    sub rsp, 40
    mov rcx, rdi
    mov rdx, rsi
    call ctr_random_bytes
    add rsp, 40
    ret
ctr_sysv_random_bytes ENDP
EXTERN ctr_sysmodule_load:PROC
PUBLIC ctr_sysv_sysmodule_load
ctr_sysv_sysmodule_load PROC
    mov rdx, qword ptr [rsp]
    sub rsp, 40
    movzx ecx, di
    call ctr_sysmodule_load
    add rsp, 40
    ret
ctr_sysv_sysmodule_load ENDP
EXTERN ctr_direct_memory_size:PROC
PUBLIC ctr_sysv_direct_memory_size
ctr_sysv_direct_memory_size PROC
    mov rcx, qword ptr [rsp]
    sub rsp, 40
    call ctr_direct_memory_size
    add rsp, 40
    ret
ctr_sysv_direct_memory_size ENDP
EXTERN ctr_direct_memory_query:PROC
PUBLIC ctr_sysv_direct_memory_query
ctr_sysv_direct_memory_query PROC
    mov rax, qword ptr [rsp]
    sub rsp, 56
    mov qword ptr [rsp+32], rax
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov edx, esi
    call ctr_direct_memory_query
    add rsp, 56
    ret
ctr_sysv_direct_memory_query ENDP
EXTERN ctr_thread_destructor_set:PROC
EXTERN ctr_dtor_control_increment:PROC
PUBLIC ctr_sysv_thread_destructor_set
ctr_sysv_thread_destructor_set PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_thread_destructor_set
    add rsp, 40
    ret
ctr_sysv_thread_destructor_set ENDP
PUBLIC ctr_sysv_dtor_control
ctr_sysv_dtor_control PROC
    sub rsp, 40
    call ctr_dtor_control_increment
    add rsp, 40
    ret
ctr_sysv_dtor_control ENDP
EXTERN ctr_thread_atexit_set:PROC
PUBLIC ctr_sysv_thread_atexit_set
ctr_sysv_thread_atexit_set PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_thread_atexit_set
    add rsp, 40
    ret
ctr_sysv_thread_atexit_set ENDP
PUBLIC ctr_sysv_atexit_control
ctr_sysv_atexit_control PROC
    mov eax, edi
    ret
ctr_sysv_atexit_control ENDP
EXTERN ctr_thread_atexit_report_set:PROC
EXTERN ctr_atexit_report_capture:PROC
PUBLIC ctr_sysv_thread_atexit_report_set
ctr_sysv_thread_atexit_report_set PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_thread_atexit_report_set
    add rsp, 40
    ret
ctr_sysv_thread_atexit_report_set ENDP
PUBLIC ctr_sysv_atexit_report_control
ctr_sysv_atexit_report_control PROC
    mov ecx, edi
    sub rsp, 40
    call ctr_atexit_report_capture
    add rsp, 40
    ret
ctr_sysv_atexit_report_control ENDP
EXTERN ctr_get_proc_param:PROC
PUBLIC ctr_sysv_get_proc_param
ctr_sysv_get_proc_param PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_get_proc_param
    add rsp, 40
    ret
ctr_sysv_get_proc_param ENDP
EXTERN ctr_application_heap_set:PROC
EXTERN malloc:PROC
EXTERN free:PROC
PUBLIC ctr_sysv_application_heap_set
ctr_sysv_application_heap_set PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_application_heap_set
    add rsp, 40
    ret
ctr_sysv_application_heap_set ENDP
PUBLIC ctr_sysv_heap_malloc_control
ctr_sysv_heap_malloc_control PROC
    mov rcx, rdi
    sub rsp, 40
    call malloc
    add rsp, 40
    ret
ctr_sysv_heap_malloc_control ENDP
PUBLIC ctr_sysv_heap_free_control
ctr_sysv_heap_free_control PROC
    mov rcx, rdi
    sub rsp, 40
    call free
    add rsp, 40
    ret
ctr_sysv_heap_free_control ENDP
EXTERN ctr_heap_trace_query:PROC
PUBLIC ctr_sysv_heap_trace_query
ctr_sysv_heap_trace_query PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_heap_trace_query
    add rsp, 40
    ret
ctr_sysv_heap_trace_query ENDP
EXTERN ctr_is_neo_mode:PROC
PUBLIC ctr_sysv_is_neo_mode
ctr_sysv_is_neo_mode PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_is_neo_mode
    add rsp, 40
    ret
ctr_sysv_is_neo_mode ENDP
EXTERN ctr_allocate_direct:PROC
PUBLIC ctr_sysv_allocate_direct
ctr_sysv_allocate_direct PROC
    mov r10, [rsp]
    sub rsp, 72
    mov [rsp+32], r8
    mov [rsp+40], r9
    mov [rsp+48], r10
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_allocate_direct
    add rsp, 72
    ret
ctr_sysv_allocate_direct ENDP
EXTERN ctr_map_direct:PROC
PUBLIC ctr_sysv_map_direct
ctr_sysv_map_direct PROC
    mov r10, [rsp]
    sub rsp, 72
    mov [rsp+32], r8
    mov [rsp+40], r9
    mov [rsp+48], r10
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_map_direct
    add rsp, 72
    ret
ctr_sysv_map_direct ENDP
EXTERN ctr_asan_enabled:PROC
PUBLIC ctr_sysv_asan_enabled
ctr_sysv_asan_enabled PROC
    sub rsp, 40
    call ctr_asan_enabled
    add rsp, 40
    ret
ctr_sysv_asan_enabled ENDP
EXTERN ctr_orbis_mutex_lock:PROC
PUBLIC ctr_sysv_orbis_mutex_lock
ctr_sysv_orbis_mutex_lock PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_orbis_mutex_lock
    add rsp, 40
    ret
ctr_sysv_orbis_mutex_lock ENDP
EXTERN ctr_orbis_mutex_unlock:PROC
PUBLIC ctr_sysv_orbis_mutex_unlock
ctr_sysv_orbis_mutex_unlock PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_orbis_mutex_unlock
    add rsp, 40
    ret
ctr_sysv_orbis_mutex_unlock ENDP
EXTERN ctr_thread_attributes_init:PROC
PUBLIC ctr_sysv_thread_attributes_init
ctr_sysv_thread_attributes_init PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_thread_attributes_init
    add rsp, 40
    ret
ctr_sysv_thread_attributes_init ENDP
EXTERN ctr_thread_self:PROC
PUBLIC ctr_sysv_thread_self
ctr_sysv_thread_self PROC
    sub rsp, 40
    call ctr_thread_self
    add rsp, 40
    ret
ctr_sysv_thread_self ENDP
EXTERN ctr_thread_attributes_current:PROC
PUBLIC ctr_sysv_thread_attributes_current
ctr_sysv_thread_attributes_current PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_attributes_current
    add rsp, 40
    ret
ctr_sysv_thread_attributes_current ENDP
EXTERN ctr_thread_attributes_affinity:PROC
PUBLIC ctr_sysv_thread_attributes_affinity
ctr_sysv_thread_attributes_affinity PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_attributes_affinity
    add rsp, 40
    ret
ctr_sysv_thread_attributes_affinity ENDP
EXTERN ctr_thread_attributes_destroy:PROC
PUBLIC ctr_sysv_thread_attributes_destroy
ctr_sysv_thread_attributes_destroy PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_thread_attributes_destroy
    add rsp, 40
    ret
ctr_sysv_thread_attributes_destroy ENDP
EXTERN ctr_thread_key_create:PROC
PUBLIC ctr_sysv_thread_key_create
ctr_sysv_thread_key_create PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_key_create
    add rsp, 40
    ret
ctr_sysv_thread_key_create ENDP
EXTERN ctr_thread_key_get:PROC
PUBLIC ctr_sysv_thread_key_get
ctr_sysv_thread_key_get PROC
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_key_get
    add rsp, 40
    ret
ctr_sysv_thread_key_get ENDP
EXTERN ctr_thread_key_set:PROC
PUBLIC ctr_sysv_thread_key_set
ctr_sysv_thread_key_set PROC
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_key_set
    add rsp, 40
    ret
ctr_sysv_thread_key_set ENDP
EXTERN ctr_thread_key_delete:PROC
PUBLIC ctr_sysv_thread_key_delete
ctr_sysv_thread_key_delete PROC
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_key_delete
    add rsp, 40
    ret
ctr_sysv_thread_key_delete ENDP
EXTERN ctr_cpu_mode:PROC
PUBLIC ctr_sysv_cpu_mode
ctr_sysv_cpu_mode PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_cpu_mode
    add rsp, 40
    ret
ctr_sysv_cpu_mode ENDP
EXTERN ctr_thread_set_affinity:PROC
PUBLIC ctr_sysv_thread_set_affinity
ctr_sysv_thread_set_affinity PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_set_affinity
    add rsp, 40
    ret
ctr_sysv_thread_set_affinity ENDP
EXTERN ctr_tsc_frequency:PROC
PUBLIC ctr_sysv_tsc_frequency
ctr_sysv_tsc_frequency PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_tsc_frequency
    add rsp, 40
    ret
ctr_sysv_tsc_frequency ENDP
EXTERN ctr_read_tsc:PROC
PUBLIC ctr_sysv_read_tsc
ctr_sysv_read_tsc PROC
    sub rsp, 40
    call ctr_read_tsc
    add rsp, 40
    ret
ctr_sysv_read_tsc ENDP
EXTERN ctr_user_service_initialize:PROC
PUBLIC ctr_sysv_user_service_initialize
ctr_sysv_user_service_initialize PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_user_service_initialize
    add rsp, 40
    ret
ctr_sysv_user_service_initialize ENDP
EXTERN ctr_semaphore_create:PROC
PUBLIC ctr_sysv_semaphore_create
ctr_sysv_semaphore_create PROC
    mov r10, rcx
    mov r11, rdx
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], r9
    mov rax, [rsp+56]
    mov [rsp+48], rax
    mov rcx, rdi
    mov rdx, rsi
    mov r8, r11
    mov r9, r10
    call ctr_semaphore_create
    add rsp, 56
    ret
ctr_sysv_semaphore_create ENDP
EXTERN ctr_semaphore_wait:PROC
PUBLIC ctr_sysv_semaphore_wait
ctr_sysv_semaphore_wait PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_semaphore_wait
    add rsp, 40
    ret
ctr_sysv_semaphore_wait ENDP
EXTERN ctr_system_parameter:PROC
PUBLIC ctr_sysv_system_parameter
ctr_sysv_system_parameter PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_system_parameter
    add rsp, 40
    ret
ctr_sysv_system_parameter ENDP
EXTERN ctr_posix_mutex_init:PROC
PUBLIC ctr_sysv_posix_mutex_init
ctr_sysv_posix_mutex_init PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_posix_mutex_init
    add rsp, 40
    ret
ctr_sysv_posix_mutex_init ENDP
EXTERN ctr_posix_mutexattr_settype:PROC
PUBLIC ctr_sysv_posix_mutexattr_settype
ctr_sysv_posix_mutexattr_settype PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_posix_mutexattr_settype
    add rsp, 40
    ret
ctr_sysv_posix_mutexattr_settype ENDP
EXTERN ctr_net_toolkit_register:PROC
PUBLIC ctr_sysv_net_toolkit_register
ctr_sysv_net_toolkit_register PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_net_toolkit_register
    add rsp, 40
    ret
ctr_sysv_net_toolkit_register ENDP
EXTERN ctr_user_callback_register:PROC
PUBLIC ctr_sysv_user_callback_register
ctr_sysv_user_callback_register PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_user_callback_register
    add rsp, 40
    ret
ctr_sysv_user_callback_register ENDP
EXTERN ctr_np_toolkit_register:PROC
PUBLIC ctr_sysv_np_toolkit_register
ctr_sysv_np_toolkit_register PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_np_toolkit_register
    add rsp, 40
    ret
ctr_sysv_np_toolkit_register ENDP
EXTERN ctr_np_toolkit_unregister:PROC
PUBLIC ctr_sysv_np_toolkit_unregister
ctr_sysv_np_toolkit_unregister PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_np_toolkit_unregister
    add rsp, 40
    ret
ctr_sysv_np_toolkit_unregister ENDP
EXTERN ctr_net_toolkit_check:PROC
PUBLIC ctr_sysv_net_toolkit_check
ctr_sysv_net_toolkit_check PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_net_toolkit_check
    add rsp, 40
    ret
ctr_sysv_net_toolkit_check ENDP
PUBLIC ctr_sysv_offline_callback_fixture
ctr_sysv_offline_callback_fixture PROC
    mov [rsi], edi
    inc DWORD PTR [rsi+4]
    xor eax, eax
    ret
ctr_sysv_offline_callback_fixture ENDP
EXTERN ctr_condition_signal:PROC
PUBLIC ctr_sysv_condition_signal
ctr_sysv_condition_signal PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_condition_signal
    add rsp, 40
    ret
ctr_sysv_condition_signal ENDP
EXTERN ctr_np_toolkit_check:PROC
PUBLIC ctr_sysv_np_toolkit_check
ctr_sysv_np_toolkit_check PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_np_toolkit_check
    add rsp, 40
    ret
ctr_sysv_np_toolkit_check ENDP
PUBLIC ctr_sysv_np_callback_fixture
ctr_sysv_np_callback_fixture PROC
    mov [rdx], edi
    mov [rdx+4], esi
    inc DWORD PTR [rdx+8]
    xor eax, eax
    ret
ctr_sysv_np_callback_fixture ENDP
EXTERN ctr_netctl_info:PROC
PUBLIC ctr_sysv_netctl_info
ctr_sysv_netctl_info PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_netctl_info
    add rsp, 40
    ret
ctr_sysv_netctl_info ENDP
EXTERN ctr_convert_local:PROC
PUBLIC ctr_sysv_convert_local
ctr_sysv_convert_local PROC
    mov r10, [rsp]
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    mov [rsp+32], r10
    call ctr_convert_local
    add rsp, 40
    ret
ctr_sysv_convert_local ENDP
EXTERN ctr_posix_clock:PROC
PUBLIC ctr_sysv_posix_clock
ctr_sysv_posix_clock PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_posix_clock
    add rsp, 40
    ret
ctr_sysv_posix_clock ENDP
EXTERN ctr_semaphore_poll:PROC
PUBLIC ctr_sysv_semaphore_poll
ctr_sysv_semaphore_poll PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_semaphore_poll
    add rsp, 40
    ret
ctr_sysv_semaphore_poll ENDP
EXTERN ctr_semaphore_signal:PROC
PUBLIC ctr_sysv_semaphore_signal
ctr_sysv_semaphore_signal PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_semaphore_signal
    add rsp, 40
    ret
ctr_sysv_semaphore_signal ENDP
EXTERN ctr_common_dialog_initialize:PROC
PUBLIC ctr_sysv_common_dialog_initialize
ctr_sysv_common_dialog_initialize PROC
    sub rsp, 40
    call ctr_common_dialog_initialize
    add rsp, 40
    ret
ctr_sysv_common_dialog_initialize ENDP
EXTERN ctr_save_dialog_initialize:PROC
PUBLIC ctr_sysv_save_dialog_initialize
ctr_sysv_save_dialog_initialize PROC
    sub rsp, 40
    call ctr_save_dialog_initialize
    add rsp, 40
    ret
ctr_sysv_save_dialog_initialize ENDP
EXTERN ctr_save_dialog_status:PROC
PUBLIC ctr_sysv_save_dialog_status
ctr_sysv_save_dialog_status PROC
    sub rsp, 40
    call ctr_save_dialog_status
    add rsp, 40
    ret
ctr_sysv_save_dialog_status ENDP
EXTERN ctr_save_dialog_terminate:PROC
PUBLIC ctr_sysv_save_dialog_terminate
ctr_sysv_save_dialog_terminate PROC
    sub rsp, 40
    call ctr_save_dialog_terminate
    add rsp, 40
    ret
ctr_sysv_save_dialog_terminate ENDP
EXTERN ctr_condition_attributes_init:PROC
PUBLIC ctr_sysv_condition_attributes_init
ctr_sysv_condition_attributes_init PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_condition_attributes_init
    add rsp, 40
    ret
ctr_sysv_condition_attributes_init ENDP
EXTERN ctr_condition_init:PROC
PUBLIC ctr_sysv_condition_init
ctr_sysv_condition_init PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_condition_init
    add rsp, 40
    ret
ctr_sysv_condition_init ENDP
EXTERN ctr_condition_attributes_destroy:PROC
PUBLIC ctr_sysv_condition_attributes_destroy
ctr_sysv_condition_attributes_destroy PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_condition_attributes_destroy
    add rsp, 40
    ret
ctr_sysv_condition_attributes_destroy ENDP
EXTERN ctr_thread_stack_size:PROC
PUBLIC ctr_sysv_thread_stack_size
ctr_sysv_thread_stack_size PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_stack_size
    add rsp, 40
    ret
ctr_sysv_thread_stack_size ENDP
EXTERN ctr_thread_policy:PROC
PUBLIC ctr_sysv_thread_policy
ctr_sysv_thread_policy PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov edx, esi
    sub rsp, 40
    call ctr_thread_policy
    add rsp, 40
    ret
ctr_sysv_thread_policy ENDP
EXTERN ctr_thread_priority:PROC
PUBLIC ctr_sysv_thread_priority
ctr_sysv_thread_priority PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_priority
    add rsp, 40
    ret
ctr_sysv_thread_priority ENDP
EXTERN ctr_thread_inherit:PROC
PUBLIC ctr_sysv_thread_inherit
ctr_sysv_thread_inherit PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov edx, esi
    sub rsp, 40
    call ctr_thread_inherit
    add rsp, 40
    ret
ctr_sysv_thread_inherit ENDP
EXTERN ctr_tls_address:PROC
PUBLIC ctr_sysv_tls_address
ctr_sysv_tls_address PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_tls_address
    add rsp, 40
    ret
ctr_sysv_tls_address ENDP
EXTERN ctr_guest_errno:PROC
PUBLIC ctr_sysv_guest_errno
ctr_sysv_guest_errno PROC
    sub rsp, 40
    call ctr_guest_errno
    add rsp, 40
    ret
ctr_sysv_guest_errno ENDP
EXTERN ctr_thread_create:PROC
PUBLIC ctr_sysv_thread_create
ctr_sysv_thread_create PROC
    mov r10, rdx
    mov r11, rcx
    mov rax, [rsp]
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], rax
    mov rcx, rdi
    mov rdx, rsi
    mov r8, r10
    mov r9, r11
    call ctr_thread_create
    add rsp, 56
    ret
ctr_sysv_thread_create ENDP
EXTERN ctr_condition_relative_missing:PROC
PUBLIC ctr_sysv_condition_relative_missing
ctr_sysv_condition_relative_missing PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_condition_relative_missing
    add rsp, 40
    ret
ctr_sysv_condition_relative_missing ENDP
PUBLIC ctr_condition_relative_fixture
PUBLIC ctr_condition_relative_caller
ctr_condition_relative_fixture PROC
    sub rsp, 8
    call ctr_sysv_condition_relative_missing
ctr_condition_relative_caller LABEL BYTE
    add rsp, 8
    ret
ctr_condition_relative_fixture ENDP
EXTERN ctr_condition_wait_missing:PROC
PUBLIC ctr_sysv_condition_wait_missing
ctr_sysv_condition_wait_missing PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_condition_wait_missing
    add rsp, 40
    ret
ctr_sysv_condition_wait_missing ENDP
PUBLIC ctr_condition_wait_fixture
PUBLIC ctr_condition_wait_caller
ctr_condition_wait_fixture PROC
    sub rsp, 8
    call ctr_sysv_condition_wait_missing
ctr_condition_wait_caller LABEL BYTE
    add rsp, 8
    ret
ctr_condition_wait_fixture ENDP
EXTERN ctr_worker_self_join_control:PROC
PUBLIC ctr_worker_self_join_fixture
ctr_worker_self_join_fixture PROC
    mov rcx, rdi
    sub rsp, 40
    call ctr_worker_self_join_control
    add rsp, 40
    ret
ctr_worker_self_join_fixture ENDP
EXTERN ctr_thread_detach_owned:PROC
PUBLIC ctr_sysv_thread_detach_owned
ctr_sysv_thread_detach_owned PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_thread_detach_owned
    add rsp, 40
    ret
ctr_sysv_thread_detach_owned ENDP

EXTERN ctr_worker_join_detach_gate:PROC
PUBLIC ctr_worker_join_detach_fixture
ctr_worker_join_detach_fixture PROC
    mov rcx, rdi
    sub rsp, 40
    call ctr_worker_join_detach_gate
    add rsp, 40
    ret
ctr_worker_join_detach_fixture ENDP

EXTERN ctr_posix_write:PROC
PUBLIC ctr_sysv_posix_write
ctr_sysv_posix_write PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_posix_write
    add rsp, 40
    ret
ctr_sysv_posix_write ENDP
EXTERN ctr_console_write:PROC
PUBLIC ctr_sysv_console_write
ctr_sysv_console_write PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_console_write
    add rsp, 40
    ret
ctr_sysv_console_write ENDP
EXTERN ctr_console_debug:PROC
PUBLIC ctr_sysv_console_debug
ctr_sysv_console_debug PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_console_debug
    add rsp, 40
    ret
ctr_sysv_console_debug ENDP

EXTERN ctr_save_data_initialize3:PROC
PUBLIC ctr_sysv_save_data_initialize3
ctr_sysv_save_data_initialize3 PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_save_data_initialize3
    add rsp, 40
    ret
ctr_sysv_save_data_initialize3 ENDP

EXTERN ctr_user_login_list:PROC
PUBLIC ctr_sysv_user_login_list
ctr_sysv_user_login_list PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_user_login_list
    add rsp, 40
    ret
ctr_sysv_user_login_list ENDP

EXTERN ctr_signal_mask:PROC
PUBLIC ctr_sysv_signal_mask
ctr_sysv_signal_mask PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_signal_mask
    add rsp, 40
    ret
ctr_sysv_signal_mask ENDP

EXTERN ctr_thread_numeric_id:PROC
PUBLIC ctr_sysv_thread_numeric_id
ctr_sysv_thread_numeric_id PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_thread_numeric_id
    add rsp, 40
    ret
ctr_sysv_thread_numeric_id ENDP

EXTERN ctr_event_flag_create:PROC
PUBLIC ctr_sysv_event_flag_create
ctr_sysv_event_flag_create PROC
    mov r10, rdx
    mov r11, rcx
    mov rax, [rsp]
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], rax
    mov rcx, rdi
    mov rdx, rsi
    mov r8, r10
    mov r9, r11
    call ctr_event_flag_create
    add rsp, 56
    ret
ctr_sysv_event_flag_create ENDP
EXTERN ctr_attr_set_affinity:PROC
PUBLIC ctr_sysv_attr_set_affinity
ctr_sysv_attr_set_affinity PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_attr_set_affinity
    add rsp, 40
    ret
ctr_sysv_attr_set_affinity ENDP
EXTERN ctr_attr_set_detach:PROC
PUBLIC ctr_sysv_attr_set_detach
ctr_sysv_attr_set_detach PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_attr_set_detach
    add rsp, 40
    ret
ctr_sysv_attr_set_detach ENDP
EXTERN ctr_event_flag_wait:PROC
PUBLIC ctr_sysv_event_flag_wait
ctr_sysv_event_flag_wait PROC
    mov r10, rdx
    mov r11, rcx
    mov rax, [rsp]
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], rax
    mov rcx, rdi
    mov rdx, rsi
    mov r8, r10
    mov r9, r11
    call ctr_event_flag_wait
    add rsp, 56
    ret
ctr_sysv_event_flag_wait ENDP
EXTERN ctr_event_flag_set:PROC
PUBLIC ctr_sysv_event_flag_set
ctr_sysv_event_flag_set PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_event_flag_set
    add rsp, 40
    ret
ctr_sysv_event_flag_set ENDP
EXTERN ctr_thread_yield:PROC
PUBLIC ctr_sysv_thread_yield
ctr_sysv_thread_yield PROC
    sub rsp, 40
    call ctr_thread_yield
    add rsp, 40
    ret
ctr_sysv_thread_yield ENDP
EXTERN ctr_thread_join_owned:PROC
PUBLIC ctr_sysv_thread_join_owned
ctr_sysv_thread_join_owned PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_join_owned
    add rsp, 40
    ret
ctr_sysv_thread_join_owned ENDP
EXTERN ctr_condition_wait_owned:PROC
PUBLIC ctr_sysv_condition_wait_owned
ctr_sysv_condition_wait_owned PROC
    mov r8, [rsp]
    mov r9, rbp
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_condition_wait_owned
    add rsp, 40
    ret
ctr_sysv_condition_wait_owned ENDP
EXTERN ctr_kernel_open:PROC
PUBLIC ctr_sysv_kernel_open
ctr_sysv_kernel_open PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_kernel_open
    add rsp, 40
    ret
ctr_sysv_kernel_open ENDP
EXTERN ctr_app_content_initialize:PROC
PUBLIC ctr_sysv_app_content_initialize
ctr_sysv_app_content_initialize PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_app_content_initialize
    add rsp, 40
    ret
ctr_sysv_app_content_initialize ENDP
EXTERN ctr_app_content_get_int:PROC
PUBLIC ctr_sysv_app_content_get_int
ctr_sysv_app_content_get_int PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_app_content_get_int
    add rsp, 40
    ret
ctr_sysv_app_content_get_int ENDP
EXTERN ctr_playgo_get_todo:PROC
PUBLIC ctr_sysv_playgo_get_todo
ctr_sysv_playgo_get_todo PROC
    mov r10, [rsp]
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    mov [rsp+32], r10
    call ctr_playgo_get_todo
    add rsp, 40
    ret
ctr_sysv_playgo_get_todo ENDP
EXTERN ctr_playgo_todo:PROC
PUBLIC ctr_sysv_playgo_todo
ctr_sysv_playgo_todo PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_playgo_todo
    add rsp, 40
    ret
ctr_sysv_playgo_todo ENDP
EXTERN ctr_playgo_locus:PROC
PUBLIC ctr_sysv_playgo_locus
ctr_sysv_playgo_locus PROC
    mov r10, [rsp]
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    mov [rsp+32], r10
    call ctr_playgo_locus
    add rsp, 40
    ret
ctr_sysv_playgo_locus ENDP
EXTERN ctr_playgo_get_speed:PROC
PUBLIC ctr_sysv_playgo_get_speed
ctr_sysv_playgo_get_speed PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_playgo_get_speed
    add rsp, 40
    ret
ctr_sysv_playgo_get_speed ENDP
EXTERN ctr_playgo_set_speed:PROC
PUBLIC ctr_sysv_playgo_set_speed
ctr_sysv_playgo_set_speed PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_playgo_set_speed
    add rsp, 40
    ret
ctr_sysv_playgo_set_speed ENDP
EXTERN ctr_playgo_init:PROC
PUBLIC ctr_sysv_playgo_init
ctr_sysv_playgo_init PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_playgo_init
    add rsp, 40
    ret
ctr_sysv_playgo_init ENDP
EXTERN ctr_playgo_open:PROC
PUBLIC ctr_sysv_playgo_open
ctr_sysv_playgo_open PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_playgo_open
    add rsp, 40
    ret
ctr_sysv_playgo_open ENDP
EXTERN ctr_playgo_chunks:PROC
PUBLIC ctr_sysv_playgo_chunks
ctr_sysv_playgo_chunks PROC
    mov r10, [rsp]
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    mov [rsp+32], r10
    call ctr_playgo_chunks
    add rsp, 40
    ret
ctr_sysv_playgo_chunks ENDP
EXTERN ctr_playgo_close:PROC
PUBLIC ctr_sysv_playgo_close
ctr_sysv_playgo_close PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_playgo_close
    add rsp, 40
    ret
ctr_sysv_playgo_close ENDP
EXTERN ctr_playgo_term:PROC
PUBLIC ctr_sysv_playgo_term
ctr_sysv_playgo_term PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_playgo_term
    add rsp, 40
    ret
ctr_sysv_playgo_term ENDP
EXTERN ctr_kernel_stat:PROC
PUBLIC ctr_sysv_kernel_stat
ctr_sysv_kernel_stat PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_kernel_stat
    add rsp, 40
    ret
ctr_sysv_kernel_stat ENDP
EXTERN ctr_kernel_fstat:PROC
PUBLIC ctr_sysv_kernel_fstat
ctr_sysv_kernel_fstat PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_kernel_fstat
    add rsp, 40
    ret
ctr_sysv_kernel_fstat ENDP
EXTERN ctr_kernel_pread:PROC
PUBLIC ctr_sysv_kernel_pread
ctr_sysv_kernel_pread PROC
    mov r10, [rsp]
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    mov [rsp+32], r10
    call ctr_kernel_pread
    add rsp, 40
    ret
ctr_sysv_kernel_pread ENDP
EXTERN ctr_kernel_lseek:PROC
PUBLIC ctr_sysv_kernel_lseek
ctr_sysv_kernel_lseek PROC
    mov r9, [rsp]
    mov r8d, edx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_kernel_lseek
    add rsp, 40
    ret
ctr_sysv_kernel_lseek ENDP
EXTERN ctr_kernel_pwrite:PROC
PUBLIC ctr_sysv_kernel_pwrite
ctr_sysv_kernel_pwrite PROC
    mov r10, [rsp]
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    mov [rsp+32], r10
    call ctr_kernel_pwrite
    add rsp, 40
    ret
ctr_sysv_kernel_pwrite ENDP
EXTERN ctr_kernel_close:PROC
PUBLIC ctr_sysv_kernel_close
ctr_sysv_kernel_close PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_kernel_close
    add rsp, 40
    ret
ctr_sysv_kernel_close ENDP
EXTERN ctr_kernel_write:PROC
PUBLIC ctr_sysv_kernel_write
ctr_sysv_kernel_write PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_kernel_write
    add rsp, 40
    ret
ctr_sysv_kernel_write ENDP

EXTERN ctr_equeue_create:PROC
EXTERN ctr_equeue_delete:PROC
EXTERN ctr_equeue_wait:PROC
PUBLIC ctr_sysv_equeue_create
ctr_sysv_equeue_create PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_equeue_create
    add rsp, 40
    ret
ctr_sysv_equeue_create ENDP
PUBLIC ctr_sysv_equeue_delete
ctr_sysv_equeue_delete PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_equeue_delete
    add rsp, 40
    ret
ctr_sysv_equeue_delete ENDP
PUBLIC ctr_sysv_equeue_wait
ctr_sysv_equeue_wait PROC
    mov r10, rcx
    mov r11, [rsp]
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], r11
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    mov r9, r10
    call ctr_equeue_wait
    add rsp, 56
    ret
ctr_sysv_equeue_wait ENDP
; Synthetic Win64 caller -> five SysV args -> actual resident adapter.
PUBLIC ctr_test_equeue_wait
ctr_test_equeue_wait PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov rsi, rdx
    mov rdx, r8
    mov rcx, r9
    mov r8, [rsp+96]
    call ctr_sysv_equeue_wait
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_test_equeue_wait ENDP
PUBLIC ctr_sysv_json2_0
ctr_sysv_json2_0 PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 0
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_json2_0 ENDP
PUBLIC ctr_sysv_json2_1
ctr_sysv_json2_1 PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 1
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_json2_1 ENDP
PUBLIC ctr_sysv_json2_2
ctr_sysv_json2_2 PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 2
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_json2_2 ENDP
PUBLIC ctr_sysv_json2_3
ctr_sysv_json2_3 PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 3
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_json2_3 ENDP
PUBLIC ctr_sysv_json2_4
ctr_sysv_json2_4 PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 4
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_json2_4 ENDP
PUBLIC ctr_sysv_json2_5
ctr_sysv_json2_5 PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 5
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_json2_5 ENDP
PUBLIC ctr_sysv_json2_6
ctr_sysv_json2_6 PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 6
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_json2_6 ENDP
PUBLIC ctr_sysv_net_init_compat
ctr_sysv_net_init_compat PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 8
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_net_init_compat ENDP
PUBLIC ctr_sysv_netctl_init_compat
ctr_sysv_netctl_init_compat PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 9
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_netctl_init_compat ENDP
PUBLIC ctr_font_memory_callback_control
ctr_font_memory_callback_control PROC
    inc qword ptr [rsi]
    ret
ctr_font_memory_callback_control ENDP
EXTERN ctr_font_memory_init:PROC
EXTERN ctr_font_memory_term:PROC
PUBLIC ctr_sysv_font_memory_init
ctr_sysv_font_memory_init PROC
    sub rsp, 72
    mov [rsp+32], r8
    mov [rsp+40], r9
    mov rax, [rsp+80]
    mov [rsp+48], rax
    mov rax, [rsp+72]
    mov [rsp+56], rax
    mov r8, rdx
    mov r9, rcx
    mov rcx, rdi
    mov rdx, rsi
    call ctr_font_memory_init
    add rsp, 72
    ret
ctr_sysv_font_memory_init ENDP
PUBLIC ctr_sysv_font_memory_term
ctr_sysv_font_memory_term PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov rcx, rdi
    call ctr_font_memory_term
    add rsp, 40
    ret
ctr_sysv_font_memory_term ENDP
PUBLIC ctr_test_font_memory_init
ctr_test_font_memory_init PROC
    push rdi
    push rsi
    sub rsp, 72
    mov rdi, rcx
    mov rsi, rdx
    mov rdx, r8
    mov rcx, r9
    mov r8, [rsp+128]
    mov r9, [rsp+136]
    mov rax, [rsp+144]
    mov [rsp], rax
    call ctr_sysv_font_memory_init
    add rsp, 72
    pop rsi
    pop rdi
    ret
ctr_test_font_memory_init ENDP
EXTERN ctr_font_select_library:PROC
EXTERN ctr_font_select_renderer:PROC
EXTERN ctr_font_callback_missing:PROC
PUBLIC ctr_sysv_font_select_library
ctr_sysv_font_select_library PROC
    sub rsp, 40
    mov ecx, edi
    call ctr_font_select_library
    add rsp, 40
    ret
ctr_sysv_font_select_library ENDP
PUBLIC ctr_sysv_font_select_renderer
ctr_sysv_font_select_renderer PROC
    sub rsp, 40
    mov ecx, edi
    call ctr_font_select_renderer
    add rsp, 40
    ret
ctr_sysv_font_select_renderer ENDP
PUBLIC ctr_sysv_font_pixel_resolution
ctr_sysv_font_pixel_resolution PROC
    mov eax, 64
    ret
ctr_sysv_font_pixel_resolution ENDP
PUBLIC ctr_sysv_font_library_callback_missing
ctr_sysv_font_library_callback_missing PROC
    mov rdx, [rsp]
    sub rsp, 40
    xor ecx, ecx
    call ctr_font_callback_missing
    int 3
ctr_sysv_font_library_callback_missing ENDP
PUBLIC ctr_sysv_font_renderer_callback_missing
ctr_sysv_font_renderer_callback_missing PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 1
    call ctr_font_callback_missing
    int 3
ctr_sysv_font_renderer_callback_missing ENDP
PUBLIC ctr_sysv_process_time
ctr_sysv_process_time PROC
    sub rsp, 40
    call ctr_kernel_process_time
    add rsp, 40
    ret
ctr_sysv_process_time ENDP
EXTERN ctr_thread_get_priority:PROC
PUBLIC ctr_sysv_thread_get_priority
ctr_sysv_thread_get_priority PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_get_priority
    add rsp, 40
    ret
ctr_sysv_thread_get_priority ENDP
EXTERN ctr_posix_thread_attr_init:PROC
PUBLIC ctr_sysv_posix_thread_attr_init
ctr_sysv_posix_thread_attr_init PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_posix_thread_attr_init
    add rsp, 40
    ret
ctr_sysv_posix_thread_attr_init ENDP
EXTERN ctr_posix_thread_attr_detach:PROC
PUBLIC ctr_sysv_posix_thread_attr_detach
ctr_sysv_posix_thread_attr_detach PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_posix_thread_attr_detach
    add rsp, 40
    ret
ctr_sysv_posix_thread_attr_detach ENDP
EXTERN ctr_posix_thread_attr_stack_size:PROC
PUBLIC ctr_sysv_posix_thread_attr_stack_size
ctr_sysv_posix_thread_attr_stack_size PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_posix_thread_attr_stack_size
    add rsp, 40
    ret
ctr_sysv_posix_thread_attr_stack_size ENDP
EXTERN ctr_posix_thread_create:PROC
PUBLIC ctr_sysv_posix_thread_create
ctr_sysv_posix_thread_create PROC
    mov r10, rdx
    mov r11, rcx
    mov rax, [rsp]
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], rax
    mov rcx, rdi
    mov rdx, rsi
    mov r8, r10
    mov r9, r11
    call ctr_posix_thread_create
    add rsp, 56
    ret
ctr_sysv_posix_thread_create ENDP
EXTERN ctr_posix_thread_attr_destroy:PROC
PUBLIC ctr_sysv_posix_thread_attr_destroy
ctr_sysv_posix_thread_attr_destroy PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_posix_thread_attr_destroy
    add rsp, 40
    ret
ctr_sysv_posix_thread_attr_destroy ENDP
EXTERN ctr_gnm_init350:PROC
PUBLIC ctr_sysv_gnm_init350
ctr_sysv_gnm_init350 PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov edx, esi
    sub rsp, 40
    call ctr_gnm_init350
    add rsp, 40
    ret
ctr_sysv_gnm_init350 ENDP

EXTERN ctr_video_open:PROC
PUBLIC ctr_sysv_video_open
ctr_sysv_video_open PROC
    sub rsp, 56
    mov rax, [rsp+56]
    mov [rsp+32], rax
    mov r9, rcx
    mov r8d, edx
    mov edx, esi
    mov ecx, edi
    call ctr_video_open
    add rsp, 56
    ret
ctr_sysv_video_open ENDP


EXTERN ctr_video_resolution:PROC
PUBLIC ctr_sysv_video_resolution
ctr_sysv_video_resolution PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_video_resolution
    add rsp, 40
    ret
ctr_sysv_video_resolution ENDP
EXTERN ctr_video_close:PROC
PUBLIC ctr_sysv_video_close
ctr_sysv_video_close PROC
    mov rdx, [rsp]
    mov ecx, edi
    sub rsp, 40
    call ctr_video_close
    add rsp, 40
    ret
ctr_sysv_video_close ENDP

EXTERN ctr_video_capability:PROC
PUBLIC ctr_sysv_video_capability
ctr_sysv_video_capability PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_video_capability
    add rsp, 40
    ret
ctr_sysv_video_capability ENDP

EXTERN ctr_video_add_flip:PROC
PUBLIC ctr_sysv_video_add_flip
ctr_sysv_video_add_flip PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_video_add_flip
    add rsp, 40
    ret
ctr_sysv_video_add_flip ENDP

EXTERN ctr_video_add_vblank:PROC
PUBLIC ctr_sysv_video_add_vblank
ctr_sysv_video_add_vblank PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_video_add_vblank
    add rsp, 40
    ret
ctr_sysv_video_add_vblank ENDP

EXTERN ctr_video_flip_status:PROC
PUBLIC ctr_sysv_video_flip_status
ctr_sysv_video_flip_status PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_video_flip_status
    add rsp, 40
    ret
ctr_sysv_video_flip_status ENDP

EXTERN ctr_video_vblank_status:PROC
PUBLIC ctr_sysv_video_vblank_status
ctr_sysv_video_vblank_status PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_video_vblank_status
    add rsp, 40
    ret
ctr_sysv_video_vblank_status ENDP

EXTERN ctr_video_set_rate:PROC
EXTERN ctr_video_register:PROC
PUBLIC ctr_sysv_video_register
ctr_sysv_video_register PROC
    sub rsp, 56
    mov [rsp+32], r8
    mov rax, [rsp+56]
    mov [rsp+40], rax
    mov r9d, ecx
    mov r8, rdx
    mov edx, esi
    mov ecx, edi
    call ctr_video_register
    add rsp, 56
    ret
ctr_sysv_video_register ENDP
EXTERN ctr_video_label_address:PROC
EXTERN ctr_video_configure:PROC
PUBLIC ctr_sysv_video_configure
ctr_sysv_video_configure PROC
    sub rsp, 56
    mov [rsp+32], r8d
    mov [rsp+40], r9d
    mov rax, [rsp+56]
    mov [rsp+48], rax
    mov r9, rcx
    mov r8, rdx
    mov edx, esi
    mov ecx, edi
    call ctr_video_configure
    add rsp, 56
    ret
ctr_sysv_video_configure ENDP
PUBLIC ctr_video_configure_bridge_fixture
ctr_video_configure_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdx, rcx
    mov edi, 1
    xor esi, esi
    xor ecx, ecx
    mov r8d, 32
    mov r9d, 16
    call ctr_sysv_video_configure
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_video_configure_bridge_fixture ENDP
EXTERN ctr_video_unregister:PROC
PUBLIC ctr_sysv_video_unregister
ctr_sysv_video_unregister PROC
    mov r8, [rsp]
    mov edx, esi
    mov ecx, edi
    sub rsp, 40
    call ctr_video_unregister
    add rsp, 40
    ret
ctr_sysv_video_unregister ENDP
EXTERN ctr_video_mode_any:PROC
PUBLIC ctr_sysv_video_mode_any
ctr_sysv_video_mode_any PROC
    mov r8, [rsp]
    mov edx, esi
    mov rcx, rdi
    sub rsp, 40
    call ctr_video_mode_any
    add rsp, 40
    ret
ctr_sysv_video_mode_any ENDP
PUBLIC ctr_sysv_video_label_address
ctr_sysv_video_label_address PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov ecx, edi
    sub rsp, 40
    call ctr_video_label_address
    add rsp, 40
    ret
ctr_sysv_video_label_address ENDP
PUBLIC ctr_sysv_video_set_rate
ctr_sysv_video_set_rate PROC
    mov r8, [rsp]
    mov edx, esi
    mov ecx, edi
    sub rsp, 40
    call ctr_video_set_rate
    add rsp, 40
    ret
ctr_sysv_video_set_rate ENDP

EXTERN ctr_video_set_attributes:PROC
PUBLIC ctr_sysv_video_set_attributes
ctr_sysv_video_set_attributes PROC
    sub rsp, 72
    mov [rsp+32], r8d
    mov [rsp+40], r9d
    mov eax, [rsp+80]
    mov [rsp+48], eax
    mov rax, [rsp+72]
    mov [rsp+56], rax
    mov r9d, ecx
    mov r8d, edx
    mov edx, esi
    mov rcx, rdi
    call ctr_video_set_attributes
    add rsp, 72
    ret
ctr_sysv_video_set_attributes ENDP
PUBLIC ctr_video_attribute_bridge_fixture
ctr_video_attribute_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov esi, 80000000h
    xor edx, edx
    xor ecx, ecx
    mov r8d, 1920
    mov r9d, 1080
    mov dword ptr [rsp], 2048
    call ctr_sysv_video_set_attributes
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_video_attribute_bridge_fixture ENDP
PUBLIC ctr_video_register_bridge_fixture
ctr_video_register_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov r10, rcx
    mov edi, 1
    mov esi, [r10]
    lea rdx, [r10+8]
    mov ecx, 2
    lea r8, [r10+24]
    call ctr_sysv_video_register
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_video_register_bridge_fixture ENDP
EXTERN ctr_gnm_submit:PROC
PUBLIC ctr_sysv_gnm_submit
ctr_sysv_gnm_submit PROC
    sub rsp, 56
    mov [rsp+32], r8
    mov rax, [rsp+56]
    mov [rsp+40], rax
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov ecx, edi
    call ctr_gnm_submit
    add rsp, 56
    ret
ctr_sysv_gnm_submit ENDP
PUBLIC ctr_gnm_submit_bridge_fixture
ctr_gnm_submit_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rax, rcx
    mov rdi, [rax]
    mov rsi, [rax+8]
    mov rdx, [rax+16]
    mov rcx, [rax+24]
    mov r8, [rax+32]
    call ctr_sysv_gnm_submit
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_gnm_submit_bridge_fixture ENDP
EXTERN ctr_batch_map:PROC
PUBLIC ctr_sysv_batch_map
ctr_sysv_batch_map PROC
    mov r9, [rsp]
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    sub rsp, 40
    call ctr_batch_map
    add rsp, 40
    ret
ctr_sysv_batch_map ENDP
PUBLIC ctr_batch_map_bridge_fixture
ctr_batch_map_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov esi, edx
    mov rdx, r8
    call ctr_sysv_batch_map
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_batch_map_bridge_fixture ENDP
EXTERN ctr_usleep:PROC
PUBLIC ctr_sysv_usleep
ctr_sysv_usleep PROC
    mov rdx, [rsp]
    mov ecx, edi
    sub rsp, 40
    call ctr_usleep
    add rsp, 40
    ret
ctr_sysv_usleep ENDP
PUBLIC ctr_usleep_bridge_fixture
ctr_usleep_bridge_fixture PROC
    push rdi
    sub rsp, 32
    mov edi, ecx
    call ctr_sysv_usleep
    add rsp, 32
    pop rdi
    ret
ctr_usleep_bridge_fixture ENDP
EXTERN ctr_gnm_set_vs:PROC
PUBLIC ctr_sysv_gnm_set_vs
ctr_sysv_gnm_set_vs PROC
    sub rsp, 40
    mov rax, [rsp+40]
    mov [rsp+32], rax
    mov r9d, ecx
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    call ctr_gnm_set_vs
    add rsp, 40
    ret
ctr_sysv_gnm_set_vs ENDP
PUBLIC ctr_gnm_set_vs_bridge_fixture
ctr_gnm_set_vs_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov esi, edx
    mov rdx, r8
    mov ecx, r9d
    call ctr_sysv_gnm_set_vs
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_gnm_set_vs_bridge_fixture ENDP
EXTERN ctr_shareplay_info_compat:PROC
PUBLIC ctr_sysv_shareplay_info_compat
ctr_sysv_shareplay_info_compat PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_shareplay_info_compat
    add rsp, 40
    ret
ctr_sysv_shareplay_info_compat ENDP
PUBLIC ctr_sysv_shareplay_init_compat
ctr_sysv_shareplay_init_compat PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 14
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_shareplay_init_compat ENDP
PUBLIC ctr_sysv_shareplay_prohibition_compat
ctr_sysv_shareplay_prohibition_compat PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 20
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_shareplay_prohibition_compat ENDP
EXTERN ctr_pad_init:PROC
PUBLIC ctr_sysv_pad_init
ctr_sysv_pad_init PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_pad_init
    add rsp, 40
    ret
ctr_sysv_pad_init ENDP
EXTERN ctr_gnm_set_ps:PROC
PUBLIC ctr_sysv_gnm_set_ps
ctr_sysv_gnm_set_ps PROC
    mov r9, [rsp]
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    sub rsp, 40
    call ctr_gnm_set_ps
    add rsp, 40
    ret
ctr_sysv_gnm_set_ps ENDP
PUBLIC ctr_gnm_set_ps_bridge_fixture
ctr_gnm_set_ps_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov esi, edx
    mov rdx, r8
    call ctr_sysv_gnm_set_ps
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_gnm_set_ps_bridge_fixture ENDP
EXTERN ctr_gnm_draw_auto:PROC
PUBLIC ctr_sysv_gnm_draw_auto
ctr_sysv_gnm_draw_auto PROC
    sub rsp, 40
    mov rax, [rsp+40]
    mov [rsp+32], rax
    mov r9d, ecx
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    call ctr_gnm_draw_auto
    add rsp, 40
    ret
ctr_sysv_gnm_draw_auto ENDP
PUBLIC ctr_gnm_draw_auto_bridge_fixture
ctr_gnm_draw_auto_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov esi, edx
    mov rdx, r8
    mov ecx, r9d
    call ctr_sysv_gnm_draw_auto
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_gnm_draw_auto_bridge_fixture ENDP

EXTERN ctr_gnm_update_vs:PROC
PUBLIC ctr_sysv_gnm_update_vs
ctr_sysv_gnm_update_vs PROC
    sub rsp, 40
    mov rax, [rsp+40]
    mov [rsp+32], rax
    mov r9d, ecx
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    call ctr_gnm_update_vs
    add rsp, 40
    ret
ctr_sysv_gnm_update_vs ENDP
PUBLIC ctr_gnm_update_vs_bridge_fixture
ctr_gnm_update_vs_bridge_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rdi, rcx
    mov esi, edx
    mov rdx, r8
    mov ecx, r9d
    call ctr_sysv_gnm_update_vs
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_gnm_update_vs_bridge_fixture ENDP

EXTERN ctr_hide_splash:PROC
PUBLIC ctr_sysv_hide_splash
ctr_sysv_hide_splash PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_hide_splash
    add rsp, 40
    ret
ctr_sysv_hide_splash ENDP

EXTERN ctr_kernel_clock:PROC
PUBLIC ctr_sysv_kernel_clock
ctr_sysv_kernel_clock PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_kernel_clock
    add rsp, 40
    ret
ctr_sysv_kernel_clock ENDP

EXTERN ctr_ajm_initialize:PROC
PUBLIC ctr_sysv_ajm_initialize
ctr_sysv_ajm_initialize PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_ajm_initialize
    add rsp, 40
    ret
ctr_sysv_ajm_initialize ENDP

EXTERN ctr_ajm_register:PROC
PUBLIC ctr_sysv_ajm_register
ctr_sysv_ajm_register PROC
    mov r9, [rsp]
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    sub rsp, 40
    call ctr_ajm_register
    add rsp, 40
    ret
ctr_sysv_ajm_register ENDP

EXTERN ctr_audio_out_init:PROC
PUBLIC ctr_sysv_audio_out_init
ctr_sysv_audio_out_init PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_audio_out_init
    add rsp, 40
    ret
ctr_sysv_audio_out_init ENDP

EXTERN ctr_gnm_set_cs:PROC
PUBLIC ctr_sysv_gnm_set_cs
ctr_sysv_gnm_set_cs PROC
    sub rsp, 40
    mov rax, [rsp+40]
    mov [rsp+32], rax
    mov r9d, ecx
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    call ctr_gnm_set_cs
    add rsp, 40
    ret
ctr_sysv_gnm_set_cs ENDP

EXTERN ctr_gnm_dispatch_direct_bridge:PROC
PUBLIC ctr_sysv_gnm_dispatch_direct
ctr_sysv_gnm_dispatch_direct PROC
    sub rsp, 56
    mov [rsp+32], r8d
    mov [rsp+40], r9d
    mov rax, [rsp+56]
    mov [rsp+48], rax
    mov r9d, ecx
    mov r8d, edx
    mov edx, esi
    mov rcx, rdi
    call ctr_gnm_dispatch_direct_bridge
    add rsp, 56
    ret
ctr_sysv_gnm_dispatch_direct ENDP
PUBLIC ctr_gnm_dispatch_direct_fixture
ctr_gnm_dispatch_direct_fixture PROC
    push rdi
    push rsi
    sub rsp, 56
    mov rdi, rcx
    mov esi, edx
    mov edx, r8d
    mov ecx, r9d
    mov r8d, [rsp+112]
    mov r9d, [rsp+120]
    call ctr_sysv_gnm_dispatch_direct
    add rsp, 56
    pop rsi
    pop rdi
    ret
ctr_gnm_dispatch_direct_fixture ENDP
EXTERN ctr_audio_out_open:PROC
PUBLIC ctr_sysv_audio_out_open
ctr_sysv_audio_out_open PROC
    sub rsp, 56
    mov [rsp+32], r8d
    mov [rsp+40], r9d
    mov rax, [rsp+56]
    mov [rsp+48], rax
    mov r9d, ecx
    mov r8d, edx
    mov edx, esi
    mov rcx, rdi
    call ctr_audio_out_open
    add rsp, 56
    ret
ctr_sysv_audio_out_open ENDP
PUBLIC ctr_audio_out_open_fixture
ctr_audio_out_open_fixture PROC
    push rdi
    push rsi
    sub rsp, 56
    mov rdi, rcx
    mov esi, edx
    mov edx, r8d
    mov ecx, r9d
    mov r8d, [rsp+112]
    mov r9d, [rsp+120]
    call ctr_sysv_audio_out_open
    add rsp, 56
    pop rsi
    pop rdi
    ret
ctr_audio_out_open_fixture ENDP
EXTERN ctr_audio_out_volume:PROC
PUBLIC ctr_sysv_audio_out_volume
ctr_sysv_audio_out_volume PROC
    mov r9, [rsp]
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    sub rsp, 40
    call ctr_audio_out_volume
    add rsp, 40
    ret
ctr_sysv_audio_out_volume ENDP
EXTERN ctr_thread_set_priority:PROC
PUBLIC ctr_sysv_thread_set_priority
ctr_sysv_thread_set_priority PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_set_priority
    add rsp, 40
    ret
ctr_sysv_thread_set_priority ENDP
EXTERN ctr_audio_out_close:PROC
PUBLIC ctr_sysv_audio_out_close
ctr_sysv_audio_out_close PROC
    mov rdx, [rsp]
    mov ecx, edi
    sub rsp, 40
    call ctr_audio_out_close
    add rsp, 40
    ret
ctr_sysv_audio_out_close ENDP
EXTERN ctr_audio_out_output:PROC
PUBLIC ctr_sysv_audio_out_output
ctr_sysv_audio_out_output PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_audio_out_output
    add rsp, 40
    ret
ctr_sysv_audio_out_output ENDP
EXTERN ctr_gnm_update_ps:PROC
PUBLIC ctr_sysv_gnm_update_ps
ctr_sysv_gnm_update_ps PROC
    mov r9, [rsp]
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    sub rsp, 40
    call ctr_gnm_update_ps
    add rsp, 40
    ret
ctr_sysv_gnm_update_ps ENDP
EXTERN ctr_gnm_push_marker_bridge:PROC
PUBLIC ctr_sysv_gnm_push_marker
ctr_sysv_gnm_push_marker PROC
    mov r9, [rsp]
    mov r8, rdx
    mov edx, esi
    mov rcx, rdi
    sub rsp, 40
    call ctr_gnm_push_marker_bridge
    add rsp, 40
    ret
ctr_sysv_gnm_push_marker ENDP
EXTERN ctr_gnm_pop_marker_bridge:PROC
PUBLIC ctr_sysv_gnm_pop_marker
ctr_sysv_gnm_pop_marker PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov edx, esi
    sub rsp, 40
    call ctr_gnm_pop_marker_bridge
    add rsp, 40
    ret
ctr_sysv_gnm_pop_marker ENDP
EXTERN ctr_user_event:PROC
PUBLIC ctr_sysv_user_event
ctr_sysv_user_event PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_user_event
    add rsp, 40
    ret
ctr_sysv_user_event ENDP
EXTERN ctr_net_pool_create:PROC
PUBLIC ctr_sysv_net_pool_create
ctr_sysv_net_pool_create PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_net_pool_create
    add rsp, 40
    ret
ctr_sysv_net_pool_create ENDP
EXTERN ctr_net_pool_destroy:PROC
PUBLIC ctr_sysv_net_pool_destroy
ctr_sysv_net_pool_destroy PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_net_pool_destroy
    add rsp, 40
    ret
ctr_sysv_net_pool_destroy ENDP
EXTERN ctr_cxa_atexit:PROC
PUBLIC ctr_sysv_cxa_atexit
ctr_sysv_cxa_atexit PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_cxa_atexit
    add rsp, 40
    ret
ctr_sysv_cxa_atexit ENDP
EXTERN ctr_cxa_finalize:PROC
PUBLIC ctr_sysv_cxa_finalize
ctr_sysv_cxa_finalize PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_cxa_finalize
    add rsp, 40
    ret
ctr_sysv_cxa_finalize ENDP
EXTERN ctr_cxa_destructor_observe:PROC
PUBLIC ctr_sysv_cxa_destructor_control
ctr_sysv_cxa_destructor_control PROC
    mov rcx, rdi
    sub rsp, 40
    call ctr_cxa_destructor_observe
    add rsp, 40
    ret
ctr_sysv_cxa_destructor_control ENDP
EXTERN ctr_cxa_guard_acquire:PROC
PUBLIC ctr_sysv_cxa_guard_acquire
ctr_sysv_cxa_guard_acquire PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_cxa_guard_acquire
    add rsp, 40
    ret
ctr_sysv_cxa_guard_acquire ENDP
EXTERN ctr_cxa_guard_release:PROC
PUBLIC ctr_sysv_cxa_guard_release
ctr_sysv_cxa_guard_release PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_cxa_guard_release
    add rsp, 40
    ret
ctr_sysv_cxa_guard_release ENDP
EXTERN ctr_cxa_guard_abort:PROC
PUBLIC ctr_sysv_cxa_guard_abort
ctr_sysv_cxa_guard_abort PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_cxa_guard_abort
    add rsp, 40
    ret
ctr_sysv_cxa_guard_abort ENDP
EXTERN ctr_user_initial:PROC
PUBLIC ctr_sysv_user_initial
ctr_sysv_user_initial PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_user_initial
    add rsp, 40
    ret
ctr_sysv_user_initial ENDP
EXTERN ctr_pad_open:PROC
PUBLIC ctr_sysv_pad_open
ctr_sysv_pad_open PROC
    mov r10, rcx
    sub rsp, 40
    mov rax, [rsp+40]
    mov [rsp+32], rax
    mov r9, r10
    mov r8d, edx
    mov edx, esi
    mov ecx, edi
    call ctr_pad_open
    add rsp, 40
    ret
ctr_sysv_pad_open ENDP
PUBLIC ctr_sysv_live_stream_init_compat
ctr_sysv_live_stream_init_compat PROC
    mov rdx, [rsp]
    sub rsp, 40
    mov ecx, 16
    call ctr_json2_stub
    add rsp, 40
    ret
ctr_sysv_live_stream_init_compat ENDP
EXTERN ctr_gnm_draw_index_bridge:PROC
PUBLIC ctr_sysv_gnm_draw_index
ctr_sysv_gnm_draw_index PROC
    sub rsp, 56
    mov [rsp+32], r8d
    mov [rsp+40], r9d
    mov rax, [rsp+56]
    mov [rsp+48], rax
    mov r9, rcx
    mov r8d, edx
    mov edx, esi
    mov rcx, rdi
    call ctr_gnm_draw_index_bridge
    add rsp, 56
    ret
ctr_sysv_gnm_draw_index ENDP
PUBLIC ctr_gnm_draw_index_fixture
ctr_gnm_draw_index_fixture PROC
    push rdi
    push rsi
    sub rsp, 56
    mov rdi, rcx
    mov esi, edx
    mov edx, r8d
    mov rcx, r9
    mov r8d, [rsp+112]
    mov r9d, [rsp+120]
    call ctr_sysv_gnm_draw_index
    add rsp, 56
    pop rsi
    pop rdi
    ret
ctr_gnm_draw_index_fixture ENDP

EXTERN ctr_gnm_flip_bridge:PROC
PUBLIC ctr_sysv_gnm_flip
ctr_sysv_gnm_flip PROC
    sub rsp, 88
    mov [rsp+32], r8
    mov [rsp+40], r9d
    mov eax, [rsp+96]
    mov [rsp+48], eax
    mov eax, [rsp+104]
    mov [rsp+56], eax
    mov rax, [rsp+112]
    mov [rsp+64], rax
    mov rax, [rsp+88]
    mov [rsp+72], rax
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov ecx, edi
    call ctr_gnm_flip_bridge
    add rsp, 88
    ret
ctr_sysv_gnm_flip ENDP
PUBLIC ctr_gnm_flip_fixture
ctr_gnm_flip_fixture PROC
    push rdi
    push rsi
    sub rsp, 40
    mov rax, rcx
    mov rdi, [rax]
    mov rsi, [rax+8]
    mov rdx, [rax+16]
    mov rcx, [rax+24]
    mov r8, [rax+32]
    mov r9, [rax+40]
    mov r10, [rax+48]
    mov [rsp], r10
    mov r10, [rax+56]
    mov [rsp+8], r10
    mov r10, [rax+64]
    mov [rsp+16], r10
    call ctr_sysv_gnm_flip
    add rsp, 40
    pop rsi
    pop rdi
    ret
ctr_gnm_flip_fixture ENDP
EXTERN ctr_gnm_add_eq_event:PROC
EXTERN ctr_gnm_delete_eq_event:PROC
EXTERN ctr_gnm_get_eq_event_type:PROC
PUBLIC ctr_sysv_gnm_add_eq_event
ctr_sysv_gnm_add_eq_event PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_gnm_add_eq_event
    add rsp, 40
    ret
ctr_sysv_gnm_add_eq_event ENDP
PUBLIC ctr_sysv_gnm_delete_eq_event
ctr_sysv_gnm_delete_eq_event PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_gnm_delete_eq_event
    add rsp, 40
    ret
ctr_sysv_gnm_delete_eq_event ENDP
PUBLIC ctr_sysv_gnm_get_eq_event_type
ctr_sysv_gnm_get_eq_event_type PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_gnm_get_eq_event_type
    add rsp, 40
    ret
ctr_sysv_gnm_get_eq_event_type ENDP
EXTERN ctr_user_name:PROC
PUBLIC ctr_sysv_user_name
ctr_sysv_user_name PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_user_name
    add rsp, 40
    ret
ctr_sysv_user_name ENDP

EXTERN ctr_np_id:PROC
PUBLIC ctr_sysv_np_id
ctr_sysv_np_id PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_np_id
    add rsp, 40
    ret
ctr_sysv_np_id ENDP

EXTERN ctr_pad_read_state:PROC
PUBLIC ctr_sysv_pad_read_state
ctr_sysv_pad_read_state PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_pad_read_state
    add rsp, 40
    ret
ctr_sysv_pad_read_state ENDP
EXTERN ctr_pad_set_vibration:PROC
PUBLIC ctr_sysv_pad_set_vibration
ctr_sysv_pad_set_vibration PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov ecx, edi
    sub rsp, 40
    call ctr_pad_set_vibration
    add rsp, 40
    ret
ctr_sysv_pad_set_vibration ENDP
EXTERN ctr_pad_set_light_bar:PROC
PUBLIC ctr_sysv_pad_set_light_bar
ctr_sysv_pad_set_light_bar PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov ecx, edi
    sub rsp, 40
    call ctr_pad_set_light_bar
    add rsp, 40
    ret
ctr_sysv_pad_set_light_bar ENDP
EXTERN ctr_pad_close:PROC
PUBLIC ctr_sysv_pad_close
ctr_sysv_pad_close PROC
    mov rdx, [rsp]
    mov ecx, edi
    sub rsp, 40
    call ctr_pad_close
    add rsp, 40
    ret
ctr_sysv_pad_close ENDP
EXTERN ctr_pad_reset_light_bar:PROC
PUBLIC ctr_sysv_pad_reset_light_bar
ctr_sysv_pad_reset_light_bar PROC
    mov rdx, [rsp]
    mov ecx, edi
    sub rsp, 40
    call ctr_pad_reset_light_bar
    add rsp, 40
    ret
ctr_sysv_pad_reset_light_bar ENDP

EXTERN ctr_condition_destroy:PROC
PUBLIC ctr_sysv_condition_destroy
ctr_sysv_condition_destroy PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_condition_destroy
    add rsp, 40
    ret
ctr_sysv_condition_destroy ENDP

EXTERN ctr_mutex_destroy:PROC
PUBLIC ctr_sysv_mutex_destroy
ctr_sysv_mutex_destroy PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_mutex_destroy
    add rsp, 40
    ret
ctr_sysv_mutex_destroy ENDP
EXTERN ctr_posix_mutex_destroy:PROC
PUBLIC ctr_sysv_posix_mutex_destroy
ctr_sysv_posix_mutex_destroy PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_posix_mutex_destroy
    add rsp, 40
    ret
ctr_sysv_posix_mutex_destroy ENDP

EXTERN ctr_video_gamma:PROC
PUBLIC ctr_sysv_video_gamma
ctr_sysv_video_gamma PROC
    mov r8, [rsp]
    mov rcx, rdi
    movaps xmm1, xmm0
    sub rsp, 40
    call ctr_video_gamma
    add rsp, 40
    ret
ctr_sysv_video_gamma ENDP
PUBLIC ctr_video_gamma_fixture
ctr_video_gamma_fixture PROC
    push rdi
    sub rsp, 32
    mov rdi, rcx
    movaps xmm0, xmm1
    call ctr_sysv_video_gamma
    add rsp, 32
    pop rdi
    ret
ctr_video_gamma_fixture ENDP

EXTERN ctr_video_adjust_color:PROC
PUBLIC ctr_sysv_video_adjust_color
ctr_sysv_video_adjust_color PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_video_adjust_color
    add rsp, 40
    ret
ctr_sysv_video_adjust_color ENDP

EXTERN ctr_pad_information:PROC
PUBLIC ctr_sysv_pad_information
ctr_sysv_pad_information PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_pad_information
    add rsp, 40
    ret
ctr_sysv_pad_information ENDP

EXTERN ctr_ssl_init:PROC
PUBLIC ctr_sysv_ssl_init
ctr_sysv_ssl_init PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_ssl_init
    add rsp, 40
    ret
ctr_sysv_ssl_init ENDP
EXTERN ctr_ssl_term:PROC
PUBLIC ctr_sysv_ssl_term
ctr_sysv_ssl_term PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_ssl_term
    add rsp, 40
    ret
ctr_sysv_ssl_term ENDP

EXTERN ctr_http_init:PROC
PUBLIC ctr_sysv_http_init
ctr_sysv_http_init PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_http_init
    add rsp, 40
    ret
ctr_sysv_http_init ENDP
EXTERN ctr_http_term:PROC
PUBLIC ctr_sysv_http_term
ctr_sysv_http_term PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_http_term
    add rsp, 40
    ret
ctr_sysv_http_term ENDP

EXTERN ctr_np_web_init:PROC
PUBLIC ctr_sysv_np_web_init
ctr_sysv_np_web_init PROC
    mov r8, [rsp]
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_np_web_init
    add rsp, 40
    ret
ctr_sysv_np_web_init ENDP
EXTERN ctr_np_web_term:PROC
PUBLIC ctr_sysv_np_web_term
ctr_sysv_np_web_term PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_np_web_term
    add rsp, 40
    ret
ctr_sysv_np_web_term ENDP

EXTERN ctr_map_flexible:PROC
PUBLIC ctr_sysv_map_flexible
ctr_sysv_map_flexible PROC
    mov r10, [rsp]
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], r10
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_map_flexible
    add rsp, 56
    ret
ctr_sysv_map_flexible ENDP
EXTERN ctr_system_status:PROC
PUBLIC ctr_sysv_system_status
ctr_sysv_system_status PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_system_status
    add rsp, 40
    ret
ctr_sysv_system_status ENDP

EXTERN ctr_np_check:PROC
PUBLIC ctr_sysv_np_check
ctr_sysv_np_check PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_np_check
    add rsp, 40
    ret
ctr_sysv_np_check ENDP
EXTERN ctr_thread_get_affinity:PROC
PUBLIC ctr_sysv_thread_get_affinity
ctr_sysv_thread_get_affinity PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_thread_get_affinity
    add rsp, 40
    ret
ctr_sysv_thread_get_affinity ENDP
EXTERN ctr_ime_dialog_status:PROC
PUBLIC ctr_sysv_ime_dialog_status
ctr_sysv_ime_dialog_status PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_ime_dialog_status
    add rsp, 40
    ret
ctr_sysv_ime_dialog_status ENDP
EXTERN ctr_attr_get_stack:PROC
PUBLIC ctr_sysv_attr_get_stack
ctr_sysv_attr_get_stack PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_attr_get_stack
    add rsp, 40
    ret
ctr_sysv_attr_get_stack ENDP
EXTERN ctr_posix_thread_self:PROC
PUBLIC ctr_sysv_posix_thread_self
ctr_sysv_posix_thread_self PROC
    mov rcx, [rsp]
    sub rsp, 40
    call ctr_posix_thread_self
    add rsp, 40
    ret
ctr_sysv_posix_thread_self ENDP
EXTERN ctr_posix_set_sched:PROC
PUBLIC ctr_sysv_posix_set_sched
ctr_sysv_posix_set_sched PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_posix_set_sched
    add rsp, 40
    ret
ctr_sysv_posix_set_sched ENDP
EXTERN ctr_orbis_gettimeofday:PROC
PUBLIC ctr_sysv_orbis_gettimeofday
ctr_sysv_orbis_gettimeofday PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_orbis_gettimeofday
    add rsp, 40
    ret
ctr_sysv_orbis_gettimeofday ENDP
EXTERN ctr_np_web_create_user:PROC
PUBLIC ctr_sysv_np_web_create_user
ctr_sysv_np_web_create_user PROC
    mov r8, [rsp]
    mov rcx, rdi
    mov rdx, rsi
    sub rsp, 40
    call ctr_np_web_create_user
    add rsp, 40
    ret
ctr_sysv_np_web_create_user ENDP
EXTERN ctr_net_connection_state:PROC
PUBLIC ctr_sysv_net_connection_state
ctr_sysv_net_connection_state PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_net_connection_state
    add rsp, 40
    ret
ctr_sysv_net_connection_state ENDP
EXTERN ctr_np_web_create_request:PROC
PUBLIC ctr_sysv_np_web_create_request
ctr_sysv_np_web_create_request PROC
    mov r10, [rsp]
    sub rsp, 56
    mov [rsp+32], r8
    mov [rsp+40], r9
    mov [rsp+48], r10
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_np_web_create_request
    add rsp, 56
    ret
ctr_sysv_np_web_create_request ENDP
PUBLIC ctr_sysv_np_request_fixture
ctr_sysv_np_request_fixture PROC
    mov r11, rdi
    mov rdi, [r11]
    mov rsi, [r11+8]
    mov rdx, [r11+16]
    mov rcx, [r11+24]
    mov r8, [r11+32]
    mov r9, [r11+40]
    jmp ctr_sysv_np_web_create_request
ctr_sysv_np_request_fixture ENDP
EXTERN ctr_np_web_send_offline:PROC
PUBLIC ctr_sysv_np_web_send_offline
ctr_sysv_np_web_send_offline PROC
    mov r10, [rsp]
    sub rsp, 40
    mov [rsp+32], r10
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_np_web_send_offline
    add rsp, 40
    ret
ctr_sysv_np_web_send_offline ENDP
PUBLIC ctr_sysv_np_send_fixture
ctr_sysv_np_send_fixture PROC
    mov r11, rdi
    mov rdi, [r11]
    mov rsi, [r11+8]
    mov rdx, [r11+16]
    mov rcx, [r11+24]
    jmp ctr_sysv_np_web_send_offline
ctr_sysv_np_send_fixture ENDP
EXTERN ctr_np_web_delete_request:PROC
PUBLIC ctr_sysv_np_web_delete_request
ctr_sysv_np_web_delete_request PROC
    mov rdx, [rsp]
    mov rcx, rdi
    sub rsp, 40
    call ctr_np_web_delete_request
    add rsp, 40
    ret
ctr_sysv_np_web_delete_request ENDP
EXTERN ctr_app_content_list:PROC
PUBLIC ctr_sysv_app_content_list
ctr_sysv_app_content_list PROC
    mov r10, [rsp]
    sub rsp, 40
    mov [rsp+32], r10
    mov r9, rcx
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    call ctr_app_content_list
    add rsp, 40
    ret
ctr_sysv_app_content_list ENDP
PUBLIC ctr_sysv_app_list_fixture
ctr_sysv_app_list_fixture PROC
    mov r11, rdi
    mov rdi, [r11]
    mov rsi, [r11+8]
    mov rdx, [r11+16]
    mov rcx, [r11+24]
    jmp ctr_sysv_app_content_list
ctr_sysv_app_list_fixture ENDP
EXTERN ctr_kernel_read:PROC
PUBLIC ctr_sysv_kernel_read
ctr_sysv_kernel_read PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_kernel_read
    add rsp, 40
    ret
ctr_sysv_kernel_read ENDP
EXTERN ctr_condition_timedwait:PROC
PUBLIC ctr_sysv_condition_timedwait
ctr_sysv_condition_timedwait PROC
    mov r9, [rsp]
    mov r8, rdx
    mov rdx, rsi
    mov rcx, rdi
    sub rsp, 40
    call ctr_condition_timedwait
    add rsp, 40
    ret
ctr_sysv_condition_timedwait ENDP
EXTERN ctr_virtual_query:PROC
PUBLIC ctr_sysv_virtual_query
ctr_sysv_virtual_query PROC
    mov rax, qword ptr [rsp]
    sub rsp, 56
    mov qword ptr [rsp+32], rax
    mov r9, rcx
    mov r8, rdx
    mov rcx, rdi
    mov edx, esi
    call ctr_virtual_query
    add rsp, 56
    ret
ctr_sysv_virtual_query ENDP
END
