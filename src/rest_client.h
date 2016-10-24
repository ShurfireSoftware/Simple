/***************************************************************************//**
 * @file rest_client.h
 * @brief Include file containing structures and definitions for use by rest_client.c.
 *
 * @author Neal Shurmantine
 * @copyright (c) 2015 Hunter Douglas. All rights reserved.
 *
 * @version
 * 02/18/2015   Created.
 *
 ******************************************************************************/
#ifndef _REST_CLIENT_H_
#define _REST_CLIENT_H_

#define MAX_PACKET_SIZE                 768
#define MAX_RESOURCE_NAME_LENGTH        96
#define MAX_AUTH_STRING_LEN             130
#define MAX_JSON_LENGTH                 256
#define MAX_DOMAIN_NAME_LENGTH          27
#define MAX_RESPONSE_STRING_LEN     36

// The form of a rest API request to the server. Note that the Host: is arbitrary, the server does
// not seem to care what it is. We may need to replace this with the brigde IP address.
#define REST_GET_HEADER     "GET %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "Content-Length: 0\r\n"\
   "%s\r\n"\
   "\r\n"

#define REST_GET_HEADER_NO_AUTH     "GET %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "Content-Length: 0\r\n"\
   "\r\n"


#define REST_PUT_HEADER     "PUT %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "Content-Length: %d\r\n"\
   "%s\r\n"\
   "\r\n%s"

#define REST_PUT_HEADER_NO_AUTH     "PUT %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "Content-Length: %d\r\n"\
   "\r\n%s"


#define REST_POST_HEADER_NO_AUTH     "POST %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "Content-Length: %d\r\n"\
   "\r\n%s"

#define REST_POST_HEADER     "POST %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "Content-Length: %d\r\n"\
   "%s\r\n"\
   "\r\n%s"

#define REST_POST_HEADER_FILE     "POST %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "%s\r\n"\
   "Content-Length: %d\r\n\r\n"

#define REST_DELETE_HEADER     "DELETE %s HTTP/1.1\r\n"\
   "Accept-Language: en-us\r\n"\
   "Host: %s\r\n"\
   "Content-Type: application/json\r\n"\
   "Content-Length: 0\r\n"\
   "%s\r\n"\
   "\r\n"


#define GET_AUTHORIZE_NULL      ""
#define BASIC_AUTH_HEADING_STR  "Authorization: Basic "


typedef enum {
    eFWU_OK=0,							//  0
    eFWU_CANT_CONNECT_TO_SERVER,		//  1
    eFWU_CANT_CONNECT_TO_SSL,			//  2
    eFWU_CANT_OBTAIN_SSL_SOCKET,		//  3
    eFWU_CANT_BIND_TO_SERVER,			//  4
    eFWU_CANT_RESOLVE_SERVER,			//  5
    eFWU_LOCAL_RESOURCE_ERROR,			//  6
    eFWU_CANT_SEND_TO_SERVER,			//  7
    eFWU_CANT_RECEIVE_FROM_SERVER_1,	//  8
    eFWU_CANT_RECEIVE_FROM_SERVER_2,	//  9
    eFWU_CANT_RECEIVE_FROM_SERVER_3,	// 10
    eFWU_CANT_RECEIVE_FROM_SERVER_4,	// 11
    eFWU_NO_RESPONSE,					// 12
    eFWU_CANT_PARSE_SERVER_RESPONSE,	// 13
    eFWU_CANT_PARSE_UPDATE_URL,			// 14
    eFWU_CANT_PARSE_FILE_URL,			// 15
    eFWU_CANT_WRITE_VERSION_FILE,		// 16
    eFWU_CANT_RETRIEVE_FILE,			// 17
    eFWU_CANT_CREATE_LOCAL_FILE,		// 18
    eFWU_CANT_WRITE_LOCAL_FILE,			// 19
    eFWU_CANT_COMPUTE_MD5_HASH_OF_FILE,	// 20
    eFWU_DOWNLOAD_INCOMPLETE,			// 21
    eFWU_MD5_HASH_CHECK_ERROR			// 22
} eRestClientStatus;


