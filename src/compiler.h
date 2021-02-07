#pragma once

#include "common.h"
#include "object.h"

ObjFunction* compile(const char* source);
void compiler_mark_roots();
