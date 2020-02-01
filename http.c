#include <janet.h>
#include <curl/curl.h>

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static Janet c_send_request(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);

  CURL *curl;
  CURLcode res;

  struct MemoryStruct body;

  body.memory = malloc(1);  /* will be grown as needed by the realloc above */
  body.size = 0;    /* no data at this point */

  struct MemoryStruct headers;

  headers.memory = malloc(1);
  headers.size = 0;

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  const uint8_t *url = janet_getstring(argv, 0);
  JanetTable *options = janet_gettable(argv, 1);

  Janet janet_follow_redirects = janet_table_get(options, janet_ckeywordv("follow-redirects"));
  int follow_redirects = 0;

  if(janet_checktype(janet_follow_redirects, JANET_BOOLEAN)) {
    follow_redirects = janet_unwrap_boolean(janet_follow_redirects);
  }

  Janet janet_max_redirects = janet_table_get(options, janet_ckeywordv("max-redirects"));
  int max_redirects = 50;

  if(janet_checktype(janet_max_redirects, JANET_NUMBER)) {
    max_redirects = janet_unwrap_integer(janet_max_redirects);
  }

  Janet janet_user_agent = janet_table_get(options, janet_ckeywordv("user-agent"));
  char *user_agent = "janet http client";

  if(janet_checktype(janet_user_agent, JANET_STRING)) {
     user_agent = (char *)janet_unwrap_string(janet_user_agent);
  }

  Janet janet_keep_alive = janet_table_get(options, janet_ckeywordv("keep-alive"));
  int keep_alive = 1;

  if(janet_checktype(janet_keep_alive, JANET_BOOLEAN)) {
     keep_alive = janet_unwrap_boolean(janet_keep_alive);
  }

  Janet janet_request_body = janet_table_get(options, janet_ckeywordv("body"));
  char *request_body = NULL;

  if(janet_checktype(janet_request_body, JANET_STRING)) {
     request_body = (char *)janet_unwrap_string(janet_request_body);
  }

  Janet janet_method = janet_table_get(options, janet_ckeywordv("method"));
  char *method = NULL;

  if(janet_checktype(janet_method, JANET_STRING)) {
     method = (char *)janet_unwrap_string(janet_method);
  }

  JanetTable *response_table = janet_table(2);

  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // set http method
    if(method) {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    }

    // follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow_redirects);

     // max redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, max_redirects);

    // don't show progress
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    // user agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);

    // tcp keep alive
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, keep_alive);

    // request body
    if(request_body) {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    }

    // response body
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&body);

    // response headers
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&headers);

    // send request
    res = curl_easy_perform(curl);

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    /* Check for errors */
    if(res != CURLE_OK) {
      janet_table_put(response_table, janet_ckeywordv("error"), janet_wrap_string(curl_easy_strerror(res)));
    } else {
      janet_table_put(response_table, janet_ckeywordv("status"), janet_wrap_integer(response_code));
      janet_table_put(response_table, janet_ckeywordv("body"), janet_wrap_string(janet_string((const uint8_t *)body.memory, body.size)));
      janet_table_put(response_table, janet_ckeywordv("headers"), janet_wrap_string(janet_string((const uint8_t *)headers.memory, headers.size)));
    }

    /* cleanup */
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    free(body.memory);
    free(headers.memory);
  }

  return janet_wrap_table(response_table);
}

static const JanetReg cfuns[] = {
  {"send-request", c_send_request, "Sends a request with libcurl"},
  {NULL, NULL, NULL}
};

extern const unsigned char *http_lib_embed;
extern size_t http_lib_embed_size;

JANET_MODULE_ENTRY(JanetTable *env) {
  janet_cfuns(env, "http", cfuns);

  janet_dobytes(env,
        http_lib_embed,
        http_lib_embed_size,
        "http_lib.janet",
        NULL);
}