// SPDX-License-Identifier: GPL-2.0-only
#ifndef CTRPORT_OWNER_DISPATCH_API_H
#define CTRPORT_OWNER_DISPATCH_API_H
#include <stdint.h>
/* Windows C ABI v1, not PS4 SysV. Callback/user/output remain valid until invoke
   returns. invoke waits without a queue lock; caller holds no loader/kernel/
   session locks. Return0 produces action_result, including nonzero action errors.
   Nonzero transport status leaves action_result unchanged. Callback code and
   context stay loaded until all calls finish; close then join consumers before
   destroying the dispatcher or unloading a service. Owner must pump regularly. */
typedef int (*CtrOwnerAction)(void* user);
typedef struct CtrOwnerDispatchApi {
 uint32_t size,version;
 void* context;
 int (*invoke)(void*,CtrOwnerAction,void* user,int* action_result);
} CtrOwnerDispatchApi;
#endif
