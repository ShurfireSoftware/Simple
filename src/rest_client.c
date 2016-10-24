#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "rest_client.h"
#include "os.h"
#include "SCH_ScheduleTask.h"
#include "RMT_RemoteServers.h"
#include "Base64.h"

/* Local Constants and Definitions
*******************************************************************************/
#define REST_CLIENT_SOCKET_DELAY        1500
#define MAX_READ_SIZE       1024
#define FILE_BUFF_SIZE  510

#define DEBUG_DISPLAY_MESSAGE_DETAILS

#ifndef IPPORT_HTTP
#define IPPORT_HTTP 80
#endif

#ifndef IPPORT_HTTPS
#define IPPORT_HTTPS 443
#endif

const SOCKET_OPTIONS_STRUCT DEFAULT_OPTIONS = 
{
    1000,               //int32_t time_wait;
    60 * SEC_IN_MS,     //int32_t connection_timeout;
    30 * SEC_IN_MS,     //int32_t send_timeout; (old = 30 sec)
    2 * SEC_IN_MS,      //int32_t rto;
    0,                  //int32_t maxrto; (old = default = 0 = 4 min)
    0,                  //int32_t receive_timeout;
    REST_CLIENT_SOCKET_DELAY,     //int32_t rest_client_timeout;
    true,               //bool receive_no_wait;
    true,               //bool send_push;
    false,              //bool receive_push;
    false               //bool send_no_wait;
};

/* Local Function Declarations
*******************************************************************************/
static void find_response_status(char *p_str);
static void sslConnect(REST_CLIENT_QUERY_STRUCT_PTR p_query);
static void tcpConnect(REST_CLIENT_QUERY_STRUCT_PTR p_query);
static bool resolve_ip_address(char * hostname , struct in_addr * addr);

/* Local variables
*******************************************************************************/
static STATUS_RESPONSE ResponseStatus;
static struct in_addr RestServerIPAddr;

/*****************************************************************************//**
* @brief This function fills in default values for the query.
*
* @param p_query.  Pointer to REST_CLIENT_QUERY_STRUCT_PTR
* @param ssl_params.  pointer to SSL socket paramters or NULL
* @param method.  HTTPSRV_REQ_METHOD (GET, PUT, POST, DELETE)
* @param callback.  Pointer to function that is called on a return
* @return nothing.
* @author Neal Shurmantine
* @version
* 01/06/2016   Created.
*******************************************************************************/
void LoadDefaultClientData(REST_CLIENT_QUERY_STRUCT_PTR p_query,
                const RTCS_SSL_PARAMS_STRUCT * ssl_params,
                HTTPSRV_REQ_METHOD method,
                void * callback)
{
    memcpy((char*)&p_query->socket_options,(char*)&DEFAULT_OPTIONS, sizeof(SOCKET_OPTIONS_STRUCT));
    p_query->json = "";
    p_query->domain = RMT_GetDomainName();
    MakeAuthorizationString(p_query->authorize,true);
    p_query->status = eFWU_OK;
    p_query->ssl_params = ssl_params;
    if (p_query->ssl_params)
        p_query->server_port = IPPORT_HTTPS;
    else
        p_query->server_port = IPPORT_HTTP;
    p_query->method = method;
    p_query->callback = (uint32_t(*)(REST_CLIENT_QUERY_STRUCT_PTR param))callback;
}

/*****************************************************************************//**
* @brief Create an authorization string in base64.
*
* @param p_auth_str.  Pointer to buffer that will contain the string.
* @param use_default.  True if there is not a pin. (use 0000)
* @return nothing.  p_query->sock is not NULL if successful connection.
* @version
Sample:  "Authorization: Basic NUIwOEY0NUUwNTg1QTYwMDo3MDczY2Y2N2EzZTVmZTg2ZjdlYmQ5NjA5Y2Q3MTg1ZGVmYjk1ZmM4ZjM2MDYzYTZmYWNiZGU3YWE4YjAyOWMw
//hubID:hubKey
//size = 16+1+64 -> base64 = 108 bytes
//add 21 bytes for 'Authorization: Basic ' + 1 for null at end
//130 total characters
*******************************************************************************/
void MakeAuthorizationString(char *p_auth_str, bool use_default)
{
    char *p_remaining;
    char *tmp_str;
    uint16_t len;

    tmp_str = (char*)OS_GetMemBlock(ItsHubKeyLength_ + ItsMaxHubIdLength_ + 2); //add 1 for ':' and 1 for null termination
    strcpy(p_auth_str,BASIC_AUTH_HEADING_STR);
    p_remaining = p_auth_str + strlen(p_auth_str);

    strcpy(tmp_str,(char*)getHubId());
    strcat(tmp_str,":");
    strcat(tmp_str,(char*)getHubKey());

    len = strlen(tmp_str);
    ConvertToBase64(tmp_str, len, p_remaining);
    OS_ReleaseMemBlock(tmp_str);
}

