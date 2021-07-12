// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2018, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_IO_BASE64CODER_H
#define INCLUDE_IO_BASE64CODER_H

#include <string>
#include <exception>

namespace io
{
    class Base64Coder
    {
    public:
        static std::string encode(const std::string&);
        static std::string decode(const std::string&);

        class IllegalLengthException : public std::exception
        {
        public:
            const char* what() const noexcept override;
        };

        class IllegalCharacterException : public std::exception
        {
        public:
            const char* what() const noexcept override;
        };

    private:
        static Base64Coder instance;

        Base64Coder();
        char mapTo64[64]{};
        char mapFrom64[128]{};
    };
}  // namespace io

#endif  // INCLUDE_IO_BASE64CODER_H
