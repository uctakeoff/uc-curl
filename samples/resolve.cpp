/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#include "../uccurl.h"

// See https://curl.se/libcurl/c/resolve.html

/* <DESC>
 * Use CURLOPT_RESOLVE to feed custom IP addresses for given host name + port
 * number combinations.
 * </DESC>
 */
int main()
{
    try {
        /* Each single name resolve string should be written using the format
            HOST:PORT:ADDRESS where HOST is the name libcurl will try to resolve,
            PORT is the port number of the service where libcurl wants to connect to
            the HOST and ADDRESS is the numerical IP address
        */
        auto host = uc::curl::create_slist("example.com:443:127.0.0.1");
        uc::curl::easy("https://example.com").setopt<CURLOPT_RESOLVE>(host).perform();
    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