/*****************************************************************************//**
* @brief Returns the IP address for the remote connect domain.
*
* @param pointer to string with domain name
* @return true if ip address found
* @author Neal Shurmantine
* @version
* 01/18/2016    Created
*******************************************************************************/
bool ResolveIpAddress(char *p_domain)
{
    if (resolve_ip_address(p_domain, &RestServerIPAddr) == true) {
        printf("Hunter Douglas server IP address: %s\n", inet_ntoa(RestServerIPAddr));
        return true;
    }
    else {
//        RestServerIPAddr = NULL;
        printf("could NOT resolve Hunter Douglas server %s\n",p_domain);
        return false;
    }
}

// Establish a regular tcp connection
static void tcpConnect(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    int error;
    struct hostent *host;
    struct sockaddr_in server;

    host = gethostbyname(p_query->domain);
    p_query->connection.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (p_query->connection.socket == -1) {
        p_query->status = eFWU_LOCAL_RESOURCE_ERROR;
        p_query->connection.socket = 0;
    }
    else {
        server.sin_family = AF_INET;
        server.sin_port = htons(p_query->server_port);
        server.sin_addr = *((struct in_addr *)host->h_addr);
        memset(&(server.sin_zero), 0, 8);
        error = connect(p_query->connection.socket, (struct sockaddr *)&server,
                       sizeof(struct sockaddr));
        if (error == -1) {
            p_query->status = eFWU_CANT_CONNECT_TO_SERVER;
            p_query->connection.socket = 0;
        }
    }
}

/*****************************************************************************//**
* @brief Establish a connection with the remote server.
*
* @param p_query.  Pointer to the query structure.
* @return nothing.  p_query->sock is not NULL if successful connection.
* @version
*******************************************************************************/
void ConnectToServer(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    p_query->connection.sslHandle = NULL;
    p_query->connection.sslContext = NULL;
    tcpConnect(p_query);

    if (p_query->connection.socket) {
        if (p_query->ssl_params) {
            sslConnect(p_query);
        }
    }
    else {
        p_query->status = eFWU_CANT_OBTAIN_SSL_SOCKET;
    }
}

// Establish a connection using an SSL layer
static void sslConnect(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    SOCKET_CONNECTION_STRUCT_PTR c;
    c = &p_query->connection;

    // Register the error strings for libcrypto & libssl
    SSL_load_error_strings();
    // Register the available ciphers and digests
    SSL_library_init();

    // New context saying we are a client, and using SSL 2 or 3
    c->sslContext = SSL_CTX_new(SSLv23_client_method());
    if (c->sslContext != NULL) {
        // Create an SSL struct for the connection
        c->sslHandle = SSL_new(c->sslContext);
        if (c->sslHandle != NULL) {
            // Connect the SSL struct to our connection
            if (SSL_set_fd(c->sslHandle, c->socket)) {
                // Initiate SSL handshake
                if (SSL_connect(c->sslHandle) != 1) {
                    p_query->status = eFWU_CANT_OBTAIN_SSL_SOCKET;
                }
            }
            else {
                p_query->status = eFWU_CANT_OBTAIN_SSL_SOCKET;
            }
        }
        else {
            p_query->status = eFWU_CANT_OBTAIN_SSL_SOCKET;
        }
    }
    else {
        p_query->status = eFWU_CANT_OBTAIN_SSL_SOCKET;
    }
}

// Read all available text from the connection
char *sslRead(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    const int readSize = MAX_READ_SIZE;
    char *rc = NULL;
    int received;
    int count = 0;
    char buffer[MAX_READ_SIZE];

    if (p_query->connection.sslContext) {
        while (1) {
            if (!rc)
                rc = malloc(readSize + 1);
            else
                rc = realloc(rc, ((count + 1) * readSize) + 1);

            received = SSL_read(p_query->connection.sslHandle, buffer, readSize);
            buffer[received] = '\0';

            if (received > 0)
                strcat(rc, buffer);

            if (received < readSize)
                break;
            count++;
        }
    }

    return rc;
}

