#include "config.h"

#include "curl_get.h"
#include "curl_stack.h"
#include "exceptions.h"

#include <ostream>
#include <curl/curl.h>
#include <curl/easy.h>

namespace torrent {

CurlGet::CurlGet(CurlStack* s) :
  m_out(NULL),
  m_handle(NULL),
  m_stack(s) {

  if (m_stack == NULL)
    throw internal_error("Tried to create CurlGet without a valid CurlStack");
}

void CurlGet::set_url(const std::string& url) {
  if (is_busy())
    throw local_error("Tried to call CurlGet::set_url on a busy object");

  m_url = url;
}

void CurlGet::set_out(std::ostream* out) {
  if (is_busy())
    throw local_error("Tried to call CurlGet::set_url on a busy object");

  m_out = out;
}

void CurlGet::start() {
  if (is_busy())
    throw local_error("Tried to call CurlGet::start on a busy object");

  if (m_out == NULL)
    throw local_error("Tried to call CurlGet::start without a valid output stream");

  m_handle = curl_easy_init();

  curl_easy_setopt(m_handle, CURLOPT_URL,           m_url.c_str());
  curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, &curl_get_receive_write);
  curl_easy_setopt(m_handle, CURLOPT_WRITEDATA,     this);
  curl_easy_setopt(m_handle, CURLOPT_USERAGENT,     PACKAGE "/" VERSION);
  curl_easy_setopt(m_handle, CURLOPT_FORBID_REUSE,  1);

  m_stack->add_get(this);
}

void CurlGet::close() {
  if (!is_busy())
    return;

  m_stack->remove_get(this);

  curl_easy_cleanup(m_handle);

  m_handle = NULL;
}

void CurlGet::perform(CURLMsg* msg) {
  if (msg->msg != CURLMSG_DONE)
    throw internal_error("CurlGet::process got CURLMSG that isn't done");

  if (msg->data.result == CURLE_OK) {
    m_done.emit();

  } else {
    m_failed.emit(curl_easy_strerror(msg->data.result));
  }
}

size_t curl_get_receive_write(void* data, size_t size, size_t nmemb, void* handle) {
  return ((CurlGet*)handle)->m_out->write((char*)data, size * nmemb).fail() ? 0 : size * nmemb;
}

}