#define HTTPSRV_CODE_OK                         (200)
#define HTTPSRV_CODE_CREATED                    (201)
#define HTTPSRV_CODE_NO_CONTENT                 (204)
#define HTTPSRV_CODE_BAD_REQ                    (400)
#define HTTPSRV_CODE_UNAUTHORIZED               (401)
#define HTTPSRV_CODE_FORBIDDEN                  (403)
#define HTTPSRV_CODE_NOT_FOUND                  (404)
#define HTTPSRV_CODE_NO_LENGTH                  (411)
#define HTTPSRV_CODE_URI_TOO_LONG               (414)
#define HTTPSRV_CODE_INTERNAL_ERROR             (500)
#define HTTPSRV_CODE_NOT_IMPLEMENTED            (501)

#include "stub.h"
#include <openssl/ssl.h>

typedef struct SOCKET_OPTIONS_TAG
{
    int32_t time_wait;
    int32_t connection_timeout;
    int32_t send_timeout;
    int32_t rto;
    int32_t maxrto;
    int32_t receive_timeout;
    int32_t rest_client_timeout;
    bool    receive_no_wait;
    bool    send_push;
    bool receive_push;
    bool send_no_wait;
} SOCKET_OPTIONS_STRUCT, *SOCKET_OPTIONS_STRUCT_PTR;

typedef struct {
    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;
} SOCKET_CONNECTION_STRUCT, *SOCKET_CONNECTION_STRUCT_PTR;

typedef struct REST_CLIENT_QUERY_TAG
{
    eRestClientStatus status;
    SOCKET_CONNECTION_STRUCT connection;
    HTTPSRV_REQ_METHOD method;
    char *domain;
    char authorize[MAX_AUTH_STRING_LEN];
    char resource[MAX_RESOURCE_NAME_LENGTH];
    char *json;
    uint32_t(*callback)(struct REST_CLIENT_QUERY_TAG *param);
    char *save_file_name;
    SOCKET_OPTIONS_STRUCT socket_options;
    const RTCS_SSL_PARAMS_STRUCT * ssl_params;
    int32_t resp_len;
    uint32_t            timeout;            /* Session timeout in ms. timeout_time = time + timeout */
    char buffer[MAX_PACKET_SIZE];
    uint16_t server_port;
} REST_CLIENT_QUERY_STRUCT, * REST_CLIENT_QUERY_STRUCT_PTR;

typedef struct STATUS_RESPONSE_TAG
{
    int code;
    char phrase[MAX_RESPONSE_STRING_LEN];
} STATUS_RESPONSE, *STATUS_RESPONSE_PTR;

bool ResolveIpAddress(char *p_domain);
void ClearIpAddress(void);
int32_t RecvFromSocket(REST_CLIENT_QUERY_STRUCT_PTR p_query, char * buffer, uint32_t size);
void ConnectToServer(REST_CLIENT_QUERY_STRUCT_PTR p_query);
void LoadDefaultClientData(REST_CLIENT_QUERY_STRUCT_PTR p_query,
                const RTCS_SSL_PARAMS_STRUCT * ssl_params,
                HTTPSRV_REQ_METHOD method,
                void * callback);
bool GetResource(REST_CLIENT_QUERY_STRUCT_PTR p_query);
bool PutResource(REST_CLIENT_QUERY_STRUCT_PTR p_query, bool is_post);
bool PostFileResource(REST_CLIENT_QUERY_STRUCT_PTR p_query);
bool DeleteResource(REST_CLIENT_QUERY_STRUCT_PTR p_query);
void DisconnectFromServer(REST_CLIENT_QUERY_STRUCT_PTR p_query);
void restPrintHeaderFirstLine(char * type, char * p_packet);
void MakeAuthorizationString(char *p_auth_str,bool use_default);
STATUS_RESPONSE_PTR GetResponseStatus(void);

// extensions to string library.
// TODO: Move to string utility file
char * endstrstr(const char * s1, const char * s2);
char * strcasestr(const char *s, const char *find);

#endif