/*****************************************************************************//**
* @brief Close and release the socket.
*
* @param p_query.  Pointer to the query structure.
* @return nothing.
* @version
*******************************************************************************/
void DisconnectFromServer(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    if (p_query->connection.socket)
        close(p_query->connection.socket);
    if (p_query->connection.sslHandle) {
        SSL_shutdown(p_query->connection.sslHandle);
        SSL_free(p_query->connection.sslHandle);
    }
    if (p_query->connection.sslContext)
        SSL_CTX_free(p_query->connection.sslContext);
}

/*****************************************************************************//**
* @brief Send the contents of the query buffer out the socket.
*
* @param p_query.  Pointer to the query structure.
* @param size.  Number of bytes to send.
* @return boolean.  True if transmission is successful.
* @version
*******************************************************************************/
static bool client_send(REST_CLIENT_QUERY_STRUCT_PTR p_query, int size) 
{
    if (p_query->connection.sslHandle) {
        p_query->resp_len = SSL_write(p_query->connection.sslHandle, p_query->buffer, size);
    }
    else {    
        p_query->resp_len = send(p_query->connection.socket, p_query->buffer, size,0);
    }

    if (p_query->resp_len <=0 ) {
        p_query->status = eFWU_CANT_SEND_TO_SERVER;
        printf("Send failed\n");
        return false;
    }
    return true;
}

/*****************************************************************************//**
* @brief Receive data from either the SSL or non-encrypted socket.
*
* @param p_query.  Pointer to the query buffer.
* @param buffer.  Pointer to the character array where the data is to be placed.
* @param size.  Number of bytes to receive.
* @return int32_t.  Number of bytes received.
* @version
*******************************************************************************/
int32_t RecvFromSocket(REST_CLIENT_QUERY_STRUCT_PTR p_query, char * buffer, uint32_t size)
{
    if (p_query->connection.sslHandle) {
        return SSL_read(p_query->connection.sslHandle, buffer, size);
    }
    else {    
        return recv(p_query->connection.socket, buffer, size,0);
    }
}

/*****************************************************************************//**
* @brief Begin receiving a packet from the remote server.
*
* @param p_query. Pointer to the query structure.
* @return bool.  True if reception is successful.
* @version
*******************************************************************************/
static bool client_receive(REST_CLIENT_QUERY_STRUCT_PTR p_query) 
{
    int32_t response;

    p_query->buffer[0]=0; 
    
    // For debugging, clear out old data so we don't look at 
    // stale information in the debugger.
    memset(p_query->buffer, 0,sizeof(p_query->buffer));

    // With HTTP, we will most likely get the complete response in one 
    // receive call. With HTTPS, we need to issue multiple recv calls,
    // One for the header, one for the chunk size, one for the chunk

    // we need to account for everything appearing in one read or in multiple reads.
    p_query->resp_len = RecvFromSocket(p_query, p_query->buffer, MAX_PACKET_SIZE);

    if (p_query->resp_len <= 0) {
        p_query->status = eFWU_NO_RESPONSE;
        return false;
    }

    find_response_status(p_query->buffer);

    #ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
        printf("Received: %s",p_query->buffer);
    #endif

    if (NULL != strcasestr(p_query->buffer,"Transfer-Encoding: chunked"))  {
        // Transfer encoding is being used.
        char *  temp =  endstrstr(p_query->buffer,"\r\n\r\n");
        size_t  left = p_query->resp_len - (temp - p_query->buffer);
        size_t chunk_size;

        if (left) {
            memcpy(p_query->buffer, temp, left);
        }
        p_query->resp_len = left;

        // Need at least one CR LF in buffer, that is the
        // line that contains the content length.
        temp = endstrstr(p_query->buffer,"\r\n");
        while (NULL==temp) {
            response = RecvFromSocket(p_query, &p_query->buffer[p_query->resp_len], MAX_PACKET_SIZE-p_query->resp_len);
    #ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
        printf("%s",p_query->buffer);
    #endif
            if (response <= 0) {
                printf("Receive1 failed\n");
                p_query->status = eFWU_CANT_RECEIVE_FROM_SERVER_1;
                return false;
            }
            p_query->resp_len += response;
            temp = endstrstr(p_query->buffer,"\r\n");
        }


        if (left == 0) {
            memset(p_query->buffer, 0, sizeof(p_query->buffer));
            p_query->resp_len = RecvFromSocket(p_query, p_query->buffer, MAX_PACKET_SIZE);
        #ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
            printf("%s",p_query->buffer);
        #endif
        }

        if (sscanf(p_query->buffer,"%x", &chunk_size)!=1) {
            printf("Parse failed\n");
            p_query->status = eFWU_CANT_PARSE_SERVER_RESPONSE;
            return false;
        }

        temp = endstrstr(p_query->buffer,"\r\n");
        // now we know how much data to read
        // Adjust the buffer so only chunk data is in the buffer
        left = p_query->resp_len - (temp - p_query->buffer);
        if (left) {
            memcpy(p_query->buffer, temp, left);
        }
        p_query->resp_len = left;

        // and fill the buffer up with the required data
        while (p_query->resp_len<chunk_size) {
            response = RecvFromSocket(p_query, &p_query->buffer[p_query->resp_len], chunk_size-p_query->resp_len);
        #ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
            printf("%s",&p_query->buffer[p_query->resp_len]);
        #endif
            if (response <= 0) {
                printf("Receive2 failed\n");
                p_query->status = eFWU_CANT_RECEIVE_FROM_SERVER_2;
                return false;
            }
            p_query->resp_len += response;
        }
    }
    else if (NULL != strcasestr(p_query->buffer,"Content-Length:"))  {
        //NOTE: firmware file is receive as Content-Length.  Packets are retrieved in
        //   FWU_FirmwareUpdate.c (function: get_image_file_return).
        // If chunked transfer encoding is not used, we should parse for content-length, 
        // and then read that amount of data. 
//        if (MAX_PACKET_SIZE > p_query->resp_len) {
//            size_t left = RecvFromSocket(p_query, &p_query->buffer[p_query->resp_len], MAX_PACKET_SIZE-p_query->resp_len);
//            p_query->resp_len += left;
//        }
    }
    else {
    }

    if (p_query->resp_len <= 0) {
        printf("Receive3 failed\n");
        if (p_query->status != eFWU_NO_RESPONSE) {
            p_query->status = eFWU_CANT_RECEIVE_FROM_SERVER_3;
        }
        return false;
    }
    else {
        p_query->buffer[p_query->resp_len] = 0;
    #ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
        printf("\n");
    #endif
        return true;
    }
}

