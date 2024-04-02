/**
 * state machine for http protocol parsing.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_HTTP_METHOD_LEN    32
#define MAX_HTTP_URL_LEN       2048
#define MAX_HTTP_VERSION_LEN   32
#define MAX_HTTP_HEADERS_LEN   8192
#define MAX_HTTP_BODY_LEN      8192

typedef enum HttpParseError {
    HPE_SUCCESS,
    HPE_METHOD_TOO_LONG,
    HPE_URL_TOO_LONG,
    HPE_VERSION_TOO_LONG,
    HPE_HEADERS_TOO_LONG,
    HPE_BODY_TOO_LONG,
    HPE_INVALID_CRLF
} HttpParseError;

typedef enum HttpParseState {
    HPS_METHOD,
    HPS_URL,
    HPS_VERSION,
    HPS_HEADERS,
    HPS_BODY
} HttpParseState;

typedef struct HttpRequest {
    char method[MAX_HTTP_METHOD_LEN + 1];
    char url[MAX_HTTP_URL_LEN + 1];
    char version[MAX_HTTP_VERSION_LEN + 1];
    char headers[MAX_HTTP_HEADERS_LEN + 1];
    char body[MAX_HTTP_BODY_LEN + 1];
} HttpRequest;

static const char* get_http_parse_error(HttpParseError e){
    switch(e){
        case HPE_SUCCESS:
            return "parse success";
        case HPE_METHOD_TOO_LONG:
            return "method too long";
        case HPE_URL_TOO_LONG:
            return "url too long";
        case HPE_VERSION_TOO_LONG:
            return "version too long";
        case HPE_HEADERS_TOO_LONG:
            return "headers too long";
        case HPE_BODY_TOO_LONG:
            return "body too long";
        case HPE_INVALID_CRLF:
            return "invalid crlf";
        default:
            return "unknown error";
    }
}

void* malloc_die(size_t len) {
    void* memory = malloc(len);
    if (memory == NULL){
        perror("out of memory");
        exit(EXIT_FAILURE);
    }

    return memory;
}

HttpParseError parse_http_request(const char* request, HttpRequest* hr) {
    HttpParseState state = HPS_METHOD;
    const char* ptr = request;
    int token_index = 0;

    while (*ptr != '\0') {
        switch(state){
            case HPS_METHOD:
                if (*ptr == ' '){
                    hr->method[token_index] = '\0';
                    state = HPS_URL;
                    token_index = 0;
                }
                else {
                    if (token_index == MAX_HTTP_METHOD_LEN){
                        return HPE_METHOD_TOO_LONG;
                    }

                    hr->method[token_index] = *ptr;
                    ++token_index;
                }

                break;
            case HPS_URL:
                if (*ptr == ' '){
                    hr->url[token_index] = '\0';
                    state = HPS_VERSION;
                    token_index = 0;
                }
                else {
                    if (token_index == MAX_HTTP_URL_LEN){
                        return HPE_URL_TOO_LONG;
                    }

                    hr->url[token_index] = *ptr;
                    ++token_index;
                }

                break;
            case HPS_VERSION:
                if (*ptr == '\r'){
                    if (ptr[1] != '\n'){
                        return HPE_INVALID_CRLF;
                    }

                    hr->version[token_index] = '\0';
                    state = HPS_HEADERS;
                    token_index = 0;
                    ++ptr;
                }
                else {
                    if (token_index == MAX_HTTP_VERSION_LEN){
                        return HPE_VERSION_TOO_LONG;
                    }

                    hr->version[token_index] = *ptr;
                    ++token_index;
                }

                break;
            case HPS_HEADERS:
                if (*ptr == '\r'){
                    if (ptr[1] != '\n'){
                        return HPE_INVALID_CRLF;
                    }
                    else if (ptr[2] == '\r' && ptr[3] == '\n'){
                        hr->headers[token_index] = '\0';
                        state = HPS_BODY;
                        token_index = 0;
                        ptr += 3;
                    }
                }
                else {
                    if (token_index == MAX_HTTP_HEADERS_LEN){
                        return HPE_HEADERS_TOO_LONG;
                    }

                    hr->headers[token_index] = *ptr;
                    ++token_index;
                }

                break;
            case HPS_BODY:
                if (token_index == MAX_HTTP_BODY_LEN){
                    return HPE_BODY_TOO_LONG;
                }

                hr->body[token_index] = *ptr;
                ++token_index;

                break;
            default:
                break;
        }

        ++ptr;
    }

    hr->body[token_index] = '\0';
    return HPE_SUCCESS;
}

int hex_to_decimal(char c) {
    if (c >= '0' && c <= '9'){
        return c - '0';
    }
    else if (c >= 'a' && c <= 'z'){
        return c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'Z'){
        return c - 'A' + 10;
    }

    return -1;
}

/**
 * decode the percent-encoding url.
*/
void decode_url(HttpRequest* hr) {
    char url[MAX_HTTP_URL_LEN];
    int urlLen = strlen(hr->url);
    strncpy(url, hr->url, urlLen);

    int i, ti;
    for (i = 0; i < urlLen;) {
        if (url[i] == '%' && i + 2 < urlLen) {
            hr->url[ti] = 16 * hex_to_decimal(url[i + 1]) + hex_to_decimal(url[i + 2]);
            i += 3;
        }
        else {
            hr->url[ti] = url[i];
            i += 1;
        }

        ++ti;
    }

    hr->url[ti] = '\0';
}

int main(){
    const char* request = "GET http://www.hatsunemiku.com/はつねみく HTTP/1.1\r\n"
                            "Host: www.example.com\r\n"
                            "Content-Length: 10\r\n"
                            "Accept-Encoding: utf-8\r\n"
                            "\r\n"
                            "Hello World";
    HttpRequest* hr = (HttpRequest*)malloc_die(sizeof(HttpRequest));
    decode_url(hr);

    int i = parse_http_request(request, hr);
    if (i != HPE_SUCCESS){
        printf("%s\n", get_http_parse_error(i));
    }
    else {
        printf("Method: %s\n", hr->method);
        printf("URL: %s\n", hr->url);
        printf("Version: %s\n", hr->version);
        printf("Headers: %s\n", hr->headers);
        printf("Body: %s\n", hr->body);
    }

    free(hr);
}
