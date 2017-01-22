/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.haxx.se/libcurl/c/hiperfifo.html
#include <iostream>
#include <stdexcept>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <event2/event.h>
#include "../uccurl.h"

#define MSG_OUT std::cerr

struct event_deleter
{
    void operator()(struct event* p) const noexcept
    {
        event_free(p);
    }
};
using event_ptr = std::unique_ptr<struct event, event_deleter>;

struct event_base_deleter
{
    void operator()(struct event_base* p) const noexcept
    {
        event_base_free(p);
    }
};
using event_base_ptr = std::unique_ptr<struct event_base, event_base_deleter>;


/* Create a named pipe and tell libevent to monitor it */ 
curl_socket_t create_fifo(const char* fifoname)
{
    MSG_OUT << "Creating named pipe \"" << fifoname << "\"\n";
    struct stat st;
    if(lstat(fifoname, &st) == 0) {
        if((st.st_mode & S_IFMT) == S_IFREG) {
            throw std::runtime_error("lstat");
        }
    }
    unlink(fifoname);
    if(mkfifo(fifoname, 0600) == -1) {
        throw std::runtime_error("mkfifo");
    }
    curl_socket_t sockfd = open(fifoname, O_RDWR | O_NONBLOCK, 0);
    if(sockfd == -1) {
        throw std::runtime_error("open");
    }
    return sockfd;
}


/* Information associated with a specific easy handle */ 
class ConnInfo
{
public:
    static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data)
    {
        return size * nmemb;
    }
#if LIBCURL_VERSION_NUM >= 0x072000
    static int prog_cb(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
#else
    static int prog_cb(void *p, double dltotal, double dlnow, double, double)
#endif
    {
        ConnInfo *conn = static_cast<ConnInfo*>(p);
        MSG_OUT << "Progress: " << conn->url << " (" << dlnow << "/" << dltotal << ")\n";
        return 0;
    }
    ConnInfo(const char* url_) : easy(url_), url(url_), error{}
    {
        easy.setopt(CURLOPT_WRITEFUNCTION, write_cb)
            .setopt(CURLOPT_WRITEDATA, this)
            .setopt(CURLOPT_ERRORBUFFER, error)
            .private_data(this)
            .clear<CURLOPT_NOPROGRESS>()
            .setopt<CURLOPT_VERBOSE>()
            .progress(prog_cb, this);
    }
    std::string url;
    uc::curl::easy easy;
    char error[CURL_ERROR_SIZE];
};

/* Information associated with a specific socket */ 
class SockInfo
{
public:
    /* Assign information to a SockInfo structure */ 
    void setsock(int act, event_ptr&& e)
    {
        action = act;
        ev = std::move(e);
    }
    int action = 0;
private:
    event_ptr ev;
};

/* Global information, common to all connections */ 
class GlobalInfo
{
public:
    /* This gets called whenever data is received from the fifo */ 
    static void fifo_cb(int, short, void *arg)
    {
        GlobalInfo* g = static_cast<GlobalInfo*>(arg);
        long int rv = 0;
        do {
            char url[1024] {};
            int n = 0;
            rv = fscanf(g->input, "%1023s%n", url, &n);
            url[n] = '\0';
            g->add_new_request(url);
        } while(rv != EOF);
    }
    /* Called by libevent when we get action on a multi socket */ 
    static void event_cb(int fd, short kind, void *userp)
    {
        const int action = (kind & EV_READ ? CURL_CSELECT_IN : 0)
                         | (kind & EV_WRITE ? CURL_CSELECT_OUT : 0);

        static_cast<GlobalInfo*>(userp)->socket_action(fd, action);
    }
    /* Called by libevent when our timeout expires */ 
    static void timer_cb(int, short, void *userp)
    {
        static_cast<GlobalInfo*>(userp)->socket_action(CURL_SOCKET_TIMEOUT, 0);
    }

    /* CURLMOPT_SOCKETFUNCTION */ 
    static int sock_cb(CURL* e, curl_socket_t sockfd, int what, GlobalInfo* g, SockInfo* fdp)
    {
        const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };

        MSG_OUT << "socket callback: sockfd=" << sockfd << " e=" << e << " what=" << whatstr[what] << " ";
        if(what == CURL_POLL_REMOVE) {
            MSG_OUT << "\n";
            delete fdp;
        } else {
            if(!fdp) {
                MSG_OUT << "Adding data: " << whatstr[what] << "\n";
                fdp = new SockInfo();
                g->multi.assign(sockfd, fdp);
            }
            else {
                MSG_OUT << "Changing action from " << whatstr[fdp->action] << " to " << whatstr[what] << "\n";
            }
            const int kind = (what & CURL_POLL_IN  ? EV_READ  : 0) |
                             (what & CURL_POLL_OUT ? EV_WRITE : 0) | EV_PERSIST;
            fdp->setsock(what, g->add_new_event(sockfd, kind, event_cb, g));
        }
        return CURL_SOCKOPT_OK;
    }
    /* Update the event timer after curl_multi library calls */ 
    static int multi_timer_cb(CURLM*, long timeout_ms, GlobalInfo *g)
    {
        MSG_OUT << "multi_timer_cb: Setting timeout to "<< timeout_ms << " ms\n";
        g->add_timer(timeout_ms);
        return 0;
    }

    GlobalInfo(const char* fifo) : fifoname(fifo), evbase(event_base_new())
    {
        const curl_socket_t sockfd = create_fifo(fifo);
        input = fdopen(sockfd, "r");
        MSG_OUT << "Now, pipe some URL's into > " << fifo << "\n";

        fifo_event = add_new_event(sockfd, EV_READ|EV_PERSIST, fifo_cb, this);

        timer_event.reset(evtimer_new(evbase.get(), timer_cb, this));
        /* setup the generic multi interface options we want */ 
        multi
            .setopt(CURLMOPT_SOCKETFUNCTION, sock_cb)
            .setopt(CURLMOPT_SOCKETDATA, this)
            .setopt(CURLMOPT_TIMERFUNCTION, multi_timer_cb)
            .setopt(CURLMOPT_TIMERDATA, this);
    }
    ~GlobalInfo()
    {
        fclose(input);
        unlink(fifoname.c_str());
    }

    void dispatch()
    {
        event_base_dispatch(evbase.get());
    }

    void add_new_request(const char* url)
    {
        if (url && url[0]) {
            ConnInfo* conn = new ConnInfo(url);
            MSG_OUT << "Adding easy "<< conn->easy.native_handle() << " to multi " << multi.native_handle() << " (" << url << ")\n";
            multi.add(conn->easy);
        }
    }
    int socket_action(int fd, int action)
    {
        int still_running = multi.socket_action(fd, action);
        // check_multi_info();
        MSG_OUT << "REMAINING: " << still_running << "\n";
        multi.for_each_done_info([&](uc::curl::easy_ref&& h, CURLcode result) {
            auto conn = h.private_data<ConnInfo>();
            MSG_OUT << "DONE: " << h.uri() << " (" << result << ") " << conn->error << "\n";
            multi.remove(h);
            delete conn;
        });
        if (still_running <= 0) {
            MSG_OUT << "last transfer done, kill timeout\n";
            remove_timer();
        }
        return still_running;
    }
    event_ptr add_new_event(evutil_socket_t fd, short events, event_callback_fn callback, void* arg)
    {
        event_ptr ret(event_new(evbase.get(), fd, events, callback, arg));
        event_add(ret.get(), NULL);
        return ret;
    }

    void add_timer(long timeout_ms)
    {
        struct timeval timeout = uc::curl::msec_to_timeval(timeout_ms);
        evtimer_add(timer_event.get(), &timeout);
    }
    void remove_timer()
    {
        if (evtimer_pending(timer_event.get(), NULL)) {
            evtimer_del(timer_event.get());
        }
    }
private:
    std::string fifoname;
    event_base_ptr evbase;
    event_ptr fifo_event;
    event_ptr timer_event;
    uc::curl::multi multi;
    FILE *input;
};

int main()
{
    try {
        uc::curl::global libcurlInit;

        GlobalInfo g("hiper.fifo");

        /* we don't call any curl_multi_socket*() function yet as we have no handles
            added! */ 
        g.dispatch();

    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
