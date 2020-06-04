
# Table of Contents
- [Table of Contents](#table-of-contents)
  - [mod_mosquitto](#mod_mosquitto)
  - [License](#license)
  - [Contributor(s):](#contributors)
  - [Features](#features)
  - [How to build and install](#how-to-build-and-install)
  - [Configuration](#configuration)
  - [Usage](#usage)
  - [Command Line Interface (CLI)](#command-line-interface-cli)
  - [Notes](#notes)
## mod_mosquitto

mod_mosquitto is an interface to an MQTT broker using the Eclipse Mosquitto project C client library.  MQTT is a lightweight protocol to send or receive messages using a publish / subscribe pattern.  Echipse Mosquitto is an open source message broker that implements the MQTT protocol.

mod_mosquitto it able to publish and/or subscribe to one or more MQTT message brokers.
Implements a Publish/Subscribe (pub/sub) messaging pattern using the Mosquitto API library

More information can be found here:

* (MQTT)[http://mqtt.org/]
* (Mosquitto)[https://mosquitto.org/]

## License

FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>

Version: MPL 1.1

The contents of this file are subject to the Mozilla Public License Version 1.1 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for the specific language governing rights and limitations under the License.

The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application

The Initial Developer of the Original Code is Anthony Minessale II <anthm@freeswitch.org>
Portions created by the Initial Developer are Copyright (C) the Initial Developer. All Rights Reserved.

## Contributor(s):
* Norm Brandinger <n.brandinger@gmail.com>

## Features

* Global Settings
* Multiple Profiles
* Multiple MQTT broker connections
* Multiple Subscribers
* Multiple Publishers
* Multiple Topics
* TLS Support
* Last Will and Testament (Will) support

* Publish one or more FreeSWITCH events to one or more Topics
* Subscribe to FreeSWITCH bgapi or originate requests

## How to build and install

1.	Enable mod_mosquitto in the modules.conf file
2.	Execute configure;make;make install
3.	Restart FreeSWITCH

Note that mod_mosquitto requires libmosquitto-dev to build on Debian.

To install local modifications to mod_mosquitto:

1.	cd freeswitch/src/mod/event_handlers/mod_mosquitto
2.	Make any local modifications
3.	Execute make;make install
4.	Enter the FreeSWITCH console (fs_cli)
5.	Enter 'reload mod_mosquitto'

## Configuration

All configuration is done within the mosquitto.conf.xml file located in the freeswitch/conf/autoload_configs/mosquitto.conf.xml file.

The configuration follows the standard way all other FreeSWITCH modules are configured.

At a high level, the configuration file is organized as follows:

1.	Global / default settings are specified at the top level.
2.	One or more profiles can be defined.
3.	Within each profile, there can be one or more connections, publishers and subscribers defined.
4.	Within each publisher or subscriber, there can be one or more topics defined.
5.	Publisher topics can have one or more event parameters

Configuration parameter definitions

SETTINGS

	log-enable			- Is logging enabled (true) / disabled (false)
	log-details			- Is logging of message details enabled (true) / disabled (false)
	log-level			- Log severity level is one of: debug,info,notice,warning,error,critical,alert,console
	log-dir				- Directory where the log file(s) will be written. Defaults to 'freeswitch/log'
	log-file			- Name of the log file where mod_mosquitto messages will written.
						- Defaults to 'mosquitto.log'
	
	enable-profiles		- Default setting if unspecified in the configuration file
	enable-publishers	- Default setting if unspecified in the configuration file
	enable-subscribers	- Default setting if unspecified in the configuration file
	enable-connections	- Default setting if unspecified in the configuration file
	enable-topics		- Default setting if unspecified in the configuration file
	event-queue-size	- Number of pending events that the event queue can hold
	unique-string-length- Length of random characters added to FreeSWITCH-Switchname-Unique / FreeSWITCH-Hostname-Unique

PROFILE
	name				- Name of the profile. Each profile must have a unique name.
	enable				- Is this profile enabled (true) / disabled (false).
	
	log-enable			- Is logging enabled (true) / disabled (false) for this profile
	log-details			- Is logging of message details enabled (true) / disabled (false)
	log-level			- Log severity level is one of: debug,info,notice,warning,error,critical,alert,console
	log-dir				- Directory where the log file(s) will be written. Defaults to 'freeswitch/log'
	log-file			- Name of the log file where mod_mosquitto messages will written.
						- Defaults to the profile name + '.log'.

CONNECTIONS

CONNECTION
	name				- Name of this connection.  Each connection within a profile must have unique name.
	enable				- Is this connection enabled (true) / disabled (false).
	receive_maximum		- The maximum number of incoming QoS1 and QoS2 messages to be processed at once.
	send_maximum		- The maximum number of 'in-flight' QoS1 and QoS2 messages to be processed at once.
	host				- The hostname (FQDN) of the MQTT broker to connect to.
	port				- The port on the MQTT broker to connect to.
	bind_address		- the hostname or IP address of the local network interface to bind to.
	srv					- Should SRV lookups be performed (true/false).
	keepalive			- The MQTT keepalive value.
	username			- The username used by the MQTT broker to authorize the connection.
	password			- The password used by the MQTT broker to authenticate the connection.
	client_id			- The client id to use. If not specified a random client id will be generated.
						- FreeSWITCH-Switchname - Use the switchname as the client id.
						- FreeSWITCH-Switchname-Unique - Use the switchname with random characters appended as the client id.
						- FreeSWITCH-Hostname - Use the hostname as the client id.
						- FreeSWITCH-Hostname-Unique - Use the hostname with random characters appended as the client id.
	clean_session		- The MQTT clean sesson flag (frue/false)
	retries				- The maximum number of reconnect retries.
	reconnect_delay		- The number of seconds to wait between reconnects.
	reconnect_delay_max	- The maximum number of seconds to taie between reconnects.
	reconnect_exponsntial_backoff - Use exponential backoff between reconnect attempts (true/false).

	TLS
		enable			- Enabled (true) / disabled (false): Should SSL/TLS be enabled for this connection
		support			- 'certificate': The connection will be configured for certificated based SSL/TLS support.
						- 'pks': The connection will be configured for pre-shared-key based TLS support.
						- If not specified, the connection will NOT be configured for SSL/TLS and none of the settings below will have any effect.

		port			- Overrides the connection port
		cafile			- Path to a file containing the PEM encoded trusted CA certificate files.
						- Either cafile or capath must not be NULL.
		capath			- Path to a directory containing the PEM encoded trusted CA certificate files.
						- Either cafile or capath must not be NULL.
		certfile		- Path to a file containing the PEM encoded certificate file for this client.
						- If NULL, keyfile must also be NULL and no client certificate will be used.
		keyfile			- Path to a file containing the PEM encoded private key for this client.
						- If NULL, certfile must also be NULL and no client certificate will be used.

		advanced_options - enabled (true) / disabled (false): When enabled, 'cert_reqs', 'version', and 'opts_ciphers' are set for the connection.
		cert_reqs		- Verification requirements the connection will impose on the server
						- 'SSL_VERIFY_NONE': The server will not be verified in any way.
						- 'SSL_VERIFY_PEER': The server certificate will be verified and the connection aborted if the verification fails.
						- The default and recommended value is SSL_VERIFY_PEER.  Using SSL_VERIFY_NONE provides no security.
		opts_ciphers	- A string describing the ciphers available for use.  See the “openssl ciphers” tool for more information.
						- If NULL, the default ciphers will be used.
		version			- The version of the SSL/TLS protocol to use as a string.  If NULL, the default value is used.
						- The default value and the available values depend on the version of openssl that the library was compiled against.
						- For openssl >= 1.0.1, the available options are tlsv1.2, tlsv1.1 and tlsv1, with tlv1.2 as the default.
						- For openssl < 1.0.1, only tlsv1 is available.

		psk				- The pre-shared-key in hex format with no leading “0x”.
		identity		- The identity of this client.  May be used as the username depending on the server settings.
		psk_ciphers		- A string describing the PSK ciphers available for use.  See the “openssl ciphers” tool for more information.
						- If NULL, the default ciphers will be used.
	WILL
		enable			- Enable (true) or disable (false) the will associated with this connection.
						- The topic/payload of the will are published when the connection is closed.
		topic			- The topic name associated with this connecton.
		payload			- The payload data that will be published to the topic when the connection is closed.
		qos				- Message delivery Quality of Service
						- At most once (0)	- Best effort delivery. Mesage may not be delivered to the receiver.
						- At least once (1)	- Guarantees that the mesage will be delivered at least one time to the receiver (duplicates possible).
						- Exactly once (2)	- Guarentees that the message is received only once by the receiver.
		retain			- (true/false) The MQTT broker stores the last retained messae and the corresponding QoS for that topic.



PUBLISHER
	name				- Name of this publisher.  Each publisher within a profile must have unique name.
	enable				- Is this publisher enabled (true) / disabled (false).

TOPIC
	name				- Name of this topic.  Each topic within a profile/publisher must have unique name.
	enable				- Is this topic enabled (true) / disabled (false).
	connection_name		- The connection name associated with this topic.  The connection name must be defined within the same profile.
	event				- The FreeSWITCH event name that will be published to this topic.  Multiple events can be specified, one per param.
	pattern				- Pattern used to classify and filter different MQTT messages.  For example: FreeSWITCH/command or FreeSWITCH/heartbeat.
	qos					- Message delivery Quality of Service
						- At most once (0)	- Best effort delivery. Mesage may not be delivered to the receiver.
						- At least once (1)	- Guarantees that the mesage will be delivered at least one time to the receiver (duplicates possible).
						- Exactly once (2)	- Guarentees that the message is received only once by the receiver.
	retain				- (true/false) The MQTT broker stores the last retained messae and the corresponding QoS for that topic.

SUBSCRIBER
	name				- Name of this subscriber.  Each subscriber within a profile must have unique name.
	enable				- Is this subscriber enabled (true) / disabled (false).

TOPIC
	name				- Name of this topic.  Each topic within a profile/subscriber must have unique name.
	enable				- Is this topic enabled (true) / disabled (false).
	connection_name		- The connection name associated with this topic.  The connection name must be defined within the same profile.
	pattern				- Pattern used to classify and filter different MQTT messages.  For example: FreeSWITCH/command or FreeSWITCH/heartbeat.
	qos					- The requested Quality of Service for this subscription.
						- At most once (0)	- Best effort delivery. Mesage may not be delivered to the receiver.
						- At least once (1)	- Guarantees that the mesage will be delivered at least one time to the receiver (duplicates possible).
						- Exactly once (2)	- Guarentees that the message is received only once by the receiver.
	originate_authorized- Are originate commands authorized (true) / not authorized (false)
	bgapi_authorized	- Are bgapi commands authorized (true) / not authorized (false)


An empty configuration file showing the structure:
Note: Multiple profiles, connections, publishers, subscribers and topics are allowed.

<configuration>
	<settings>
	</settings>
	<profiles>
		<profile>
			<connections>
				<connection>
					<tls>
					</tls>
					<will>
					</will>
				</connection>
			</connections>
			<publishers>
				<publisher>
					<topics>
						<topic>
						</topic>
					</topics>
				</publisher>
			</publishers>
			<subscribers>
				<subscriber>
					<topics>
						<topic>
						</topic>
					</topics>
				</subscriber>
			</subscribers>
		</profile>
	</profiles>
</configuration>


An example configuration file:

<configuration name="mosquitto.conf" description="mod_mosquitto">
	<settings>
		<param name="log-enable" value="true"/>
        <!-- Default Log Level - value is one of: debug,info,notice,warning,error,critical,alert,console -->
        <param name="log-level" value="debug"/>
        <param name="log-dir" value="/usr/local/freeswitch/log"/>
        <param name="log-file" value="mosquitto.log"/>
		<param name="enable-profiles" value="true"/>
		<param name="enable-publishers" value="true"/>
		<param name="enable-subscribers" value="true"/>
		<param name="enable-connections" value="true"/>
		<param name="enable-topics" value="true"/>
		<param name="event-queue-size" value="5000"/>
		<param name="unique-string-length" value="10"/>
	</settings>
	<profiles>
		<profile name="default">
			<param name="enable" value="true"/>
			<param name="log-enable" value="true"/>
            <param name="log-level" value="debug"/>
            <param name="log-dir" value="/usr/local/freeswitch/log"/>
            <param name="log-file" value=""/>
			<connections>
				<param name="send_maximum" value="10"/>
				<param name="bind_address" value=""/>
				<connection name="broker1">
					<param name="enable" value="true"/>
					<param name="receive_maximum" value="20"/>
					<param name="send_maximum" value="20"/>
					<param name="host" value="broker1.example.com"/>
					<param name="port" value="1883"/>
					<param name="keepalive" value="10"/>
					<param name="username" value="[REDACTED]"/>
					<param name="password" value="[REDACTED]"/>
					<!-- mod_mosquitto will generate a random client_id if not specified -->
					<!-- FreeSWITCH-Hostname can be used to set the client_id to the FreeSWITCH hostname -->
					<!-- FreeSWITCH-Switchname can be used to set the client_id to the FreeSWITCH switchname -->
					<!-- FreeSWITCH-Switchname-Unique or FreeSWITCH-Hostname-Unique can be used to create a unique client_id -->
					<!-- This is done by appending random characters after the FreeSWITCH-Hostname or FreeSWITCH-Switchname -->
					<!-- The number of random characters appended is defined by setting 'unique-string-length' (see above) -->
					<!-- WARNING: Do not use the same client_id more than once per MQTT broker, this includes across profiles -->
					<param name="client_id" value="FreeSWITCH-Switchname-Unique"/>
					<param name="clean_session" value="false"/>
					<param name="retries" value="3"/>
					<param name="reconnect_delay" value="1"/>
					<param name="reconnect_delay_max" value="10"/>
					<param name="reconnect_exponential_backoff" value="false"/>
					<tls>
						<param name="enable" value="true"/>
						<param name="enable" value="certificate"/>
						<param name="advanced_options" value="true"/>
						<param name="port" value="8883"/>
						<param name="cafile" value=""/>
						<param name="capath" value="/etc/ssl/certs/"/>
						<param name="certfile" value=""/>
						<param name="keyfile" value=""/>
						<!-- <param name="cert_reqs" value="SSL_VERIFY_NONE"/> -->
						<!-- <param name="cert_reqs" value="SSL_VERIFY_PEER"/> -->
						<param name="cert_reqs" value=""/>
						<param name="version" value=""/>
						<param name="opts_ciphers" value=""/>
						<param name="psk" value=""/>
						<param name="identity" value=""/>
						<param name="psk_ciphers" value=""/>
					</tls>
					<will>
                        <param name="enable" value="true"/>
                        <param name="topic" value="FreeSWITCH/will"/>
                        <param name="payload" value="broker1 connection has been closed"/>
                        <param name="qos" value="0"/>
                        <param name="retain" value="true"/>
                    </will>
				</connection>
				<connection name="broker2">
					<param name="host" value="broker2.example.com"/>
					<param name="port" value="1883"/>
					<param name="username" value="[REDACTED]"/>
					<param name="password" value="[REDACTED]"/>
				</connection>
			</connections>
			<publishers>
				<publisher name="primary_publisher">
					<param name="enable" value="true"/>
					<topics>
						<topic name="Background Job">
							<param name="enable" value="true"/>
							<param name="connection_name" value="broker1"/>
							<!-- https://docs.freeswitch.org/switch__types_8h.html#ac9614049b0344bb672df9d23a7ec4a09 -->
							<param name="event" value="BACKGROUND_JOB"/>
							<param name="pattern" value="FreeSWITCH/BACKGROUND_JOB"/>
							<param name="qos" value="0"/>
							<param name="retain" value="false"/>
						</topic>
						<topic name="Multiple">
							<param name="enable" value="false"/>
							<param name="connection_name" value="broker2"/>
							<!-- https://docs.freeswitch.org/switch__types_8h.html#ac9614049b0344bb672df9d23a7ec4a09 -->
							<param name="event" value="HEARTBEAT"/>
							<param name="event" value="RE_SCHEDULE"/>
							<param name="pattern" value="FreeSWITCH/MULTIPLE"/>
							<param name="qos" value="0"/>
							<param name="retain" value="false"/>
						</topic>
					</topics>
				</publisher>
			</publishers>
			<subscribers>
				<subscriber name="primary_subscriber">
					<param name="enable" value="true"/>
					<topics>
						<topic name="primary_subscribed_topic_01">
							<param name="enable" value="true"/>
							<param name="connection_name" value="broker1"/>
							<param name="pattern" value="FreeSWITCH/command"/>
							<param name="qos" value="0"/>
							<!-- WARNING: THE FOLLOWING TWO SETTINGS WILL ALLOW THE MQTT BROKER TO INITIATE PHONE CALLS -->
							<!-- WARNING: Setting either of these permissons to true WILL allow phone calls to be initiated by the MQTT broker to FreeSWITCH. -->
							<!-- WARNING: If the connected MQTT broker is not locked down, toll fraud WILL HAPPEN by bad actors. -->
							<!-- WARNING: REPEATED: If the connected MQTT broker is not locked down, toll fraud WILL HAPPEN by bad actors. -->
							<!-- WARNING: REPEATED A THIRD TIME: If the connected MQTT broker is not locked down, toll fraud WILL HAPPEN by bad actors. -->
							<param name="originate_authorized" value="false"/>
							<param name="bgapi_authorized" value="false"/>
						</topic>
						<topic name="test_subscriber">
							<param name="enable" value="true"/>
							<param name="connection_name" value="broker2"/>
							<param name="pattern" value="FreeSWITCH/test"/>
							<param name="qos" value="0"/>
						</topic>
					</topics>
				</subscriber>
			</subscribers>
		</profile>
	</profiles>
</configuration>

## Usage

BGAPI

1.	Set up mod_mosquitto to publish the BACKGROUND_JOB event to a topic, for example: "FreeSWITCH/BACKGROUND_JOB".
2.	With your mqtt client subscribe to the above topic.
3.	Set up mod_mosquitto to subscribe to a topic with the "bgapi_authorized" permissions set, for example: "FreeSWITCH/command".
4.	With your MQTT client, publish to a topic mod_mosquitto is listening to, for example: "FreeSWITCH/command".
	The content of the published message should start with "bgapi", for example:
		bgapi version
		bgapi show registrations
5.	The responses will be published to the above topic, for example "FreeSWITCH/BACKGROUND_JOB".

ORIGINATE

1.	With your MQTT client, publish to a topic mod_mosquitto is listening to, for example: "FreeSWITCH/command".
2.	Set up mod_mosquitto to subscribe to a topic with the "originate_authorized" permissions set, for example: "FreeSWITCH/command".
3.	With your MQTT client, publish to a topic mod_mosquitto is listening to, for example: "FreeSWITCH/command".
	The content of the published message should start with "originate", for example:
		originate {origination_caller_id_number=19005551212}sofia/internal/sip:1000@192.168.1.1:58289 9386 XML default
		originate {origination_caller_id_number=9005551212}sofia/default/whatever@wherever 19005551212 XML default
4.	Note: Depending on the FreeSWITCH events being published, the result of the originate" command may, or may not be sent to the MQTT client.

SUBSCRIBE to a topic

Example of the the topic configuration to subscribe to a topic (and allowing both bgapi and originate commands):

Notes:
	1.	The topic name must be unique within a profile.
	2.	The topic can be enabled  or disabled by setting the "enable" param.
	3.	The connection_name must reference a connection previously defined in the configuration file.
	4.	The pattern must comply with MQTT standards.
	5.	The qos value must comply with MQTT standards. Value 0, 1 or 2 indicating the Quality of Service to be used.
	6.	The originate_authorized / bgapi_authorized permission flags.
		WARNING: Allowing either of these settings WILL allow phone calls to be initiated by the MQTT broker to FreeSWITCH.
		WARNING: If the connected MQTT broker is not locked down, toll fraud WILL HAPPEN by bad actors.
		WARNING: REPEATED: If the connected MQTT broker is not locked down, toll fraud WILL HAPPEN by bad actors.
		WARNING: REPEATED A THIRD TIME: If the connected MQTT broker is not locked down, toll fraud WILL HAPPEN by bad actors.

<subscriber>
	<topics>
		<topic name="command">
			<param name="enable" value="true"/>
			<param name="connection_name" value="mqtt_broker"/>
			<param name="pattern" value="FreeSWITCH/command"/>
			<param name="qos" value="0"/>
			<!-- WARNING EITHER OF THESE TWO SETTINGS WILL ALLOW THE MQTT BROKER TO INITIATE PHONE CALLS -->
			<param name="originate_authorized" value="true"/>
			<param name="bgapi_authorized" value="true"/>
		</topic>
	</topics>
</subscriber>


PUBLISH an event to a topic:

Notes:
	1.	The topic name must be unique within a profile.
	2.	The topic can be enabled or disabled by setting the "enable" param.
	3.	The connection_name must reference a connection previously defined in the configuration file.
	4.	The pattern must comply with MQTT standards.
	5.	The qos value must comply with MQTT standards. Value 0, 1 or 2 indicating the Quality of Service to be used.
	6.	The retain boolean must be either true or false.
	7.	The event parameter must contain a single FreeSWITCH supported event.
		Multiple event parameters be be added to a topic.

The example below shows mod_mosquitto publish the single event type, HEARTBEAT to a topic called Heartbeat.
In addition, mod_mosquitto will publish two event types, HEARTBEAT and RE_SCHEDULE to a topic called Miltiple.

The list of valid FreeSWITCH event types can be found here:  https://docs.freeswitch.org/switch__types_8h.html#ac9614049b0344bb672df9d23a7ec4a09
If the above link doesn't work, search the FreeSWITCH documentaton for "switch_event_types_t".
The event names used here are the same as those defined in "switch_event_types_t" with the removal of the "SWITCH_EVENT" prefix.

<publisher>
	<topics>
		<topic name="Heartbeat">
			<param name="enable" value="true"/>
			<param name="connection_name" value="mqtt_broker"/>
			<param name="event" value="HEARTBEAT"/>
			<param name="pattern" value="FreeSWITCH/HEARTBEAT"/>
			<param name="qos" value="0"/>
			<param name="retain" value="false"/>
		</topic>
		<topic name="Multiple">
			<param name="enable" value="false"/>
			<param name="connection_name" value="mqtt_broker"/>
			<param name="event" value="HEARTBEAT"/>
			<param name="event" value="RE_SCHEDULE"/>
			<param name="pattern" value="FreeSWITCH/MULTIPLE"/>
			<param name="qos" value="0"/>
			<param name="retain" value="false"/>
		</topic>
	</topics>
</publisher>

## Command Line Interface (CLI)

The following mod_mosquitto commands can be entered in the FreeSWITCH console (fs_api):

mosquitto [help]
mosquitto status
mosquitto loglevel [debug|info|notice|warning|error|critical|alert|console]
mosquitto enable profile <profile-name> [connection|publisher|subscriber] <name>
mosquitto disable profile <profile-name> [connection|publisher|subscriber] <name>
mosquitto remove profile <profile-name> [connection|publisher|subscriber] <name>
mosquitto connect profile <profile-name> connection <name>
mosquitto disconnect profile <profile-name> connection <name>
mosquitto bgapi <command> [<arg>]

## Notes

The file EVENT_TYPES is a list of the FreeSWITCH events that can be published.  This list is accurate at the time mod_mosquitto was created.

The list of available bgapi commands are available by entering "show api" in the FreeSWITCH console (fs_cli).

Possible TODO items:

	MQTT V5 support
	Throttle (circuit-breaker) support
	Enhanced security around command execution
	Enhanced CLI functionality
	Enhanced logging / statistics
 