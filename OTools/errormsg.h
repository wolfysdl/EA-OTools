#pragma once
#include <string>
#include "utils.h"
#include <iostream>

enum ErrorDisplayType {
    ERR_MESSAGE_BOX,
    ERR_CONSOLE,
    ERR_NONE
};

extern ErrorDisplayType displayType;

void SetErrorDisplayType(ErrorDisplayType type);
bool ErrorMessage(std::string const &msg);
