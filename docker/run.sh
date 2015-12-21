#!/bin/bash
echo -ne "\033]0;$BROKER_NAME\007"
if [ -z "$DEALER_PORT" ] 
then 
  ddbroker -r $BROKER_PORT -k /keys/broker-keys.json -s $BROKER_SCOPE
else 
  ddbroker -d $DEALER_PORT -r $BROKER_PORT -k /keys/broker-keys.json -s $BROKER_SCOPE
fi
