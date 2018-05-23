/* libcurl wrapper */
#include "Curl.h"
#include <curl/curl.h>
#include "localptr.h"

static CURL * handle;

void Curl_init(void)
{
  curl_global_init(CURL_GLOBAL_ALL);
  handle = curl_easy_init();
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb,
                             void *userdata)
{
  String * result = userdata;
  // fprintf(stderr, "%s() result@%p\n", __func__, result);
  String_append_bytes(result, ptr, size*nmemb);
  return size*nmemb;
}


static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata)
{
  String_list * headers = userdata;
  localptr(String, header) = String_new("");
  String_append_bytes(header, buffer, size*nitems);
  String_list_append_s(headers, s(header));
  return size*nitems;
}

void post(const char * uri, String_list * headers, String * data,
          void (*handler)(int code, String_list * headers, String * data))
{
  int i;
  CURLcode res;
  
  if (handle == NULL) {
    fprintf(stderr, "curl handle is not set up!\n");
    return;
  }

  struct curl_slist *chunk = NULL;
  for (i=0; i < String_list_len(headers); i++) {
    chunk = curl_slist_append(chunk, s(String_list_get(headers, i)));
  }
  res = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, chunk);

  curl_easy_setopt(handle, CURLOPT_POSTFIELDS, s(data));
  curl_easy_setopt(handle, CURLOPT_URL, uri);
  //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);

  localptr(String, result) = String_new("");
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, result);

  localptr(String_list, out_headers) = String_list_new();
  curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(handle, CURLOPT_HEADERDATA, out_headers);
  
  res = curl_easy_perform(handle);
  if (res != CURLE_OK) {
    fprintf(stderr, "%s:%s() %s\n", __FILE__, __func__,
            curl_easy_strerror(res));
  }
  else {
    // printf("%s\n", s(result));
    if (handler) {
      handler(0, out_headers, result);
    }
    else {
      fprintf(stderr, "%s: no handler defined\n", __func__);
    }
  }
  curl_slist_free_all(chunk);  
}
