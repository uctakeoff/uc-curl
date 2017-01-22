/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.haxx.se/libcurl/c/simple.html
#include <iostream>
#include <stdexcept>
#include "../uccurl.h"

int main()
{
    try {
        uc::curl::easy("http://example.com").perform();
    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
