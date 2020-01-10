#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <curl/curl.h>

struct data_to_send {
    const char *data;
    size_t size;
};

static
size_t read_callback(void *dest,
                     size_t size,
                     size_t nmemb,
                     void *userp) {
    struct data_to_send *data = (struct data_to_send *)userp;
    size_t buffer_size = size * nmemb;

    if (data->size) {
        size_t copy_this_much = data->size;

        if (copy_this_much > buffer_size) {
            copy_this_much = buffer_size;
        }

        memcpy(dest, data->data, copy_this_much);

        data->data += copy_this_much;
        data->size -= copy_this_much;

        return copy_this_much;
    }

    return 0;
}

struct data_to_store {
    char *data;
    size_t size;
};

static
size_t
write_callback(void *contents,
               size_t size,
               size_t nmemb,
               void *userp) {
    size_t realsize = size * nmemb;
    struct data_to_store *data = (struct data_to_store *)userp;

    char *ptr = realloc(data->data, data->size + realsize + 1);

    if (ptr == NULL) {
        fprintf(stderr, "realloc() failed: %s\n",
                strerror(errno));
        return 0;
    }

    data->data = ptr;
    memcpy(&(data->data[data->size]), contents, realsize);

    data->size += realsize;
    data->data[data->size] = 0;

    return realsize;
}

int
http_post(const char *host,
          unsigned port,
          const char *path,
          const char *json,
          char **_response) {
    CURL *curl = NULL;
    CURLcode res;

    char *url = NULL;

    struct data_to_store response;

    response.data = malloc(1);
    response.size = 0;

    response.data[0] = 0;

    struct data_to_send data;

    data.data = json;
    data.size = strlen(json);

    res = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed: %s\n",
                curl_easy_strerror(res));
        return -1;
    }

    while (*path && *path == '/') path += 1;

    if (asprintf(&url, "%s:%u/%s", host, port, path) == -1) {
        fprintf(stderr, "asprintf() failed: %s\n",
                strerror(errno));
        return -1;
    }

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &data);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();
        
        *_response = strdup(response.data);

        return (int)res;
    }

    curl_global_cleanup();
    return -1;
}

int
http_get(const char *host,
         unsigned port,
         const char *path,
         char **_response) {
    CURL *curl = NULL;
    CURLcode res;

    char *url = NULL;

    struct data_to_store response;

    response.data = malloc(1);
    response.size = 0;

    response.data[0] = 0;


    res = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed: %s\n",
                curl_easy_strerror(res));
        return -1;
    }

    while (*path && *path == '/') path += 1;

    if (asprintf(&url, "%s:%u/%s", host, port, path) == -1) {
        fprintf(stderr, "asprintf() failed: %s\n",
                strerror(errno));
        return -1;
    }

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();
    
        *_response = strdup(response.data);

        return (int)res;
    }

    curl_global_cleanup();
    return -1;
}

int
main(int argc, const char **argv) {
    char *res = NULL;
    int ret = http_post("localhost", 5000, "/home", "Hello World", &res);

    printf("%s\n", res);
    free(res);
    return ret;
}
