#include "ctr/atexit_embedded_registration.h"
extern "C" __declspec(dllexport) unsigned long long ctr_atexit_registration_cookie(){return ctr::atexit_actual_cookie();}
