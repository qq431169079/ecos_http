#ifndef __UPNPHTTP_H__
#define __UPNPHTTP_H__
#include <sys/queue.h>
#ifndef __ECOS
#include <sys/un.h>
#endif

#include "mini_upnp_global.h"

/** @name UPNP_E_SUCCESS [0]
 *  {\tt UPNP_E_SUCCESS} signifies that the operation completed successfully.
 *  For asynchronous functions, this only means that the packet generated by 
 *  the operation was successfully transmitted on the network.  The result of 
 *  the entire operation comes as part of the callback for that operation.
 */
//@{
#define UPNP_E_SUCCESS          0
//@}

/** @name UPNP_E_INVALID_PARAM [-101]
 *  {\tt UPNP_E_INVALID_PARAM} signifies that one or more of the parameters 
 *  passed to the function is not valid.  Refer to the documentation for each 
 *  function for more information on the valid ranges of the parameters.
 */
//@{
#define UPNP_E_INVALID_PARAM    -101
//@}

/** @name UPNP_E_OUTOF_MEMORY [-104]
 *  {\tt UPNP_E_OUTOF_MEMORY} signifies that not enough resources are 
 *  currently available to complete the operation.  Most operations require 
 *  some free memory in order to complete their work.
 */
//@{
#define UPNP_E_OUTOF_MEMORY     -102
//@}

#define UPNP_E_TOOMANY_SUBSCRIBERS	-103

/* event ID */
#define UPNP_EVENT_SUBSCRIPTION_REQUEST 	1
#define UPNP_EVENT_RENEWAL_COMPLETE		2
#define UPNP_EVENT_UNSUBSCRIBE_COMPLETE	3

typedef struct _IPCon {
  char *ifname;
} _IPCon;

typedef struct _IPCon *IPCon;

/*
 States :
  0 - Waiting for data to read
  1 - Waiting for HTTP Post Content.
 ...
 >= 100 - to be deleted
*/
enum httpCommands {
	EUnknown = 0,
	EGet,
	EPost
};

struct Upnp_Document_element {
	char *VarName;
	char *message;
	LIST_ENTRY(Upnp_Document_element) entries;
};

typedef struct Upnp_Document_record {
	unsigned int TotalMessageLen; // (total strlen(VarName) * 2) + total strlen(message)
	unsigned short NumOfVarName;
	LIST_HEAD(DocumentHead, Upnp_Document_element) doc_head;
} Upnp_Document_CTX, *Upnp_Document;

struct EvtRespElement {
	int socket;
	char sid[SID_LEN];
	unsigned short TimeOut;
	LIST_ENTRY(EvtRespElement) entries;
};

struct process_upnp_subscription {
	char IP[IP_ADDRLEN];
	int IP_inet_addr;
	unsigned int port;
	char sid[SID_LEN];
	char callback_url[URL_MAX_LEN];
	unsigned long TimeOut;
};

struct upnp_subscription_element {
	char IP[IP_ADDRLEN];
	int IP_inet_addr;
	unsigned int port;
	char sid[SID_LEN];
	char callback_url[URL_MAX_LEN];
	unsigned long seq;
	unsigned long TimeOut;
	unsigned int subscription_timeout;
	int eventID;
	int wscdReNewState;  //add for wscd report renew state;2009-09-10
	LIST_ENTRY(upnp_subscription_element) entries;
};

struct upnp_subscription_record {
	char my_IP[IP_ADDRLEN];
	unsigned int my_port;
	unsigned short total_subscription;
	unsigned short max_subscription_num;
	char event_url[URL_MAX_LEN];
	unsigned short max_subscription_time;
	unsigned short subscription_timeout;
	void (*EventCallBack)(struct upnp_subscription_element *sub);
	LIST_HEAD(subscriptionlisthead, upnp_subscription_element) subscription_head;
	LIST_HEAD(EvtResplisthead, EvtRespElement) EvtResp_head;
};

struct upnphttp {
	int socket;
	/* Data structure added by Sean --begin-- */
	char IP[IP_ADDRLEN];
	struct _soapMethods *soapMethods;
	struct _sendDesc *sendDesc;
	struct upnp_subscription_record *subscribe_list;
	/* Data structure added by Sean --end-- */
	struct in_addr clientaddr;	/* client address */
	int state;
	char HttpVer[16];
	/* request */
	char * req_buf;
	int req_buflen;
	int req_contentlen;
	int req_contentoff;     /* header length */
	enum httpCommands req_command;
	char * req_soapAction;
	int req_soapActionLen;
	/* response */
	char * res_buf;
	int res_buflen;
	int res_buf_alloclen;
	/*int res_contentlen;*/
	/*int res_contentoff;*/		/* header length */
	LIST_ENTRY(upnphttp) entries;
};

struct _soapMethods {
	char * methodName;
	void (*methodImpl)(struct upnphttp *h);
};

struct _sendDesc {
	char * DescName;
	char * (*sendDescImpl)(int *len);
};

struct upnphttp * New_upnphttp(int);

void CloseSocket_upnphttp(struct upnphttp *);

void Delete_upnphttp(struct upnphttp *);

void Process_upnphttp(struct upnphttp *);

/* Build the header for the HTTP Response
 * Also allocate the buffer for body data */
void
BuildHeader_upnphttp(struct upnphttp * h, int respcode,
                     const char * respmsg,
                     int bodylen);
/* BuildResp_upnphttp() fill the res_buf buffer with the complete
 * HTTP 200 OK response from the body passed as argument */
void BuildResp_upnphttp(struct upnphttp *, const char *, int);

/* same but with given response code/message */
void
BuildResp2_upnphttp(struct upnphttp * h, int respcode,
                    const char * respmsg,
                    const char * body, int bodylen);

void SendResp_upnphttp(struct upnphttp *);

extern Upnp_Document CreatePropertySet(void);
extern int UpnpAddToPropertySet(Upnp_Document PropSet,
								const char *VarName, const char *message);
extern void UpnpSendEventSingle(Upnp_Document PropSet,
								struct upnp_subscription_record *subscribe_list,
								struct upnp_subscription_element *sub);
extern void UpnpSendEventAll(Upnp_Document PropSet,
							struct upnp_subscription_record *sub);
extern void UpnpDocument_free(Upnp_Document PropSet);
extern void ProcessEventingResp(struct EvtRespElement *EvtResp);
extern int OpenAndConfHTTPSocket(const char * addr, unsigned short port);
extern IPCon IPCon_New(char *ifname);
extern IPCon IPCon_Destroy(IPCon this);
#ifndef __ECOS
extern struct in_addr *IPCon_GetIpAddr(IPCon this);
#else
extern void IPCon_GetIpAddr(IPCon this, struct in_addr *inaddr);
#endif
extern char *IPCon_GetIpAddrByStr(IPCon this);
extern int ILibBase64Encode(unsigned char* input, const int inputlen, unsigned char** output);
extern int ILibBase64Decode(unsigned char* input, const int inputlen, unsigned char** output);
extern int ReliableSend(int socket, const char *data, const int len);

// support HNAP1
extern void SendError(struct upnphttp * h, const int code, const char *title, const char *realm, const char *body);
#ifndef __ECOS
extern int OpenAndConfUNIXSocket(const char *file_path);
extern int CreateUnixSocket(const char *function_name,
								const char *file_path,
								const int time_out);
extern int UnixSocketSendAndReceive(const char *function_name,
								const char *file_path,
								const int time_out,
								const char *in, char *out, const int out_len);
#endif
#endif


