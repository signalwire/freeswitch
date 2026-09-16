#!/usr/bin/env python3
"""
Test responder for mod_amqp xml_handler.
Consumes XML fetch requests from FreeSWITCH and replies with a static XML document.
"""

import json
import sys
import textwrap

import pika

EXCHANGE      = "TAP.XML_handler"
EXCHANGE_TYPE = "topic"
BINDING_KEY   = "#"
QUEUE         = "test_amqp"

AMQP_HOST     = "127.0.0.1"
AMQP_USER     = "guest"
AMQP_PASSWORD = "guest"

XML_RESPONSE = textwrap.dedent("""\
    <?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <document type="freeswitch/xml">
      <section name="directory" description="">
        <domain name="default">
          <groups>
            <group name="default" description="">
              <users>
                <user id="1000">
                  <variables>
                    <variable name="asdf" value="true"/>
                  </variables>
                </user>
              </users>
            </group>
          </groups>
        </domain>
      </section>
    </document>
""")


def on_request(ch, method, props, body):
    print(f"\n{'─' * 64}")
    print(f"  routing_key    : {method.routing_key}")
    print(f"  correlation_id : {props.correlation_id}")
    print(f"  reply_to       : {props.reply_to}")

    try:
        print(f"  body:\n{json.dumps(json.loads(body), indent=4)}")
    except (json.JSONDecodeError, TypeError):
        print(f"  body: {body!r}")

    if not props.reply_to:
        print("  [!] no reply_to — skipping reply")
        return

    ch.basic_publish(
        exchange="",
        routing_key=props.reply_to,
        properties=pika.BasicProperties(
            correlation_id=props.correlation_id,
            content_type="text/xml",
        ),
        body=XML_RESPONSE,
    )
    print(f"  [→] replied")


def main():
    connection = pika.BlockingConnection(
        pika.ConnectionParameters(
            host=AMQP_HOST,
            credentials=pika.PlainCredentials(AMQP_USER, AMQP_PASSWORD),
            heartbeat=60,
        )
    )
    channel = connection.channel()

    channel.exchange_declare(exchange=EXCHANGE, exchange_type=EXCHANGE_TYPE, durable=True)
    channel.queue_declare(queue=QUEUE, durable=True)
    channel.queue_bind(exchange=EXCHANGE, queue=QUEUE, routing_key=BINDING_KEY)
    channel.basic_qos(prefetch_count=1)
    channel.basic_consume(queue=QUEUE, on_message_callback=on_request, auto_ack=True)

    print(f"Listening  exchange={EXCHANGE!r}  queue={QUEUE!r}  key={BINDING_KEY!r}")
    print("Press CTRL+C to stop\n")

    try:
        channel.start_consuming()
    except KeyboardInterrupt:
        channel.stop_consuming()

    connection.close()


if __name__ == "__main__":
    try:
        main()
    except pika.exceptions.AMQPConnectionError as e:
        print(f"Connection failed: {e}", file=sys.stderr)
        sys.exit(1)
