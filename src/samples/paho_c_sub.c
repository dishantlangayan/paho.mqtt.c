/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *    Ian Craggs - fix for bug 413429 - connectionLost not called
 *    Guilherme Maciel Ferreira - add keep alive option
 *    Dishant Langayan - ssl authentication options
 *******************************************************************************/

/*
 
 stdout subscriber for the asynchronous client
 
 compulsory parameters:
 
  --topic topic to subscribe to
 
 defaulted parameters:
 
	--host localhost
	--port 1883
	--qos 2
	--delimiter \n
	--clientid stdout-subscriber-async
	--showtopics off
	--keepalive 10
	
	--userid none
	--password none

	--client-key none
	--client-key-pass none
	--client-privatekey none
	--server-key none
	--server-cert-auth off
 
*/

#include "MQTTAsync.h"
#include "MQTTClientPersistence.h"

#include <stdio.h>
#include <signal.h>
#include <memory.h>


#if defined(WIN32)
#include <windows.h>
#define sleep Sleep
#else
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#endif


volatile int finished = 0;
char* topic = NULL;
int subscribed = 0;
int disconnected = 0;


void cfinish(int sig)
{
	signal(SIGINT, NULL);
	finished = 1;
}


struct
{
	char* clientid;
	int nodelimiter;
	char delimiter;
	int qos;
	char* username;
	char* password;
	char* host;
	char* port;
	int showtopics;
	int keepalive;
	char* client_key_file;
	char* client_key_pass;
	char* client_private_key;
	char* server_key_file;
	int server_cert_auth;
} opts =
{
	"stdout-subscriber-async", 1, '\n', 2, NULL, NULL, "localhost", "1883", 0, 10, NULL, NULL, NULL, NULL, 0
};


void usage(void)
{
	printf("MQTT stdout subscriber\n");
	printf("Usage: stdoutsub topicname <options>, where options are:\n");
	printf("  --host <hostname> (default is %s)\n", opts.host);
	printf("  --port <port> (default is %s)\n", opts.port);
	printf("  --qos <qos> (default is %d)\n", opts.qos);
	printf("  --delimiter <delim> (default is no delimiter)\n");
	printf("  --clientid <clientid> (default is %s)\n", opts.clientid);
	printf("  --username none\n");
	printf("  --password none\n");
	printf("  --showtopics <on or off> (default is on if the topic has a wildcard, else off)\n");
	printf("  --keepalive <seconds> (default is %d seconds)\n", opts.keepalive);
	printf("SSL authentication options:\n");
	printf("  --client-key <key_file> - Use <key_file> as the client certificate for SSL authentication\n");
	printf("  --client-key-pass <password> - Use <password> to access the private key in the client certificate\n");
	printf("  --client-privatekey <file> - Client private key file if not in certificate file\n");
	printf("  --server-key <key_file> - Use <key_file> as the trusted certificate for server\n");
	printf("  --server-cert-auth <on or off> - Validate server certificates (default is off)\n");
	exit(EXIT_FAILURE);
}