/*****************************************************************************//**
* @brief This function starts the operation of sending a GET to the remote server.
*
* @param p_query.  Pointer to a REST_CLIENT_QUERY_STRUCT
* @return bool.  True if successful
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
bool GetResource(REST_CLIENT_QUERY_STRUCT_PTR p_query) 
{
    int32_t     size;

    if (p_query->ssl_params) {
        size = snprintf(p_query->buffer,
                                MAX_PACKET_SIZE,
                                REST_GET_HEADER,
                                p_query->resource,
                                p_query->domain,
                                p_query->authorize);
    }
    else {
        size = snprintf(p_query->buffer,
                                MAX_PACKET_SIZE,
                                REST_GET_HEADER_NO_AUTH,
                                p_query->resource,
                                p_query->domain);
    }

#ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
    printf("Send: %s \n", p_query->buffer);
#endif
    // send the request
    if (!client_send(p_query, size)) {
        return false;
    }
    OS_TaskSleep(p_query->socket_options.rest_client_timeout);

    // receive the response
    return client_receive(p_query);
}

/*****************************************************************************//**
* @brief This function starts the operation of sending a PUT to the remote server.
*
* @param p_query.  Pointer to a REST_CLIENT_QUERY_STRUCT
* @return bool.  True if successful
* @author Neal Shurmantine
* @version
* 02/24/2015    Created.
*******************************************************************************/
bool PutResource(REST_CLIENT_QUERY_STRUCT_PTR p_query, bool is_post)
{
    int32_t     size;
    char * p_hdr_str;

    if ((p_query->ssl_params) && (strlen(p_query->authorize)) ) {
        if (is_post) 
            p_hdr_str = REST_POST_HEADER;
        else
            p_hdr_str = REST_PUT_HEADER;

        size = snprintf(p_query->buffer,MAX_PACKET_SIZE,
                                    p_hdr_str, p_query->resource,
                                    p_query->domain,
                                    strlen(p_query->json),
                                    p_query->authorize,
                                    p_query->json);
    }
    else {
        if (is_post) 
            p_hdr_str = REST_POST_HEADER_NO_AUTH;
        else
            p_hdr_str = REST_PUT_HEADER_NO_AUTH;

        size = snprintf(p_query->buffer,MAX_PACKET_SIZE,
                                    p_hdr_str, p_query->resource,
                                    p_query->domain,
                                    strlen(p_query->json),
                                    p_query->json);
    }

//    restPrintHeaderFirstLine("Send: ", p_query->buffer);
#ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
    printf("Send: %s \n", p_query->buffer);
#endif
    // send the request
    if (!client_send(p_query, size)) {
        return false;
    }
    OS_TaskSleep(p_query->socket_options.rest_client_timeout);

    // receive the response
    return client_receive(p_query);
}

