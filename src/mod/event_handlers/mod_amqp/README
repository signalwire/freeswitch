                      _                                
  _ __ ___   ___   __| |    __ _ _ __ ___   __ _ _ __  
 | '_ ` _ \ / _ \ / _` |   / _` | '_ ` _ \ / _` | '_ \ 
 | | | | | | (_) | (_| |  | (_| | | | | | | (_| | |_) |
 |_| |_| |_|\___/ \__,_|___\__,_|_| |_| |_|\__, | .__/ 
                      |_____|                 |_|_|    by Aeriandi


Contents
--------

1.  Features
2.  How to build and install
3.  Configuration
4.  Usage
5.  Trouleshooting

6.  Notes



1. Features
-----------

*  Authenticates with an AMQP broker such as RabbitMQ.
*  If the broker disconnects, the connection is retried.
*  Routing keys can include values from Freeswitch message headers.
*  The rate of messages pulished can be limited by filtering event types.
*  Messages are sent asynchronously so as not to block the Freeswitch core.
*  Pulishing can be temporarily suspended on the event of "back pressure" from the AMQP broker.



2. How to build and install
---------------------------

Requires librabbitmq1 to build. Debian Jessie comes with the correct version.


3. Configuration
----------------

All configuration is done within the amqp.conf.xml file located in the freeswitch/autoload_configs folder, which is usually in /etc/ for linux based machines.

The file is of the format:

  <configuration name="amqp.conf" description="mod_amqp">
    <settings>
      <param name="parameter1" value="value1"/>
      <param name="parameter2" value="value2"/>
      ...etc.
    </settings>
  </configuration>


Available parameters are as follows:

    +------------------------------+-----------------------------------+
    | name                         | default value             (units) |
    |------------------------------+-----------------------------------|
    | amqpHostnames                | localhost                         |
    | amqpVirtualHost              | /                                 |
    | amqpPort                     | 5672                              |
    | amqpUsername                 | guest                             |
    | amqpPassword                 | guest                             |
    | amqpHeartbeatSeconds         | 0                             (s) |
    | eventExchange                | TAP.Events                        |
    | eventExchangetype            | topic                             |
    | eventRoutingKeyFormat        | %s.%s.%s.%s                       | 
    | eventRoutingKeyFormatFields  | FreeSWITCH-Hostname,Event-Name,   |
    |                              |     Event-Subclass,Unique-ID      |
    | eventFilter                  | SWITCH_EVENT_CHANNEL_CREATE,      |
    |                              |     SWITCH_EVENT_CHANNEL_DESTROY, |
    |                              |     SWITCH_EVENT_HEARTBEAT,       |
    |                              |     SWITCH_EVENT_DTMF             |
    | commandExchange              | TAP.Commands                      |
    | commandExchangeType          | topic                             |
    | commandBindingKey            | TapCommands                       |
    | amqpSendQueueSize            | 500                      (events) |
    | amqpCircuitBreakerTimeout    | 10000                        (ms) |
    | amqpReconnectInterval        | 1000                         (ms) |
    +------------------------------+-----------------------------------+

Set the amqpHostname and amqpPort to point to the AMQP broker, and set valid login credentials using amqpUsername and amqpPassword.

The routing key is made from the eventRoutingKeyFormat format string using the freeswitch event header values specified in the eventRoutingKeyFormatFields. See the manpage printf(1) for more information about format strings. The numer of percent marks in the format string must match the number of comma-separated header names in the format fields string.

mod_amqp has an internal buffer for events so that it can send them asynchronously and also cope with the connection going down for a short amount of time. The size of this buffer is set by amqpSendQueueSize. If this buffer ever becomes full, then mod_amqp will drop event messages for the period of time specified by amqpCircuitBreakerTimeout (in milliseconds).

If the connection to the AMQP broker is severed, mod_amqp will attempt to reconnect regularly according to the amqpReconnectInterval (in milliseconds). It will cycle through the hostnames provided in amqpHostnames.

The eventFilter parameter specifies which events will be sent to the AMQP broker, a full list of available options can be found in src/include/switch_types.h. The special event name SWITCH_EVENT_ALL causes all events to be sent, effectively disabling the filter.


4. Usage
--------

Usually, mod_amqp will be loaded automatically when Freeswitch starts. To establish whether the module has been loaded, you can execute "module_exists mod_amqp" in fs_cli.

If the module is not set to load with Freeswitch, it can be loaded on he fly by executing "load mod_amqp" from fs_cli. You'll see a few lines of status messages as the module loads and tries to connect to the AMQP roker. Conversely, the module can be unloaded using "unload mod_amqp". 

To effect new settings having edited the config file, the module should be unloaded then loaded again. 



5. Trouleshooting
-----------------

Any errors or warnings will be reported using Freeswitch logging, so check for errors using fs_cli with the loglevel is set to be sufficiently verbose, or with your selected logging module; for example, the syslog logger. 

Typically, messages not being received by the AMQP broker is due to network connectivity or failed authentication with the broker.

If mod_amqp experiences back-pressure from the AMQP broker, its internal buffer of events to send fills up. When this buffer is half full, warning messages are logged, and when the queue is completely full the circuit breaker will be triggered, logging an error and dropping events for the predefined amount of time.



6. Notes
--------

The SHA for the revision of librabbitmq-c1 that is included is 1c213703c9fdd747bc71ea4f64943c3b4269f8cf.
 
