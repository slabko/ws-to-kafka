# Websocket to Kafka

A simple app that subscribes to a WebSocket and sends all events to a Kafka topic.
It maintains the connection to the WebSocket and restarts it when needed.

__Note__: This is a work in progress.

# Setup

Prepare the dependencies

```
sh bootstrap.sh
```

Build
```
mkdir build && cd build && cmake .. && make

# create config.json given below and a topic in your Kafka broker, then run:

./main

```


# Configuration

It expects a configuration file, `config.json` in the working directory. Here is an example:
```
{
  "url": "wss://ws.bitvavo.com/v2/",
  "brokers": "localhost:9092",
  "topic": "websocket_1",
  "message": {
    "action": "subscribe",
    "channels": [
      {
        "name": "trades",
        "markets": ["BTC-EUR", "ETH-EUR"]
      }
    ]
  }
}
```