/*****************************************************************************//**
* @brief This function starts the operation of sending a DELETE to the remote server.
*
* @param p_query.  Pointer to a REST_CLIENT_QUERY_STRUCT
* @return bool.  True if successful
* @author Neal Shurmantine
* @version
* 11/30/2015    Created.
*******************************************************************************/
bool DeleteResource(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    int32_t     size;

    size = snprintf(p_query->buffer,MAX_PACKET_SIZE,
                                REST_DELETE_HEADER, p_query->resource,
                                p_query->domain,
                                p_query->authorize);

#ifdef DEBUG_DISPLAY_MESSAGE_DETAILS
    printf("Send: %s \n", p_query->buffer);
#endif
    // send the request
    if (!client_send(p_query, size)) {
        return false;
    }
    OS_TaskSleep(p_query->socket_options.rest_client_timeout);

    // receive the response
    return client_receive(p_query);
}

/*****************************************************************************//**
* @brief This function starts the operation of sending a POST to the remote server
*      that contains hub configuration data saved as JSON in a file on the
*      SD card.
*
* @param p_query.  Pointer to a REST_CLIENT_QUERY_STRUCT
* @param f_type.  JSON data type used for locating the correct file to send.
* @return bool.  True if successful
* @author Neal Shurmantine
* @version
* 11/06/2015    Created.
*******************************************************************************/
bool PostFileResource(REST_CLIENT_QUERY_STRUCT_PTR p_query)
{
    uint16_t size;
    char * p_buff;
    int32_t sent;

    size = snprintf(p_query->buffer,MAX_PACKET_SIZE,
                                REST_POST_HEADER_FILE, p_query->resource,
                                p_query->domain,
                                p_query->authorize,
                                RDS_GetJSONSize());

    printf("%s",p_query->buffer);

    if (p_query->connection.sslHandle) {
        sent = SSL_write(p_query->connection.sslHandle, p_query->buffer, size);
    }
    else {    
        sent = send(p_query->connection.socket, p_query->buffer, size,0);
    }

    p_buff = (char*)OS_GetMemBlock(FILE_BUFF_SIZE+3); //add 3 for terminating 0 and possibly appending CRLF
    FILE * p_file = RDS_OpenJSONFile();

    do {
        size = RDS_ReadJSONFile(p_buff, FILE_BUFF_SIZE, p_file);
        p_buff[size] = 0;

        if (p_query->connection.sslHandle) {
            sent = SSL_write(p_query->connection.sslHandle, p_buff, size);
        }
        else {    
            sent = send(p_query->connection.socket, p_buff, size,0);
        }

        if (sent <= 0) {
            p_query->status = eFWU_CANT_SEND_TO_SERVER;
            break;
        }

        if (size < (FILE_BUFF_SIZE)) {
            strcat(p_buff,"\n\r");
        }
        printf("%s",p_buff);
    } while (size==FILE_BUFF_SIZE);

    RDS_CloseJSONFile(p_file);
    OS_ReleaseMemBlock(p_buff);

    OS_TaskSleep(p_query->socket_options.rest_client_timeout);

    // receive the response
    return client_receive(p_query);
}

/*!
 * \brief finds a substring within a string, and returns a pointer to the end of the substring.
 *        strstr("foobar","foo") returns "foobar"
 *        endstrstr("foobar","foo") returns "bar"
 * 
 * \param[in] s1 - string to search
 * \param[in] s2- substring to earch for
 * 
 * \return pointer to the end of s2 in s1 if s2 is in s1, otherwise NULL.
 */
char * endstrstr(const char * s1, const char * s2)
{
    char * s;
    s = strstr(s1,s2);
    if (s) {
        s+=strlen(s2);
    }
    return s;
}



/*
 * Find the first occurrence of find in s, ignore case.
 */
