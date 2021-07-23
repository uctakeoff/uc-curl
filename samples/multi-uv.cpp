/**
 * @file multi-uv.cpp
 * @author Kentaro Ushiyama
 * @brief multi_socket API using libuv
 * @date 2021-07-23
 * @copyright Copyright (c) 2021,
 * @see https://curl.se/libcurl/c/multi-uv.html
 */

/* Example application using the multi socket interface to download multiple
   files in parallel, powered by libuv.
 
   Requires libuv and (of course) libcurl.
 
   See https://nikhilm.github.io/uvbook/ for more information on libuv.
*/

#include "../uccurl.h"
#include <uv.h>
#include <vector>
#include <iostream>
#include <fstream>

void curl_action(uc::curl::multi& multi, curl_socket_t sockfd, int ev_bitmask = 0)
{
    const auto running_handles = multi.socket_action(sockfd, ev_bitmask);
    multi.for_each_done_info([&](uc::curl::easy_ref h, CURLcode result) {
        std::cout << h.uri() << " DONE\n";
    });
}
 
int main(int argc, char **argv)
{
    if(argc <= 1) return 0;

    uc::curl::global g{};
    uc::curl::multi multi{};

    uv_loop_t* loop = uv_default_loop();


    uv_timer_t timeout{};
    uv_timer_init(loop, &timeout);
    timeout.data = &multi;
    const auto timer_fun = [&timeout](uc::curl::multi_ref multi, long timeout_ms) {
        if (timeout_ms < 0) {
            uv_timer_stop(&timeout);
        } else {
            uv_timer_start(&timeout, [](uv_timer_t *req) {
                curl_action(*static_cast<uc::curl::multi*>(req->data), CURL_SOCKET_TIMEOUT, 0);
            }, std::max(1L, timeout_ms), 0);
        }
    };
    multi.on_timer(timer_fun);


    const auto socket_fun = [&](uc::curl::easy_ref easy, curl_socket_t sockfd, int action, uv_poll_t* poll_handle) {
        struct curl_context_t
        {
            uc::curl::multi* multi;
            curl_socket_t sockfd;
        };

        switch(action) {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT: {
            if (!poll_handle) {
                poll_handle = new uv_poll_t{};
                uv_poll_init_socket(loop, poll_handle, sockfd);
                poll_handle->data = new curl_context_t{&multi, sockfd};
                // std::cout << "    new    " << sockfd << ": " << poll_handle << ", " << poll_handle->data << "\n";
                multi.assign(sockfd, poll_handle);
            }
            int events{};
            if (action != CURL_POLL_IN)
                events |= UV_WRITABLE;
            if (action != CURL_POLL_OUT)
                events |= UV_READABLE;
            // std::cout << "    start  " << sockfd << ": " << poll_handle << ", " << poll_handle->data << "\n";
            uv_poll_start(poll_handle, events, [](uv_poll_t* poll_handle, int status, int events) {
                int flags{};
                if(events & UV_READABLE)
                    flags |= CURL_CSELECT_IN;
                if(events & UV_WRITABLE)
                    flags |= CURL_CSELECT_OUT;
                auto context = static_cast<curl_context_t*>(poll_handle->data);
                curl_action(*context->multi, context->sockfd, flags);
            });
            break;
        }
        case CURL_POLL_REMOVE:
            if (poll_handle) {
                multi.assign(sockfd, nullptr);
                uv_poll_stop(poll_handle);
                // std::cout << "    stop   " << sockfd << ": " << poll_handle << ", " << poll_handle->data << "\n";
                uv_close((uv_handle_t *)poll_handle, [](uv_handle_t *handle) {
                    // std::cout << "    delete " << ": " << handle << ", " << handle->data << "\n";
                    delete static_cast<curl_context_t*>(handle->data);
                    delete handle;
                });
            }
            break;
        default:
            abort();
        }
    };
    multi.on_socket<uv_poll_t>(socket_fun);
 

    std::vector<uc::curl::easy> handles;
    std::vector<std::unique_ptr<std::ofstream>> files;
    while(argc-- > 1) {
        const auto filename = std::to_string(argc).append(".download");
        const auto url = argv[argc];
        files.emplace_back(new std::ofstream(filename, std::ios::out | std::ios::binary));
        handles.push_back(uc::curl::easy(url).response(*files.back()));
        multi.add(handles.back());
        std::cerr << "Added download " << url << " -> " << filename << "\n";
    }

    uv_run(loop, UV_RUN_DEFAULT);

    for (auto& h: handles) {
        multi.remove(h);
    }
    handles.clear();
    return 0;
}