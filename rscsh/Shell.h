#pragma once

#include <sstream>

class Shell {
public:
    Shell(std::wostringstream &execution_yield);
    Shell(Shell const &other) = delete;
    Shell& operator=(Shell const &other) = delete;

    virtual ~Shell() = 0;

protected:
    std::wostringstream &execution_yield_;
};