char * strcasestr(const char *s1, const char *s2)
{
    size_t s1i, s2i, s1len,s2len;

    if ((NULL == s1) || (NULL==s2)) {
        return NULL;
    }

    s1len = strlen(s1);
    s2len = strlen(s2);

    if (s1len < s2len) {
        return NULL;
    }

    s1i=0;
    while (s1i<(s1len-s2len+1)) {
        s2i=0;
        while ((s2i < s2len) && (tolower(s1[s1i+s2i]) == tolower(s2[s2i]))) {
            s2i++;
        }
        if (s2i == s2len) {
            return (char *) &s1[s1i];
        }
        s1i++;
    }
    return NULL;
}

#ifdef USE_ME
int test(void)
{
    connection *c;
    int rtn = 0;
    char *response;

    c = sslConnect();

    char txBuff[1024];
#ifdef USE_REMOTE_ACTIONS
    strcpy(txBuff,"GET /api/v2/hubActions HTTP/1.1\r\n");
#else
    strcpy(txBuff,"GET /api/v2/times?tz=America%2FDenver&lat=40.005&lon=-105.092 HTTP/1.1\r\n");
#endif
    strcat(txBuff, "Accept-Language: en-us\r\n");
    strcat(txBuff,"Host: 192.168.1.100\r\n");
    strcat(txBuff,"Content-Type: application/json\r\n");
    strcat(txBuff,"Authorization: Basic NEZBQjZDN0E1NDU4Mzc2RjpmYTcyNTZkZTkzMGU3NTgwMTc3ZDA4NjQ1MDkwYmJkMDRhMjZhYzIyZmRkODk1N2IyZWZkNjVjNjRlNTI0NDhl\r\n");
    strcat(txBuff,"Content-Length: 0\r\n\r\n");


    client_send(c, txBuff);
    response = sslRead(c);

//  printf ("%s\n", response);
    delay(.1);
    response = sslRead(c);

printf("%s\n", response);

//  printf("\n\r");
    if(strstr(response,"Update")) {
        rtn = 1;
    }
    sslDisconnect(c);
    free(response);

    return rtn;
}
#endif

/*****************************************************************************//**
* @brief Parse HTML response header for status.
*
* @param p_str.  Pointer to start of response heasder.
* @param p_rsp.  Pointer to structure containing response code and phrase.
* @return bool.  True if format of response was OK.
* @author Neal Shurmantine
* @version
* 12/21/2015    Created.
*******************************************************************************/
STATUS_RESPONSE_PTR GetResponseStatus(void)
{
    return &ResponseStatus;
}

/*****************************************************************************//**
* @brief Examine first line of header for result code and string.
*     ie: HTTP/1.1 201 Created
*
* @param p_str.  Pointer to start of response heasder.
* @return nothing.  Global ResponseStatus info filled.
* @author Neal Shurmantine
* @version
* 12/21/2015    Created.
*******************************************************************************/
static void find_response_status(char *p_str)
{
    ResponseStatus.code = 500;
    ResponseStatus.phrase[0] = 0;
    bool is_good = true;
    char code[3];
    char *p_tmp;
    int i;
    p_tmp = p_str;
    while ((*p_tmp != '\r') && (*p_tmp != '\n') && (*p_tmp != ' ')) ++p_tmp;
    if (*p_tmp == ' ') {
        while (*p_tmp == ' ') ++p_tmp;
        for (i=0; i<3; ++i, ++p_tmp) {
            if ((*p_tmp >= '0') && (*p_tmp <= '9')) {
                code[i] = *p_tmp;
            }
            else {
                is_good = false;
                break;
            }
        }
        if (is_good == true) {
            code[i] = 0;
            ResponseStatus.code = atoi(code);
            while (*p_tmp == ' ') ++p_tmp;
            for (i=0; i<(MAX_RESPONSE_STRING_LEN-1); ++i, ++p_tmp) {
                if ((*p_tmp != '\n') && (*p_tmp != '\r')) {
                    ResponseStatus.phrase[i] = *p_tmp;
                }
                else {
                    break;
                }
            }
            ResponseStatus.phrase[i] = 0;
        }
    }
}

bool resolve_ip_address(char * hostname , struct in_addr * addr)
{
    bool rtn_val = false;
    struct hostent *he;
    struct in_addr **addr_list;

    if ( (he = gethostbyname( hostname ) ) != NULL)
    {
        addr_list = (struct in_addr **) he->h_addr_list;
        if (addr_list[0] != NULL) {
            *addr = *addr_list[0];
            rtn_val = true;
        }
    }
    return rtn_val;
}

