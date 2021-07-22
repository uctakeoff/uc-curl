/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.haxx.se/libcurl/c/postit2.html
#include <iostream>
#include <stdexcept>
#include <string>
#include "../uccurl.h"

int main(int argc, char *argv[])
{
    try {
        uc::curl::form formpost;
        formpost
            .file("sendfile", "postit2.cpp")
            .contents("filename", "postit2.cpp")
            .contents("submit", "send");

        uc::curl::slist headerlist = uc::curl::create_slist("Expect:");

        uc::curl::easy curl("http://example.com/examplepost.cgi");
        if ((argc == 2) && (std::string{"noexpectheader"} != argv[1])) {
            curl.header(headerlist);
        }
        curl.postfields(formpost).perform();

    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
