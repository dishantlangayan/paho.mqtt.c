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
 *    Ian Craggs - change delimiter option from char to string
 *    Guilherme Maciel Ferreira - add keep alive option
 *    Dishant Langayan - ssl authentication options
 *******************************************************************************/

/*
 
 stdout subscriber
 
 compulsory parameters:
 
  --topic topic to subscribe to
 
 defaulted parameters:
 
	--host localhost
	--port 1883
	--qos 2
	--delimiter \n
	--clientid stdout-subscriber
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
#include "MQTTClient.h"
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
#endif


volatile int toStop = 0;


struct opts_struct
{
	char* clientid;
	int nodelimiter;
	char* delimiter;
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
	"stdout-subscriber", 0, "\n", 2, NULL, NULL, "localhost", "1883", 0, 10, NULL, NULL, NULL, NULL, 0
};


void usage(void)
{
	printf("MQTT stdout subscriber\n");
	printf("Usage: stdoutsub topicname <options>, where options are:\n");
	printf("  --host <hostname> (default is %s)\n", opts.host);
	printf("  --port <port> (default is %s)\n", opts.port);
	printf("  --qos <qos> (default is %d)\n", opts.qos);
	printf("  --delimiter <delim> (default is \\n)\n");
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


void myconnect(MQTTClient* client, MQTTClient_connectOptions* opts)
{
	printf("Connecting\n");
	int rc = 0;
	if ((rc = MQTTClient_connect(*client, opts)) != 0)
	{
		printf("Failed to connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
	printf("Connected\n");
}


void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}

void getopts(int argc, char** argv);

int main(int argc, char** argv)
{
	MQTTClient client;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
	char* topic = NULL;
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

	rc = MQTTClient_create(&client, url, opts.clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);

	conn_opts.keepAliveInterval = opts.keepalive;
	conn_opts.reliable = 0;
	conn_opts.cleansession = 1;
	conn_opts.username = opts.username;
	conn_opts.password = opts.password;

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
	
	myconnect(&client, &conn_opts);
	
	rc = MQTTClient_subscribe(client, topic, opts.qos);

	while (!toStop)
	{
		char* topicName = NULL;
		int topicLen;
		MQTTClient_message* message = NULL;
		
		rc = MQTTClient_receive(client, &topicName, &topicLen, &message, 1000);
		if (message)
		{
			if (opts.showtopics)
				printf("Topic: %s\t", topicName);
			if (opts.nodelimiter)
				printf("Message: %.*s", message->payloadlen, (char*)message->payload);
			else
				printf("Message: %.*s%s", message->payloadlen, (char*)message->payload, opts.delimiter);
			fflush(stdout);
			MQTTClient_freeMessage(&message);
			MQTTClient_free(topicName);
		}
		if (rc != 0)
			myconnect(&client, &conn_opts);
	}
	
	printf("Stopping\n");

	MQTTClient_disconnect(client, 0);

	MQTTClient_destroy(&client);

	return EXIT_SUCCESS;
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
				opts.delimiter = argv[count];
			else
				opts.nodelimiter = 1;
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
