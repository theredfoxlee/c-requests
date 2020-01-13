/* Hi, m8
 *
 * This file defines `http_post` and `http_get` functions
 *   -- they are simple wrappers over libcurl library.
 *
 * author: Kamil Janiec
 *
 * Behavior of these functions is based on libcurl examples:
 *   https://curl.haxx.se/libcurl/c/getinmemory.html
 *   https://curl.haxx.se/libcurl/c/post-callback.html
 */


#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <curl/curl.h>


/* `read_callback_data` is used to store data that is ought to be send
 * as HTTP request's body:
 * - `data` points to data that is going to be sent,  
 * - `size` represents actual size of the data.
 */
struct read_callback_data {
    const char *data;
    size_t size;
};

/* `write_callback_data` is used to store data that is received
 * as HTTP response's body:
 * - `data` points to data that is received,  
 * - `size` represents actual size of the data.
 */
struct write_callback_data {
    char *data;
    size_t size;
};

/* Function: read_callback
 * -----------------------
 * passes as many bytes as possible from `userp` to `dest`.
 *    It's a callback function registered as CURLOPT_READFUNCTION in libcurl.
 *
 *    Since bytes are passed over void* pointer, struct read_callback_data
 *    is required to store current state of passed data (e.g.: how many bytes
 *    are left to be send).
 *
 * Parameters in callback and return value are libcurl dependent.
 */
static size_t
read_callback(void *dest, size_t size, size_t nmemb, void *userp) {
    struct read_callback_data *data = (struct read_callback_data *) userp;
    size_t buffer_size = size * nmemb;

    if (data->size) {
        size_t copy_this_much = data->size;

        if (copy_this_much > buffer_size) {
            copy_this_much = buffer_size;
        }

        memcpy(dest, data->data, copy_this_much);

        /* NOTE: We're modifing passed struct. */
        data->data += copy_this_much;
        data->size -= copy_this_much;

        return copy_this_much;
    }

    return 0;
}

/* Function: write_callback
 * ------------------------
 * passes as many bytes as possible from `contents` to `userp`.
 *    It's a callback function registered as CURLOPT_WRITEFUNCTION in libcurl.
 *
 *    Since bytes are passed over void* pointer, struct write_callback_data
 *    is required to store current state of passed data.
 *
 * Parameters in callback and return value are libcurl dependent.
 */
static size_t
write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    struct write_callback_data *data = (struct write_callback_data *) userp;
    size_t real_size = size * nmemb;
    
    /* NOTE: We're modifing passed struct. */
    char *ptr = realloc(data->data, data->size + real_size + 1);

    if (ptr == NULL) {
        fprintf(stderr, "realloc() failed: %s\n", strerror(errno));
        return 0;
    }

    data->data = ptr;
    memcpy(&(data->data[data->size]), contents, real_size);

    data->size += real_size;
    data->data[data->size] = 0;

    return real_size;
}

/* Function: http_post
 * ------------------------
 * crates POST request with JSON as a body.
 *
 * NOTE: This function assumes that curl_global_init was spawned.
 *
 * host: host part from URI string
 * port: port part from URI string
 * path: path part from URI string
 * json: HTTP request's body
 * response: HTTP response body (allocated with malloc) / return value
 *
 * return: libcurl response code (x > 0), internal failure (-1)
 */
int
http_post(const char *host, unsigned port, const char *path,
          const char *json, char **response) {
    char url[PATH_MAX];
    
    while (*path && *path == '/') path += 1;

    if (snprintf(url, PATH_MAX, "%s:%u/%s", host, port, path) == -1) {
        fprintf(stderr, "snprintf() failed: %s\n", strerror(errno));
        return -1;
    }

    struct write_callback_data response_data;
    struct read_callback_data request_data;

    response_data.data = malloc(1);
    response_data.size = 0;

    request_data.data = json;
    request_data.size = strlen(json);

    CURL *curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);

        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) request_data.size);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &request_data);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "charsets: utf-8");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        *response = response_data.data;

        return (int)res;
    } else {
        fprintf(stderr, "curl_easy_init() failed: unknown reason\n");
        return -1;
    }
}

/* Function: http_get
 * ------------------------
 * crates GET request.
 *
 * NOTE: This function assumes that curl_global_init was spawned.
 *
 * host: host part from URI string
 * port: port part from URI string
 * path: path part from URI string
 * response: HTTP response body (allocated with malloc) / return value
 *
 * return: libcurl response code (x > 0), internal failure (-1)
 */
int
http_get(const char *host, unsigned port, const char *path,
         char **response) {
    char url[PATH_MAX];
    
    while (*path && *path == '/') path += 1;

    if (snprintf(url, PATH_MAX, "%s:%u/%s", host, port, path) == -1) {
        fprintf(stderr, "snprintf() failed: %s\n", strerror(errno));
        return -1;
    }

    struct write_callback_data response_data;

    response_data.data = malloc(1);
    response_data.size = 0;

    CURL *curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        *response = response_data.data;

        return (int)res;
    } else {
        fprintf(stderr, "curl_easy_init() failed: unknown reason\n");
        return -1;
    }
}
