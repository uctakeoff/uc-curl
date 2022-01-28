/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.se/libcurl/c/smtp-mime.html
/* <DESC>
 * SMTP example showing how to send mime emails
 * </DESC>
 */
 
#include <iostream>
#include "../uccurl.h"
 
/* This is a simple example showing how to send mime mail using libcurl's SMTP
 * capabilities. For an example of using the multi interface please see
 * smtp-multi.c.
 *
 * Note that this example requires libcurl 7.56.0 or above.
 */
 
#define FROM    "<sender@example.org>"
#define TO      "<addressee@example.net>"
#define CC      "<info@example.org>"
 
static const char inline_text[] =
  "This is the inline text message of the email.\r\n"
  "\r\n"
  "  It could be a lot of lines that would be displayed in an email\r\n"
  "viewer that is not able to handle HTML.\r\n";
 
static const char inline_html[] =
  "<html><body>\r\n"
  "<p>This is the inline <b>HTML</b> message of the email.</p>"
  "<br />\r\n"
  "<p>It could be a lot of HTML data that would be displayed by "
  "email viewers able to handle HTML.</p>"
  "</body></html>\r\n";
 
 
int main(void)
{
  try {
    /* This is the URL for your mailserver */
    uc::curl::easy curl{"smtp://mail.example.com"};

    /* Note that this option is not strictly required, omitting it will result
     * in libcurl sending the MAIL FROM command with empty sender data. All
     * autoresponses should have an empty reverse-path, and should be directed
     * to the address in the reverse-path which triggered them. Otherwise,
     * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
     * details.
     */
     curl.setopt<CURLOPT_MAIL_FROM>(FROM);
 
    /* Add two recipients, in this particular case they correspond to the
     * To: and Cc: addressees in the header, but they could be any kind of
     * recipient. */
    const auto recipients = uc::curl::create_slist(TO, CC);
    curl.setopt<CURLOPT_MAIL_RCPT>(recipients);
 

    /* Build and set the message header list. */
    const auto headers = uc::curl::create_slist(
        "Date: Tue, 22 Aug 2017 14:08:43 +0100",
        "To: " TO,
        "From: " FROM " (Example User)",
        "Cc: " CC " (Another example User)",
        "Message-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@rfcpedant.example.org>",
        "Subject: example sending a MIME-formatted message"
    );
    curl.setopt<CURLOPT_HTTPHEADER>(headers);

 
    /* The inline part is an alternative proposing the html and the text
       versions of the email. */
    auto alt = uc::curl::mime(curl);
    /* HTML message. */
    alt.addpart().data(inline_html).type("text/html");
    /* Text message. */
    alt.addpart().data(std::string{inline_text});

    /* Build the mime message. */
    auto mime = uc::curl::mime(curl);
    /* Create the inline part. */
    mime.addpart().subparts(std::move(alt)).type("multipart/alternative").headers(uc::curl::create_slist("Content-Disposition: inline")); 
    /* Add the current source program as an attachment. */
    mime.addpart().filedata("smtp-mime.cpp");
    curl.setopt<CURLOPT_MIMEPOST>(mime).perform();
 
    /* curl will not send the QUIT command until you call cleanup, so you
     * should be able to re-use this connection for additional messages
     * (setting CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and
     * calling curl_easy_perform() again. It may not be a good idea to keep
     * the connection open for a very long time though (more than a few
     * minutes may result in the server timing out the connection), and you do
     * want to clean up in the end.
     */
  } catch (std::system_error& ex) {
      std::cerr << "exception : " << ex.what() << std::endl;
      return ex.code().value();
  }
 
  return 0;
}