void getopts(int argc, char** argv)
{
	int count = 2;
	
	while (count < argc)
	{
		if (strcmp(argv[count], "--qos") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "0") == 0)
					opts.qos = 0;
				else if (strcmp(argv[count], "1") == 0)
					opts.qos = 1;
				else if (strcmp(argv[count], "2") == 0)
					opts.qos = 2;
				else
					usage();
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--host") == 0)
		{
			if (++count < argc)
				opts.host = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--port") == 0)
		{
			if (++count < argc)
				opts.port = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--clientid") == 0)
		{
			if (++count < argc)
				opts.clientid = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--username") == 0)
		{
			if (++count < argc)
				opts.username = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--password") == 0)
		{
			if (++count < argc)
				opts.password = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--delimiter") == 0)
		{
			if (++count < argc)
			{
				if (strcmp("newline", argv[count]) == 0)
					opts.delimiter = '\n';
				else
					opts.delimiter = argv[count][0];
				opts.nodelimiter = 0;
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--showtopics") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "on") == 0)
					opts.showtopics = 1;
				else if (strcmp(argv[count], "off") == 0)
					opts.showtopics = 0;
				else
					usage();
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--keepalive") == 0)
		{
			if (++count < argc)
				opts.keepalive = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--client-key") == 0)
		{
			if (++count < argc)
				opts.client_key_file = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--client-key-pass") == 0)
		{
			if (++count < argc)
				opts.client_key_pass = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--client-privatekey") == 0)
		{
			if (++count < argc)
				opts.client_private_key = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--server-key") == 0)
		{
			if (++count < argc)
				opts.server_key_file = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--server-cert-auth") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "on") == 0)
					opts.server_cert_auth = 1;
				else if (strcmp(argv[count], "off") == 0)
					opts.server_cert_auth = 0;
				else
					usage();
			}
			else
				usage();
		}
		count++;
	}
	
}


int messageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
	if (opts.showtopics)
		printf("Topic: %s\t", topicName);
	if (opts.nodelimiter)
		printf("Message: %.*s", message->payloadlen, (char*)message->payload);
	else
		printf("Message: %.*s%c", message->payloadlen, (char*)message->payload, opts.delimiter);
	fflush(stdout);
	MQTTAsync_freeMessage(&message);
	MQTTAsync_free(topicName);
	return 1;
}


void onDisconnect(void* context, MQTTAsync_successData* response)
{
	disconnected = 1;
}


void onSubscribe(void* context, MQTTAsync_successData* response)
{
	subscribed = 1;
}


void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Subscribe failed, rc %d\n", response ? response->code : -1);
	finished = 1;
}


void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Connect failed, rc %d\n", response ? response->code : -1);
	finished = 1;
}


void onConnect(void* context, MQTTAsync_successData* response)
{
	printf("Connected\n");
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions ropts = MQTTAsync_responseOptions_initializer;
	int rc;

	if (opts.showtopics)
		printf("Subscribing to topic %s with client %s at QoS %d\n", topic, opts.clientid, opts.qos);

	ropts.onSuccess = onSubscribe;
	ropts.onFailure = onSubscribeFailure;
	ropts.context = client;
	if ((rc = MQTTAsync_subscribe(client, topic, opts.qos, &ropts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start subscribe, return code %d\n", rc);
		finished = 1;
	}
}


MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;


void connectionLost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	int rc;

	printf("connectionLost called\n");
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start reconnect, return code %d\n", rc);
		finished = 1;
	}
}


int main(int argc, char** argv)
{
	MQTTAsync client;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
	MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
	int rc = 0;
	char url[100];
	
	if (argc < 2)
		usage();
	
	topic = argv[1];

	if (strchr(topic, '#') || strchr(topic, '+'))
		opts.showtopics = 1;
	if (opts.showtopics)
		printf("topic is %s\n", topic);

	getopts(argc, argv);	
	sprintf(url, "%s:%s", opts.host, opts.port);

	rc = MQTTAsync_create(&client, url, opts.clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, client, connectionLost, messageArrived, NULL);

	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);

	conn_opts.keepAliveInterval = opts.keepalive;
	conn_opts.cleansession = 1;
	conn_opts.username = opts.username;
	conn_opts.password = opts.password;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;

	// SSL authentication options
	conn_opts.ssl = &ssl_opts;
	if (opts.server_key_file != NULL)
		conn_opts.ssl->trustStore = opts.server_key_file; /*file of certificates trusted by client*/
	if (opts.client_key_file != NULL)
		conn_opts.ssl->keyStore = opts.client_key_file; /*file of certificate for client to present to server*/
	if (opts.client_key_pass != NULL)
		conn_opts.ssl->privateKeyPassword = opts.client_key_pass;
	if (opts.client_private_key != NULL)
		conn_opts.ssl->privateKey = opts.client_private_key; /*private key file in not in certificate key file*/
	conn_opts.ssl->enableServerCertAuth = opts.server_cert_auth; /*validate server certificates*/

	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

	while (!subscribed)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	if (finished)
		goto exit;

	while (!finished)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	disc_opts.onSuccess = onDisconnect;
	if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start disconnect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

	while	(!disconnected)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

exit:
	MQTTAsync_destroy(&client);

	return EXIT_SUCCESS;
}


